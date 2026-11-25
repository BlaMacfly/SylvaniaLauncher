#include "ConfigManager.h"
#include "PathUtils.h"
#include "Constants.h"

#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDir>
#include <QTextStream>
#include <QRegularExpression>
#include <QDebug>

ConfigManager::ConfigManager(QObject* parent)
    : QObject(parent)
{
    m_configDir = PathUtils::getConfigDir();
    loadConfig();

    // Flatten a v3.0 per-edition config (and drop its Legion part). Must run
    // before any accessor is used.
    bool dirty = migrateFromEditionConfig();

    // Migrate any stale realmlist inherited from an older launcher build
    // (e.g. logon.sylvania-wow.com). Done before wiring up the debounce
    // timer so the corrected config is written synchronously and the
    // realmlist.wtf on disk is refreshed immediately.
    if (migrateLegacyRealmlist()) {
        dirty = true;
        const auto entries = getRealmlistEntries();
        const int activeIndex = getActiveRealmlistIndex();
        if (activeIndex >= 0 && activeIndex < static_cast<int>(entries.size())) {
            updateRealmlist(entries[activeIndex].address);
        }
    }

    if (dirty) {
        save();
    }

    // H4: coalesce bursts of setX() calls (e.g. Settings dialog applying many
    // options at once) into a single disk write 200 ms after the last change.
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(SylvaniaConstants::kConfigSaveDebounceMs);
    connect(&m_saveTimer, &QTimer::timeout, this, [this]() { save(); });
}

QJsonArray ConfigManager::defaultRealmlistArray() {
    QJsonArray array;
    array.append(QJsonObject{
        {"name", QString::fromUtf8(SylvaniaConstants::kDefaultRealmName)},
        {"address", QString::fromUtf8(SylvaniaConstants::kDefaultRealmlist)},
        {"active", true}
    });
    return array;
}

bool ConfigManager::migrateFromEditionConfig() {
    bool migrated = false;

    // v3.0 nested the client settings per edition. Lift the WotLK ones back
    // to the top level (without overwriting a top-level value) and drop the
    // rest: Legion is no longer supported.
    if (m_config.contains("editions")) {
        const QJsonObject wotlk = m_config.value("editions").toObject()
                                      .value("wotlk").toObject();
        for (const QString& key : {QStringLiteral("wow_path"),
                                   QStringLiteral("realmlist_entries"),
                                   QStringLiteral("active_realmlist_index"),
                                   QStringLiteral("background")}) {
            if (wotlk.contains(key) && !m_config.contains(key)) {
                m_config[key] = wotlk.value(key);
            }
        }
        m_config.remove("editions");
        migrated = true;
        qDebug() << "Config v3.0 par édition aplatie (Legion retiré)";
    }
    if (m_config.contains("active_edition")) {
        m_config.remove("active_edition");
        migrated = true;
    }

    // Make sure there is always a usable realmlist.
    if (!m_config.value("realmlist_entries").isArray() ||
        m_config.value("realmlist_entries").toArray().isEmpty()) {
        m_config["realmlist_entries"] = defaultRealmlistArray();
        m_config["active_realmlist_index"] = 0;
        migrated = true;
    }
    if (!m_config.contains("active_realmlist_index")) {
        m_config["active_realmlist_index"] = 0;
        migrated = true;
    }

    return migrated;
}

bool ConfigManager::migrateLegacyRealmlist() {
    // Domains used by previous launcher builds that must be rewritten to
    // the current production address. The match is case-insensitive and
    // substring-based so any historical spelling is caught.
    static const QStringList kLegacyHosts = {
        QStringLiteral("logon.sylvania-wow.com"),
        QStringLiteral("sylvania-wow.com"),
        QStringLiteral("logon.sylvania.com"),
    };
    const QString canonicalAddress = QString::fromUtf8(SylvaniaConstants::kDefaultRealmlist);

    QJsonArray array = m_config.value("realmlist_entries").toArray();
    bool migrated = false;

    for (int i = 0; i < array.size(); ++i) {
        QJsonObject obj = array[i].toObject();
        const QString address = obj.value("address").toString();
        bool isLegacy = false;
        for (const QString& legacy : kLegacyHosts) {
            if (address.contains(legacy, Qt::CaseInsensitive)) {
                isLegacy = true;
                break;
            }
        }
        if (!isLegacy) continue;

        obj["address"] = canonicalAddress;
        array[i] = obj;
        migrated = true;
        qWarning() << "Realmlist legacy migré:" << address << "->" << canonicalAddress;
    }

    if (migrated) {
        m_config["realmlist_entries"] = array;
    }
    return migrated;
}

