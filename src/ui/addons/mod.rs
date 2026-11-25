//! Addon manager window (Qt `AddonsWindow`): three tabs — recommended addons
//! from the manifest (install / update / uninstall), the bundled quick-install
//! catalogue, and the folders present in Interface/AddOns (multi-select
//! delete, registry-aware for multi-folder addons).
//!
//! Network and heavy disk work (manifest fetch, download, extraction, folder
//! sizes) run on worker threads; the window polls their channels each frame.
//! Rendering lives in [`view`].

mod view;

use crate::app::{Shared, WindowKind, window_id};
use crate::constants::{CLIENT_DISPLAY_NAME, CLIENT_EXE_NAME};
use crate::core::addons::install::{self, InstallError, InstallEvent, InstallJob, InstalledEntry, Staging};
use crate::core::addons::manifest::{self, ManifestLoad};
use crate::core::addons::registry::{AddonRegistry, InstalledAddon};
use crate::core::addons::{self as core_addons, InstallState, InstallTarget, catalog};
use crate::util::msg;
use eframe::egui;
use std::collections::BTreeSet;
use std::path::{Path, PathBuf};
use std::sync::mpsc::{self, TryRecvError};
use std::time::Duration;

const WINDOW_SIZE: [f32; 2] = [640.0, 720.0];
const WINDOW_MIN_SIZE: [f32; 2] = [600.0, 640.0];
const POLL_INTERVAL: Duration = Duration::from_millis(100);

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum Tab {
    Recommended,
    Catalog,
    Installed,
}

/// Colour of a card's status line.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum StatusKind {
    Info,
    Success,
    Error,
    Muted,
}

/// Status line of a card: French source text (+ optional `%1` percentage),
/// translated at render time so a language switch applies live.
#[derive(Debug, Clone, Copy)]
struct Status {
    text: &'static str,
    pct: Option<u8>,
    kind: StatusKind,
}

/// Progress bar of a card. `None` fraction = indeterminate.
#[derive(Debug, Clone, Copy)]
struct Progress(Option<f32>);

struct Card {
    target: InstallTarget,
    /// French source text (translated at render time).
    description: String,
    state: InstallState,
    status: Option<Status>,
    progress: Option<Progress>,
}

impl Card {
    fn new(target: InstallTarget, description: String) -> Self {
        Card { target, description, state: InstallState::NotInstalled, status: None, progress: None }
    }
}

/// Which card list a card belongs to.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum CardRef {
    Recommended(usize),
    Catalog(usize),
}

struct ActiveInstall {
    card: CardRef,
    job: InstallJob,
}

/// "Recommandés" status line.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum ManifestStatus {
    Loading,
    Empty,
    Offline,
    Ready,
}

#[derive(Default)]
struct InstalledView {
    rows: Vec<InstalledEntry>,
    /// No AddOns directory (or no WoW path).
    no_dir: bool,
    selected: BTreeSet<usize>,
    anchor: Option<usize>,
    scan: Option<mpsc::Receiver<Option<Vec<InstalledEntry>>>>,
}

/// Something the user clicked during the frame, executed after rendering
/// (message boxes are modal and must not run inside the egui pass).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum Action {
    SelectTab(Tab),
    Install(CardRef),
    Uninstall(CardRef),
    RefreshInstalled,
    DeleteSelected,
    Close,
}

pub struct AddonsWindow {
    registry: AddonRegistry,
    tab: Tab,
    manifest_rx: Option<mpsc::Receiver<ManifestLoad>>,
    manifest_status: ManifestStatus,
    recommended: Vec<Card>,
    catalog: Vec<Card>,
    active: Option<ActiveInstall>,
    installed: InstalledView,
}

impl AddonsWindow {
    pub fn new(shared: &Shared) -> Self {
        let catalog = catalog::CATALOG
            .iter()
            .map(|e| Card::new(InstallTarget::from_catalog(e), e.description.to_string()))
            .collect();
        let mut w = AddonsWindow {
            registry: AddonRegistry::new(),
            tab: Tab::Recommended,
            manifest_rx: Some(manifest::spawn_fetch()),
            manifest_status: ManifestStatus::Loading,
            recommended: Vec::new(),
            catalog,
            active: None,
            installed: InstalledView::default(),
        };
        w.refresh_cards(shared);
        w
    }

