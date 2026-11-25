//! Bundled quick-install catalogue (single-folder addons served by
//! `constants::ADDON_DOWNLOAD_ENDPOINT?file=…`).
//!
//! Descriptions are the French source strings; the UI translates them with
//! `Shared::t` (they are in translations/en.json).

use crate::constants::ADDON_DOWNLOAD_ENDPOINT;

#[derive(Debug, Clone, Copy)]
pub struct CatalogEntry {
    pub name: &'static str,
    /// French source text (translation key).
    pub description: &'static str,
    /// `file` parameter of the download endpoint.
    pub file: &'static str,
    /// Folder dropped into Interface/AddOns (also the registry id).
    pub folder: &'static str,
}

const fn e(name: &'static str, description: &'static str, file: &'static str, folder: &'static str) -> CatalogEntry {
    CatalogEntry { name, description, file, folder }
}

pub const CATALOG: &[CatalogEntry] = &[
    e("ArkInventory", "Gestion d'inventaire avancée avec catégories automatiques.", "ArkInventory.zip", "ArkInventory"),
    e("AtlasLoot", "Base de données complète des butins de donjons et raids.", "AtlasLoot.zip", "AtlasLoot"),
    e("Auctionator", "Améliore l'interface de l'Hôtel des Ventes.", "Auctionator.zip", "Auctionator"),
    e("Bagnon", "Regroupe tous vos sacs en une seule fenêtre épurée.", "Bagnon.zip", "Bagnon"),
    e("Bartender4", "Personnalisation totale de vos barres d'actions.", "Bartender4.zip", "Bartender4"),
    e("Deadly Boss Mods (DBM)", "Alertes et chronomètres indispensables pour les raids.", "DBM.zip", "DBM-Core"),
    e("GatherMate2", "Mémorise l'emplacement des plantes et minerais sur la carte.", "GatherMate2.zip", "GatherMate2"),
    e("GearScore", "Évalue le niveau d'équipement global des joueurs.", "GearScore.zip", "GearScore"),
    e("HandyNotes", "Ajoute des notes personnalisées et des points d'intérêt.", "HandyNotes.zip", "HandyNotes"),
    e("Healium", "Interface de soin avec boutons d'accès rapide aux sorts.", "Healium.zip", "Healium"),
    e("Immersion", "Modernise l'affichage des textes de quêtes et dialogues.", "Immersion.zip", "Immersion"),
    e("Mapster", "Améliore la visibilité et les fonctions de la carte du monde.", "Mapster.zip", "Mapster"),
    e("NPCScan", "Détecte les créatures rares à proximité et vous alerte avec un signal sonore.", "NPCScan.zip", "_NPCScan"),
    e(
        "NPCScan-Overlay",
        "Affiche les trajets et zones d'apparition des créatures rares sur la carte.",
        "NPCScan-Overlay.zip",
        "_NPCScan-Overlay",
    ),
    e("Postal", "Améliore radicalement la gestion de la boîte aux lettres.", "Postal.zip", "Postal"),
    e("Prat", "Personnalisation poussée de la fenêtre de discussion.", "Prat-3.0.zip", "Prat-3.0"),
    e("Quartz", "Barres d'incantation modulaires et ultra-précises.", "Quartz.zip", "Quartz"),
    e("QuestHelper", "Le guide classique pour optimiser vos itinéraires de quêtes.", "QuestHelper.zip", "QuestHelper"),
    e("Scrap", "Vente et réparation automatique chez les marchands.", "Scrap.zip", "Scrap"),
    e("SexyMap", "Donne un look moderne et personnalisable à votre mini-carte.", "SexyMap.zip", "SexyMap"),
    e("TomTom", "Navigation par coordonnées GPS et flèche directionnelle.", "TomTom.zip", "TomTom"),
    e("WIM", "Gère vos chuchotements dans des fenêtres séparées.", "WIM.zip", "WIM"),
    e("WeakAuras", "L'outil ultime pour créer vos propres alertes visuelles.", "WeakAuras.zip", "WeakAuras2"),
    e("XPerl", "Refonte visuelle des portraits de joueurs (Style Classique).", "XPerl.zip", "XPerl"),
    e("Details!", "Le compteur de combat le plus précis et détaillé.", "details.zip", "Details"),
    e("ElvUI", "Une interface utilisateur complète, moderne et minimaliste.", "elvui.zip", "ElvUI"),
    e("Shadowed Unit Frames", "Portraits de joueurs épurés et hautement configurables.", "shadowedunitframes.zip", "ShadowedUnitFrames"),
    e("TotalRP3", "L'addon de référence pour l'immersion et le jeu de rôle.", "totalRP3.zip", "TotalRP3"),
    e("VuhDo", "Interface de raid et de soin extrêmement personnalisable.", "vuhdo.zip", "VuhDo"),
];

/// `ADDON_DOWNLOAD_ENDPOINT?file=<file>` (query value percent-encoded).
pub fn download_url(file: &str) -> String {
    match reqwest::Url::parse_with_params(ADDON_DOWNLOAD_ENDPOINT, &[("file", file)]) {
        Ok(u) => u.to_string(),
        Err(_) => format!("{ADDON_DOWNLOAD_ENDPOINT}?file={file}"),
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::core::addons::install::is_trusted_url;

    #[test]
    fn catalogue_urls_are_trusted() {
        assert_eq!(CATALOG.len(), 29);
        for entry in CATALOG {
            let url = download_url(entry.file);
            assert!(url.starts_with(ADDON_DOWNLOAD_ENDPOINT), "{url}");
            assert!(url.ends_with(&format!("?file={}", entry.file)), "{url}");
            assert!(is_trusted_url(&url), "{url}");
        }
    }

    #[test]
    fn catalogue_descriptions_are_translated() {
        let i18n = crate::i18n::I18n::new("en");
        for entry in CATALOG {
            assert_ne!(i18n.t(entry.description), entry.description, "{}", entry.name);
        }
    }
}