ConfigManager::~ConfigManager() {
    // Flush any pending write before tear-down so we never lose the last edit.
    if (m_saveTimer.isActive()) {
        m_saveTimer.stop();
        save();
    }
}

void ConfigManager::scheduleSave() {
    m_saveTimer.start();  // restart restarts the timer if already running
}

void ConfigManager::loadConfig() {
    QString configPath = getConfigPath();
    QFile file(configPath);

    if (file.exists() && file.open(QIODevice::ReadOnly)) {
        QByteArray data = file.readAll();
        file.close();

        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(data, &error);

        if (error.error == QJsonParseError::NoError && doc.isObject()) {
            m_config = doc.object();
            qDebug() << "Configuration chargée depuis:" << configPath;
            return;
        } else {
            qWarning() << "Erreur de parsing JSON:" << error.errorString();
        }
    }

    // Create default config if not found or invalid
    createDefaultConfig();
}

void ConfigManager::createDefaultConfig() {
    m_config = QJsonObject{
        {"sound_enabled", true},
        {"download_method", "direct"},
        {"max_upload_rate", 100},
        {"max_download_rate", 0},
        {"first_run", true},
        {"auto_update", true},
        {"language", "fr"},
        {"wow_path", QString()},
        {"realmlist_entries", defaultRealmlistArray()},
        {"active_realmlist_index", 0}
    };

    save();
    qDebug() << "Configuration par défaut créée";
}

QString ConfigManager::getConfigPath() const {
    return m_configDir + "/config.json";
}

bool ConfigManager::save() {
    QDir dir(m_configDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    // C3: atomic write — QSaveFile writes to a temp file and renames on commit(),
    // preventing TOCTOU and partial-write corruption if the process is killed.
    QSaveFile file(getConfigPath());
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Impossible de sauvegarder la configuration";
        return false;
    }

    file.write(QJsonDocument(m_config).toJson(QJsonDocument::Compact));
    if (!file.commit()) {
        qWarning() << "Échec du commit atomique de la configuration";
        return false;
    }
    return true;
}

QVariant ConfigManager::get(const QString& key, const QVariant& defaultValue) const {
    if (m_config.contains(key)) {
        return m_config.value(key).toVariant();
    }
    return defaultValue;
}

void ConfigManager::set(const QString& key, const QVariant& value) {
    m_config[key] = QJsonValue::fromVariant(value);
    scheduleSave();
    emit configChanged(key);
}

QString ConfigManager::getWowPath() const {
    return m_config.value("wow_path").toString();
}

void ConfigManager::setWowPath(const QString& path) {
    m_config["wow_path"] = path;
    scheduleSave();
    emit configChanged("wow_path");
}

// --- Global settings ---------------------------------------------------------

bool ConfigManager::isFirstRun() const {
    return m_config.value("first_run").toBool(true);
}

void ConfigManager::setFirstRun(bool firstRun) {
    m_config["first_run"] = firstRun;
    scheduleSave();
}

bool ConfigManager::isSoundEnabled() const {
    return m_config.value("sound_enabled").toBool(true);
}

void ConfigManager::setSoundEnabled(bool enabled) {
    m_config["sound_enabled"] = enabled;
    scheduleSave();
    emit configChanged("sound_enabled");
}

QString ConfigManager::getLanguage() const {
    return get("language", "fr").toString();
}

void ConfigManager::setLanguage(const QString& lang) {
    set("language", lang);
}

QString ConfigManager::getBackground() const {
    return m_config.value("background")
        .toString(QString::fromUtf8(SylvaniaConstants::kDefaultBackground));
}

void ConfigManager::setBackground(const QString& bgName) {
    set("background", bgName);
}

bool ConfigManager::isRandomBackgroundEnabled() const {
    return get("random_bg_on_launch", true).toBool();
}

void ConfigManager::setRandomBackgroundEnabled(bool enabled) {
    set("random_bg_on_launch", enabled);
}