    /// Renders the window (via `ui::child_window`). Returns false once closed.
    pub fn show(&mut self, ctx: &egui::Context, shared: &mut Shared) -> bool {
        self.poll(shared);
        if self.is_busy() {
            ctx.request_repaint_after(POLL_INTERVAL);
        }

        let title = shared.tf("Gestionnaire d'Addons - %1", &[&CLIENT_DISPLAY_NAME]);
        let mut actions = Vec::new();
        let open = {
            let shared: &Shared = shared;
            crate::ui::child_window(ctx, window_id(WindowKind::Addons), &title, WINDOW_SIZE, WINDOW_MIN_SIZE, |ui| {
                self.render(ui, shared, &mut actions)
            })
        };

        for action in actions {
            match action {
                Action::Close => return false,
                Action::SelectTab(tab) => {
                    if tab != self.tab {
                        self.tab = tab;
                        if tab == Tab::Installed {
                            self.start_scan(shared);
                        }
                    }
                }
                Action::Install(card) => self.start_install(shared, card),
                Action::Uninstall(card) => self.uninstall(shared, card),
                Action::RefreshInstalled => self.start_scan(shared),
                Action::DeleteSelected => self.delete_selected(shared),
            }
        }
        open
    }

    fn is_busy(&self) -> bool {
        self.manifest_rx.is_some() || self.active.is_some() || self.installed.scan.is_some()
    }

    fn card(&self, r: CardRef) -> Option<&Card> {
        match r {
            CardRef::Recommended(i) => self.recommended.get(i),
            CardRef::Catalog(i) => self.catalog.get(i),
        }
    }

    fn card_mut(&mut self, r: CardRef) -> Option<&mut Card> {
        match r {
            CardRef::Recommended(i) => self.recommended.get_mut(i),
            CardRef::Catalog(i) => self.catalog.get_mut(i),
        }
    }

    // --- Worker results ---------------------------------------------------------

    fn poll(&mut self, shared: &Shared) {
        // Manifest.
        if let Some(rx) = &self.manifest_rx {
            let load = match rx.try_recv() {
                Ok(load) => Some(load),
                Err(TryRecvError::Empty) => None,
                Err(TryRecvError::Disconnected) => {
                    Some(ManifestLoad { addons: manifest::embedded(), from_remote: false })
                }
            };
            if let Some(load) = load {
                self.manifest_rx = None;
                self.on_manifest_loaded(shared, load);
            }
        }

        // Install in progress.
        if let Some(active) = &self.active {
            let card = active.card;
            let mut finished = None;
            let mut updates = Vec::new();
            loop {
                match active.job.rx.try_recv() {
                    Ok(InstallEvent::Finished(res)) => {
                        finished = Some(res);
                        break;
                    }
                    Ok(ev) => updates.push(ev),
                    Err(TryRecvError::Empty) => break,
                    Err(TryRecvError::Disconnected) => {
                        finished = Some(Err(InstallError::Cancelled));
                        break;
                    }
                }
            }
            if let Some(c) = self.card_mut(card) {
                for ev in updates {
                    match ev {
                        InstallEvent::Downloading(pct) => {
                            c.progress = Some(Progress(pct.map(|p| f32::from(p) / 100.0)));
                            c.status = Some(match pct {
                                Some(p) => Status { text: "Téléchargement... %1%", pct: Some(p), kind: StatusKind::Info },
                                None => Status { text: "Téléchargement...", pct: None, kind: StatusKind::Info },
                            });
                        }
                        InstallEvent::Extracting(f) => {
                            c.progress = Some(Progress(Some(f)));
                            c.status = Some(Status { text: "Extraction en cours...", pct: None, kind: StatusKind::Info });
                        }
                        InstallEvent::Finished(_) => {}
                    }
                }
            }
            if let Some(res) = finished {
                self.finish_install(shared, card, res);
            }
        }

        // "Installés" scan.
        if let Some(rx) = &self.installed.scan {
            let result = match rx.try_recv() {
                Ok(r) => Some(r),
                Err(TryRecvError::Empty) => None,
                Err(TryRecvError::Disconnected) => Some(Some(Vec::new())),
            };
            if let Some(result) = result {
                let v = &mut self.installed;
                v.scan = None;
                v.no_dir = result.is_none();
                v.rows = result.unwrap_or_default();
                v.selected.clear();
                v.anchor = None;
            }
        }
    }

    fn on_manifest_loaded(&mut self, shared: &Shared, load: ManifestLoad) {
        self.recommended = load
            .addons
            .iter()
            .map(|a| Card::new(InstallTarget::from_manifest(a), a.description.clone()))
            .collect();
        self.manifest_status = if self.recommended.is_empty() {
            ManifestStatus::Empty
        } else if !load.from_remote {
            ManifestStatus::Offline
        } else {
            ManifestStatus::Ready
        };
        self.refresh_cards(shared);
    }

