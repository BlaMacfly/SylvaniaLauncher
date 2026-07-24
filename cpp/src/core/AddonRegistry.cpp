#include "AddonRegistry.h"
#include "PathUtils.h"

#include <QFile>
#include <QSaveFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>

AddonRegistry::AddonRegistry() {
    reload();
}

QString AddonRegistry::filePath() {
    return PathUtils::getConfigDir() + "/installed_addons.json";
}

void AddonRegistry::reload() {
    m_addons = QJsonObject();
    QFile file(filePath());
    if (file.exists() && file.open(QIODevice::ReadOnly)) {
        const QByteArray data = file.readAll();
        file.close();
        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
        if (err.error == QJsonParseError::NoError && doc.isObject()) {
            m_addons = doc.object();
        }
    }
}

bool AddonRegistry::contains(const QString& id) const {
    return m_addons.contains(id);
}

InstalledAddon AddonRegistry::fromJson(const QString& id, const QJsonObject& obj) {
    InstalledAddon a;
    a.id = obj.value("id").toString(id);
    a.name = obj.value("name").toString();
    a.version = obj.value("version").toString();
    a.source = obj.value("source").toString();
    const QJsonArray folders = obj.value("folders").toArray();
    for (const QJsonValue& v : folders) {
        const QString f = v.toString();
        if (!f.isEmpty()) a.folders << f;
    }
    return a;
}

QJsonObject AddonRegistry::toJson(const InstalledAddon& addon) {
    QJsonArray folders;
    for (const QString& f : addon.folders) folders.append(f);
    return QJsonObject{
        {"id", addon.id},
        {"name", addon.name},
        {"version", addon.version},
        {"source", addon.source},
        {"folders", folders}
    };
}

InstalledAddon AddonRegistry::get(const QString& id) const {
    return fromJson(id, m_addons.value(id).toObject());
}

QString AddonRegistry::installedVersion(const QString& id) const {
    return m_addons.value(id).toObject().value("version").toString();
}

QStringList AddonRegistry::folders(const QString& id) const {
    return get(id).folders;
}

std::vector<InstalledAddon> AddonRegistry::all() const {
    std::vector<InstalledAddon> result;
    for (auto it = m_addons.begin(); it != m_addons.end(); ++it) {
        result.push_back(fromJson(it.key(), it.value().toObject()));
    }
    return result;
}

bool AddonRegistry::record(const InstalledAddon& addon) {
    if (addon.id.isEmpty()) return false;
    m_addons[addon.id] = toJson(addon);
    return save();
}

bool AddonRegistry::remove(const QString& id) {
    if (!m_addons.contains(id)) return true;
    m_addons.remove(id);
    return save();
}

bool AddonRegistry::save() const {
    const QString path = filePath();
    QDir().mkpath(PathUtils::getConfigDir());

    // Atomic write, matching ConfigManager: temp file + rename on commit so a
    // crash mid-write can never corrupt the registry.
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(QJsonDocument(m_addons).toJson(QJsonDocument::Indented));
    return file.commit();
}
