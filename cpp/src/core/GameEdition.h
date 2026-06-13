#pragma once

#include <QString>
#include <QStringList>

/**
 * @brief Describes one supported game edition ("2 launchers en 1").
 *
 * Everything that differs between the WotLK 3.3.5a client and the Legion 7.3.5
 * client lives here: client detection, download source + integrity data,
 * realmlist handling, addon manifest source and theme tokens. The launcher
 * never special-cases "legion" outside of this descriptor — code asks the
 * active GameEdition instead.
 *
 * Note: the *client install path* is per-user data and therefore lives in
 * ConfigManager (one config object per edition), not in this static
 * descriptor.
 */
struct GameEdition {
    QString id;                    // "wotlk" | "legion" (stable, persisted)
    QString displayName;           // "WoW 3.3.5a" | "WoW Legion 7.3.5"
    QString windowTitle;           // full main-window title
    QStringList clientExeCandidates; // names checked under the client path; first = canonical

    // Realmlist / login redirection.
    // wotlk: a full "set realmlist <host>" line written to realmlist.wtf.
    // legion: the portal host written as SET portal "<host>" in WTF/Config.wtf.
    QString defaultRealmName;
    QString defaultRealmlist;

    // Client download + integrity (verified BEFORE extraction).
    QString clientDownloadUrl;
    qint64  clientExpectedSize = 0;   // bytes; 0 = unknown
    QString clientExpectedSha256;     // lowercase hex; empty = unknown
    bool    requireHashBeforeExtract = false; // true -> extraction refused while hash unknown/mismatching
    QString archiveFormat;            // "zip" | "tar.gz"
    // Optional embedded per-chunk SHA-256 manifest (resource path). When set,
    // the downloader verifies every segment against its chunk hash and
    // re-fetches only the corrupted chunk — so a rare corruption over a very
    // long download is repaired on the fly instead of discarding everything.
    QString chunkManifestResource;

    // Addons (the two manifests are never mixed).
    QString addonManifestUrl;
    QString embeddedManifestResource; // offline fallback, e.g. ":/addons/manifest"
    // Only addon archives served by this host may be downloaded (anti-tamper:
    // a swapped manifest can't point installs at an arbitrary domain).
    QString addonDownloadHost;

    bool supportsHdPatch = false;     // gates HdPatchManager + HD UI
    bool supportsEnUsPack = false;    // gates the 3.3.5 enUS language-pack button

    // Theme tokens (assets are relative to the Asset directory).
    QString logoAsset;
    QString taskbarIconAsset;         // .ico — window + taskbar icon
    QString accentColor;
    QString accentDark;

    // Background pool: each edition has its own set of background images, never
    // shared. backgroundDir is relative to the Asset directory; backgrounds are
    // the base file names (without extension). backgroundPaletteKey, when set,
    // forces a single palette for every background of the edition (Legion is
    // always fel-green); when empty the palette is chosen per background name
    // (WotLK's classic per-theme colours).
    QString backgroundDir;
    QStringList backgrounds;
    QString backgroundPaletteKey;
    QString defaultBackground;

    static const GameEdition& wotlk();
    static const GameEdition& legion();

    // Strictly validated lookup: any unknown/garbage id falls back to wotlk,
    // so an id read from config or the command line can never be injected
    // into a path or break the UI.
    static const GameEdition& byId(const QString& id);
    static bool isKnownId(const QString& id);
    static QStringList knownIds();
};