QByteArray ConfigManager::getWindowGeometry() const {
    return QByteArray::fromBase64(get("window_geometry", "").toString().toLatin1());
}

void ConfigManager::setWindowGeometry(const QByteArray& geometry) {
    set("window_geometry", QString::fromLatin1(geometry.toBase64()));
}

// --- Realmlist ----------------------------------------------

std::vector<RealmlistEntry> ConfigManager::getRealmlistEntries() const {
    std::vector<RealmlistEntry> entries;

    QJsonArray array = m_config.value("realmlist_entries").toArray();
    for (const QJsonValue& val : array) {
        QJsonObject obj = val.toObject();
        RealmlistEntry entry;
        entry.name = obj.value("name").toString();
        entry.address = obj.value("address").toString();
        entry.active = obj.value("active").toBool(false);
        entries.push_back(entry);
    }

    return entries;
}

void ConfigManager::setRealmlistEntries(const std::vector<RealmlistEntry>& entries) {
    QJsonArray array;
    for (const auto& entry : entries) {
        QJsonObject obj{
            {"name", entry.name},
            {"address", entry.address},
            {"active", entry.active}
        };
        array.append(obj);
    }
    m_config["realmlist_entries"] = array;
    scheduleSave();
    emit realmlistChanged();
}

void ConfigManager::setRealmlistEntry(int index, const QString& name, const QString& address, bool active) {
    QJsonArray array = m_config.value("realmlist_entries").toArray();

    QJsonObject obj{
        {"name", name},
        {"address", address},
        {"active", active}
    };
    if (index >= 0 && index < array.size()) {
        array[index] = obj;
    } else if (index == array.size()) {
        array.append(obj);
    }

    m_config["realmlist_entries"] = array;
    scheduleSave();
    emit realmlistChanged();
}

int ConfigManager::getActiveRealmlistIndex() const {
    return m_config.value("active_realmlist_index").toInt(0);
}

void ConfigManager::setActiveRealmlistIndex(int index) {
    m_config["active_realmlist_index"] = index;

    // Update active flags in entries
    QJsonArray array = m_config.value("realmlist_entries").toArray();
    for (int i = 0; i < array.size(); ++i) {
        QJsonObject obj = array[i].toObject();
        obj["active"] = (i == index);
        array[i] = obj;
    }
    m_config["realmlist_entries"] = array;

    scheduleSave();
    emit realmlistChanged();
}

bool ConfigManager::switchRealmlist(int index) {
    auto entries = getRealmlistEntries();
    if (index < 0 || index >= static_cast<int>(entries.size())) {
        return false;
    }

    setActiveRealmlistIndex(index);
    return updateRealmlist(entries[index].address);
}

QString ConfigManager::hostFromAddress(const QString& address) {
    QString host = address.trimmed();
    // Strip the classic prefix and any quotes so both "set realmlist x" and
    // a bare host normalise to just the host.
    static const QRegularExpression prefix(
        QStringLiteral("^set\\s+realmlist\\s+"),
        QRegularExpression::CaseInsensitiveOption);
    host.remove(prefix);
    host.remove(QLatin1Char('"'));
    return host.trimmed();
}

bool ConfigManager::updateRealmlist(const QString& realmlistText) {
    QString wowPath = getWowPath();
    if (wowPath.isEmpty()) {
        qWarning() << "Chemin WoW non défini";
        return false;
    }

    // Find and update all realmlist.wtf files
    QStringList realmlistPaths = {
        wowPath + "/realmlist.wtf",
        wowPath + "/Data/frFR/realmlist.wtf",
        wowPath + "/Data/enUS/realmlist.wtf",
        wowPath + "/Data/enGB/realmlist.wtf"
    };

    bool success = false;
    for (const QString& path : realmlistPaths) {
        QFileInfo fileInfo(path);

        // Skip locales that don't exist on disk.
        if (!fileInfo.dir().exists()) {
            continue;
        }

        // C3: atomic write avoids leaving a half-written realmlist.wtf if WoW
        // launches concurrently or the launcher crashes mid-write.
        QSaveFile file(path);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            stream << realmlistText << "\n";
            stream.flush();
            if (file.commit()) {
                qDebug() << "Realmlist mis à jour:" << path;
                success = true;
            }
        }
    }

    return success;
}
