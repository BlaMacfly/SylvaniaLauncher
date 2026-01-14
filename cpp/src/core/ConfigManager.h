#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QJsonObject>
#include <QJsonArray>
#include <vector>

/**
 * @brief Realmlist entry structure
 */
struct RealmlistEntry {
    QString name;
    QString address;
    bool active = false;
};

/**
 * @brief Configuration manager for Sylvania Launcher
 * 
 * Handles reading/writing configuration settings to JSON file
 * stored in %LOCALAPPDATA%/SylvaniaLauncher/config.json
 */
class ConfigManager : public QObject {
    Q_OBJECT

public:
    explicit ConfigManager(QObject* parent = nullptr);
    ~ConfigManager() override = default;

    // Generic config access
    QVariant get(const QString& key, const QVariant& defaultValue = QVariant()) const;
    void set(const QString& key, const QVariant& value);
    bool save();

    // Specific getters
    QString getWowPath() const;
    void setWowPath(const QString& path);
    
    bool isFirstRun() const;
    void setFirstRun(bool firstRun);
    
    bool isSoundEnabled() const;
    void setSoundEnabled(bool enabled);
    
    QString getLanguage() const;
    void setLanguage(const QString& lang);

    // Realmlist management
    std::vector<RealmlistEntry> getRealmlistEntries() const;
    void setRealmlistEntries(const std::vector<RealmlistEntry>& entries);
    void setRealmlistEntry(int index, const QString& name, const QString& address, bool active = false);
    int getActiveRealmlistIndex() const;
    void setActiveRealmlistIndex(int index);
    bool switchRealmlist(int index);
    bool updateRealmlist(const QString& realmlistText);

signals:
    void configChanged(const QString& key);
    void realmlistChanged();

private:
    void loadConfig();
    void createDefaultConfig();
    QString getConfigPath() const;

    QJsonObject m_config;
    QString m_configDir;
};
