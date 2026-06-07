#include "AddonManifest.h"
#include "Constants.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>

AddonManifest::AddonManifest(QObject* parent)
    : QObject(parent)
    , m_network(std::make_unique<QNetworkAccessManager>(this))
{
}

AddonManifest::~AddonManifest() {
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
    }
}

std::vector<ManifestAddon> AddonManifest::parse(const QByteArray& json, bool* ok) {
    if (ok) *ok = false;
    std::vector<ManifestAddon> result;

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return result;
    }

    const QJsonArray array = doc.object().value("addons").toArray();
    for (const QJsonValue& v : array) {
        const QJsonObject obj = v.toObject();
        ManifestAddon a;
        a.id = obj.value("id").toString();
        a.name = obj.value("name").toString();
        a.version = obj.value("version").toString();
        a.description = obj.value("description").toString();
        a.url = obj.value("url").toString();
        const QJsonArray folders = obj.value("folders").toArray();
        for (const QJsonValue& f : folders) {
            const QString folder = f.toString();
            if (!folder.isEmpty()) a.folders << folder;
        }
        // An addon is only usable if it has an id, a download URL and at least
        // one expected folder (needed to verify install and to uninstall).
        if (!a.id.isEmpty() && !a.url.isEmpty() && !a.folders.isEmpty()) {
            result.push_back(a);
        }
    }

    if (ok) *ok = !result.empty();
    return result;
}

std::vector<ManifestAddon> AddonManifest::embedded() {
    QFile file(QStringLiteral(":/addons/manifest"));
    if (file.open(QIODevice::ReadOnly)) {
        const QByteArray data = file.readAll();
        file.close();
        return parse(data);
    }
    return {};
}

void AddonManifest::fetch() {
    // Guard against overlapping fetches.
    if (m_reply) return;

    QNetworkRequest request{QUrl(QString::fromUtf8(SylvaniaConstants::kAddonManifestUrl))};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::SameOriginRedirectPolicy);

    m_reply = m_network->get(request);
    connect(m_reply, &QNetworkReply::finished, this, [this]() {
        QNetworkReply* reply = m_reply;
        m_reply = nullptr;

        bool fromRemote = false;
        if (reply && reply->error() == QNetworkReply::NoError) {
            bool ok = false;
            std::vector<ManifestAddon> remote = parse(reply->readAll(), &ok);
            if (ok) {
                m_addons = std::move(remote);
                fromRemote = true;
            }
        }
        if (reply) reply->deleteLater();

        if (!fromRemote) {
            m_addons = embedded();
        }
        emit loaded(fromRemote);
    });
}