    // --- Card state -----------------------------------------------------------------

    fn refresh_cards(&mut self, shared: &Shared) {
        let dir = addons_dir(shared);
        let registry = &self.registry;
        for card in self.recommended.iter_mut().chain(self.catalog.iter_mut()) {
            card.state = core_addons::install_state(&card.target, registry, dir.as_deref());
        }
    }

    // --- Install / uninstall ------------------------------------------------------

    /// Checks the WoW path and creates Interface/AddOns (Qt `ensureWowReady`).
    fn ensure_wow_ready(shared: &Shared) -> Option<PathBuf> {
        let wow = shared.config.wow_path();
        if wow.is_empty() || !Path::new(&wow).join(CLIENT_EXE_NAME).exists() {
            msg::warning(
                &shared.t("Erreur"),
                &shared.t("Veuillez d'abord configurer le chemin d'installation de World of Warcraft dans les réglages."),
            );
            return None;
        }
        let dir = core_addons::addons_dir(&wow);
        if !dir.is_dir() && std::fs::create_dir_all(&dir).is_err() {
            msg::warning(
                &shared.t("Erreur"),
                &shared.t("Impossible de créer le dossier Interface/AddOns (droits insuffisants ?)."),
            );
            return None;
        }
        Some(dir)
    }

    fn warn_install_running(shared: &Shared) {
        msg::info(&shared.t("Information"), &shared.t("Une installation est déjà en cours. Veuillez patienter."));
    }

    fn start_install(&mut self, shared: &Shared, card_ref: CardRef) {
        let Some(addons_dir) = Self::ensure_wow_ready(shared) else { return };
        if self.active.is_some() {
            Self::warn_install_running(shared);
            return;
        }
        let Some(target) = self.card(card_ref).map(|c| c.target.clone()) else { return };

        // Only download from the trusted addon host.
        if !install::is_trusted_url(&target.url) {
            msg::warning(&shared.t("Erreur"), &shared.t("URL de téléchargement de l'addon invalide."));
            return;
        }
        let staging = match Staging::create(&target.id) {
            Ok(s) => s,
            Err(e) => {
                log::error!("Dossier temporaire d'addon impossible : {e}");
                msg::warning(&shared.t("Erreur"), &shared.t("Impossible de créer le fichier temporaire."));
                return;
            }
        };

        if let Some(c) = self.card_mut(card_ref) {
            c.progress = Some(Progress(Some(0.0)));
            c.status = Some(Status { text: "Téléchargement...", pct: None, kind: StatusKind::Info });
        }
        let job = install::spawn_install(target.url, target.folders, staging, addons_dir);
        self.active = Some(ActiveInstall { card: card_ref, job });
    }

    fn finish_install(&mut self, shared: &Shared, card_ref: CardRef, result: Result<Vec<String>, InstallError>) {
        self.active = None;
        let status = match result {
            Ok(deployed) => {
                let record = self.card(card_ref).map(|c| InstalledAddon {
                    id: c.target.id.clone(),
                    name: c.target.name.clone(),
                    version: c.target.version.clone(),
                    folders: deployed,
                    source: c.target.source.as_str().to_string(),
                });
                if let Some(record) = record
                    && !self.registry.record(&record)
                {
                    log::warn!("Registre des addons non enregistré pour {}", record.id);
                }
                Status { text: "Installé avec succès !", pct: None, kind: StatusKind::Success }
            }
            Err(e) => Status { text: install_error_text(e), pct: None, kind: StatusKind::Error },
        };
        if let Some(c) = self.card_mut(card_ref) {
            c.progress = None;
            c.status = Some(status);
        }
        self.refresh_cards(shared);
        self.start_scan(shared);
    }

    /// Registry-aware uninstall of a recommended card (all recorded folders).
    fn uninstall(&mut self, shared: &Shared, card_ref: CardRef) {
        if self.active.is_some() {
            Self::warn_install_running(shared);
            return;
        }
        let Some(dir) = addons_dir(shared) else { return };
        let Some(card) = self.card(card_ref) else { return };
        let id = card.target.id.clone();
        let name = card.target.name.clone();

        // Prefer the registry's recorded folders (multi-folder addons); fall
        // back to the declared folders for manual installs.
        let mut folders =
            if self.registry.contains(&id) { self.registry.folders(&id) } else { card.target.folders.clone() };
        folders.retain(|f| !f.is_empty());
        if folders.is_empty() {
            return;
        }

        if !msg::confirm(
            &shared.t("Confirmer la désinstallation"),
            &shared.tf("Désinstaller %1 ?\nDossiers supprimés : %2", &[&name, &folders.join(", ")]),
        ) {
            return;
        }

        let failures = install::remove_folders(&dir, &folders);
        self.registry.remove(&id);

        if let Some(c) = self.card_mut(card_ref) {
            c.status = Some(if failures.is_empty() {
                Status { text: "Désinstallé.", pct: None, kind: StatusKind::Muted }
            } else {
                Status { text: "Désinstallation partielle.", pct: None, kind: StatusKind::Error }
            });
        }
        self.refresh_cards(shared);
        self.start_scan(shared);

        if !failures.is_empty() {
            warn_partial(shared, &failures);
        }
    }

