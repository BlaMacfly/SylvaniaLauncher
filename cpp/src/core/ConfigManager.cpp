#include "ConfigManager.h"
#include "PathUtils.h"
#include "Constants.h"

#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDir>
#include <QTextStream>
#include <QDebug>

ConfigManager::ConfigManager(QObject* parent)
    : QObject(parent)
{
    m_configDir = PathUtils::getConfigDir();
    loadConfig();

    // H4: coalesce bursts of setX() calls (e.g. Settings dialog applying many
    // options at once) into a single disk write 200 ms after the last change.
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(SylvaniaConstants::kConfigSaveDebounceMs);
    connect(&m_saveTimer, &QTimer::timeout, this, [this]() { save(); });
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
        {"wow_path", ""},
        {"sound_enabled", true},
        {"download_method", "direct"},
        {"max_upload_rate", 100},
        {"max_download_rate", 0},
        {"first_run", true},
        {"auto_update", true},
        {"language", "fr"},
        {"active_realmlist_index", 0}
    };
    
    // Default realmlist entries
    QJsonArray realmlistArray;
    QJsonObject defaultRealm{
        {"name", "Sylvania"},
        {"address", "set realmlist sylvania-servergame.com"},
        {"active", true}
    };
    realmlistArray.append(defaultRealm);
    m_config["realmlist_entries"] = realmlistArray;
    
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
    return get("background", "Azeroth").toString();
}

void ConfigManager::setBackground(const QString& bgName) {
    set("background", bgName);
}

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
    
    if (index >= 0 && index < array.size()) {
        QJsonObject obj{
            {"name", name},
            {"address", address},
            {"active", active}
        };
        array[index] = obj;
    } else if (index == array.size()) {
        // Add new entry
        QJsonObject obj{
            {"name", name},
            {"address", address},
            {"active", active}
        };
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
