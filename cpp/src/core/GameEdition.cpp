#include "GameEdition.h"
#include "Constants.h"

const GameEdition& GameEdition::wotlk() {
    static const GameEdition e = [] {
        GameEdition ed;
        ed.id = QStringLiteral("wotlk");
        ed.displayName = QStringLiteral("WoW 3.3.5a");
        ed.windowTitle = QStringLiteral("Sylvania Launcher - World of Warcraft 3.3.5");
        ed.clientExeCandidates = {QStringLiteral("Wow.exe")};
        ed.defaultRealmName = QStringLiteral("Sylvania");
        ed.defaultRealmlist = QStringLiteral("set realmlist sylvania-servergame.com");
        ed.clientDownloadUrl = QString::fromUtf8(SylvaniaConstants::kWotlkClientUrl);
        ed.clientExpectedSize = SylvaniaConstants::kWotlkClientExpectedSize;
        ed.clientExpectedSha256 = QString::fromUtf8(SylvaniaConstants::kWotlkClientSha256);
        // Historical endpoint publishes no hash yet; once one is added to
        // Constants.h the download becomes hash-enforced automatically.
        ed.requireHashBeforeExtract = false;
        ed.archiveFormat = QStringLiteral("zip");
        ed.addonManifestUrl = QString::fromUtf8(SylvaniaConstants::kAddonManifestUrl);
        ed.embeddedManifestResource = QStringLiteral(":/addons/manifest");
        ed.supportsHdPatch = true;
        ed.supportsEnUsPack = true;
        ed.logoAsset = QStringLiteral("sylvania_logo.png");
        ed.taskbarIconAsset = QStringLiteral("LogoSylvania/LogoSylvania256.ico");
        ed.backgroundAsset.clear();  // dynamic backgrounds (random/per-user)
        ed.accentColor = QStringLiteral("#d4af37");   // WoW gold
        ed.accentDark = QStringLiteral("#8a7a2d");
        return ed;
    }();
    return e;
}

const GameEdition& GameEdition::legion() {
    static const GameEdition e = [] {
        GameEdition ed;
        ed.id = QStringLiteral("legion");
        ed.displayName = QStringLiteral("WoW Legion 7.3.5");
        ed.windowTitle = QStringLiteral("Sylvania Launcher - World of Warcraft Legion 7.3.5");
        // Exact exe name still to be confirmed (§9); both common names are probed.
        ed.clientExeCandidates = {QStringLiteral("Wow.exe"), QStringLiteral("Wow-64.exe")};
        ed.defaultRealmName = QStringLiteral("Sylvania Legion");
        ed.defaultRealmlist = QString::fromUtf8(SylvaniaConstants::kLegionDefaultPortal);
        ed.clientDownloadUrl = QString::fromUtf8(SylvaniaConstants::kLegionClientUrl);
        ed.clientExpectedSize = SylvaniaConstants::kLegionClientExpectedSize;
        ed.clientExpectedSha256 = QString::fromUtf8(SylvaniaConstants::kLegionClientSha256);
        // Mandatory: while the expected hash is not provided in Constants.h the
        // archive is downloaded but NEVER extracted (no silent degradation).
        ed.requireHashBeforeExtract = true;
        ed.archiveFormat = QStringLiteral("tar.gz");
        ed.addonManifestUrl = QString::fromUtf8(SylvaniaConstants::kLegionAddonManifestUrl);
        ed.embeddedManifestResource = QStringLiteral(":/addons/manifest_legion");
        ed.supportsHdPatch = false;
        ed.logoAsset = QStringLiteral("Legion/legion_logo.png");
        ed.taskbarIconAsset = QStringLiteral("Legion/LegionSylvania256.ico");
        ed.backgroundAsset = QStringLiteral("Legion/Background/Legion.png");
        ed.accentColor = QStringLiteral("#76e84c");   // fel green
        ed.accentDark = QStringLiteral("#2f7a1e");
        return ed;
    }();
    return e;
}

const GameEdition& GameEdition::byId(const QString& id) {
    if (id == legion().id) return legion();
    return wotlk();
}

bool GameEdition::isKnownId(const QString& id) {
    return id == wotlk().id || id == legion().id;
}

QStringList GameEdition::knownIds() {
    return {wotlk().id, legion().id};
}