    // --- "Installés" tab ------------------------------------------------------------

    fn start_scan(&mut self, shared: &Shared) {
        self.registry.reload();
        match addons_dir(shared) {
            Some(dir) => self.installed.scan = Some(install::spawn_scan(dir, self.registry.all())),
            None => {
                let v = &mut self.installed;
                v.scan = None;
                v.no_dir = true;
                v.rows.clear();
                v.selected.clear();
                v.anchor = None;
            }
        }
    }

    fn delete_selected(&mut self, shared: &Shared) {
        // Not in the Qt version: deleting while an install deploys its folders
        // could remove half-moved data, so wait for the install to finish.
        if self.active.is_some() {
            Self::warn_install_running(shared);
            return;
        }
        let rows: Vec<InstalledEntry> =
            self.installed.selected.iter().filter_map(|&i| self.installed.rows.get(i).cloned()).collect();
        if rows.is_empty() {
            msg::info(&shared.t("Suppression d'addons"), &shared.t("Veuillez sélectionner au moins un addon à supprimer."));
            return;
        }
        let Some(dir) = addons_dir(shared) else { return };

        let names: Vec<&str> = rows.iter().map(|r| r.name.as_str()).collect();
        if !msg::confirm(
            &shared.t("Confirmer la suppression"),
            &shared.tf("Supprimer définitivement %1 addon(s) ?\n\n%2", &[&rows.len(), &names.join("\n")]),
        ) {
            return;
        }

        let mut failures = Vec::new();
        for row in &rows {
            failures.extend(install::remove_folders(&dir, &row.folders));
            if !row.registry_id.is_empty() {
                self.registry.remove(&row.registry_id);
            }
        }

        self.start_scan(shared);
        self.refresh_cards(shared);

        if failures.is_empty() {
            msg::info(&shared.t("Suppression d'addons"), &shared.t("Addon(s) supprimé(s) avec succès."));
        } else {
            warn_partial(shared, &failures);
        }
    }
}

impl Drop for AddonsWindow {
    fn drop(&mut self) {
        // The worker removes its staging directory once it notices.
        if let Some(active) = &self.active {
            active.job.cancel();
        }
    }
}

/// `<wow>/Interface/AddOns`, or `None` when no WoW path is configured (never
/// resolve folder names against the working directory).
fn addons_dir(shared: &Shared) -> Option<PathBuf> {
    let wow = shared.config.wow_path();
    if wow.trim().is_empty() { None } else { Some(core_addons::addons_dir(&wow)) }
}

fn warn_partial(shared: &Shared, failures: &[String]) {
    msg::warning(
        &shared.t("Suppression partielle"),
        &shared.tf(
            "Certains dossiers n'ont pas pu être supprimés (droits insuffisants ou fichiers verrouillés) :\n%1",
            &[&failures.join("\n")],
        ),
    );
}

fn install_error_text(e: InstallError) -> &'static str {
    match e {
        InstallError::Network => "Erreur réseau lors du téléchargement de l'addon.",
        InstallError::NotZip => "Le fichier téléchargé n'est pas une archive ZIP valide.",
        InstallError::Extract => "Erreur lors de l'extraction de l'archive.",
        InstallError::NoFolders => "Aucun dossier d'addon n'a été trouvé dans l'archive.",
        InstallError::Cancelled => "Erreur interne.",
    }
}

/// Qt `formatSize`: one decimal under 10, none above; translated units.
fn format_size(shared: &Shared, bytes: u64) -> String {
    let units = ["o", "Ko", "Mo", "Go"];
    let mut size = bytes as f64;
    let mut unit = 0;
    while size >= 1024.0 && unit < 3 {
        size /= 1024.0;
        unit += 1;
    }
    let decimals = if size < 10.0 { 1 } else { 0 };
    format!("{size:.decimals$} {}", shared.t(units[unit]))
}
