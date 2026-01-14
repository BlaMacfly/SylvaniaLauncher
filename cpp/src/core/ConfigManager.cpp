#include "ConfigManager.h"
#include "PathUtils.h"

#include <QFile>
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
        {"address", "set realmlist logon.sylvania-wow.com"},
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
    
    QFile file(getConfigPath());
    if (file.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(m_config);
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
        return true;
    }
    
    qWarning() << "Impossible de sauvegarder la configuration";
    return false;
}

QVariant ConfigManager::get(const QString& key, const QVariant& defaultValue) const {
    if (m_config.contains(key)) {
        return m_config.value(key).toVariant();
    }
    return defaultValue;
}

void ConfigManager::set(const QString& key, const QVariant& value) {
    m_config[key] = QJsonValue::fromVariant(value);
    save();
    emit configChanged(key);
}

QString ConfigManager::getWowPath() const {
    return m_config.value("wow_path").toString();
}

void ConfigManager::setWowPath(const QString& path) {
    m_config["wow_path"] = path;
    save();
    emit configChanged("wow_path");
}

bool ConfigManager::isFirstRun() const {
    return m_config.value("first_run").toBool(true);
}

void ConfigManager::setFirstRun(bool firstRun) {
    m_config["first_run"] = firstRun;
    save();
}

bool ConfigManager::isSoundEnabled() const {
    return m_config.value("sound_enabled").toBool(true);
}

void ConfigManager::setSoundEnabled(bool enabled) {
    m_config["sound_enabled"] = enabled;
    save();
    emit configChanged("sound_enabled");
}

QString ConfigManager::getLanguage() const {
    return m_config.value("language").toString("fr");
}

void ConfigManager::setLanguage(const QString& lang) {
    m_config["language"] = lang;
    save();
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
    save();
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
    save();
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
    
    save();
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
        QFile file(path);
        QFileInfo fileInfo(path);
        
        // Create parent directory if it doesn't exist
        QDir parentDir = fileInfo.dir();
        if (!parentDir.exists()) {
            continue; // Skip if locale folder doesn't exist
        }
        
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            stream << realmlistText << "\n";
            file.close();
            qDebug() << "Realmlist mis à jour:" << path;
            success = true;
        }
    }
    
    return success;
}
