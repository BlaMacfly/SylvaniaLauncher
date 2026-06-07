#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <memory>
#include <vector>

/**
 * @brief One entry of the recommended-addons manifest.
 */
struct ManifestAddon {
    QString id;
    QString name;
    QString version;
    QString description;
    QString url;            // download URL (may be a ?file=... PHP endpoint)
    QStringList folders;    // folders expected under Interface/AddOns
};

/**
 * @brief Loads the "recommended addons" manifest.
 *
 * Fetches addons.json from the Sylvania server (SylvaniaConstants::kAddonManifestUrl)
 * and, if that fails or returns nothing usable, falls back to the copy embedded
 * in resources (:/addons/manifest) so the list always renders — even offline.
 * Adding an addon later is a server-side manifest edit; no client rebuild needed.
 */
class AddonManifest : public QObject {
    Q_OBJECT

public:
    explicit AddonManifest(QObject* parent = nullptr);
    ~AddonManifest() override;

    // Asynchronous. Emits loaded() exactly once; never blocks the UI thread.
    void fetch();

    const std::vector<ManifestAddon>& addons() const { return m_addons; }

    // Parse a manifest JSON document. ok (if provided) reports whether the
    // document was well-formed and contained at least one valid addon.
    static std::vector<ManifestAddon> parse(const QByteArray& json, bool* ok = nullptr);
    // The manifest bundled in resources, used as offline fallback.
    static std::vector<ManifestAddon> embedded();

signals:
    // fromRemote = false means the embedded fallback was used.
    void loaded(bool fromRemote);

private:
    std::unique_ptr<QNetworkAccessManager> m_network;
    QNetworkReply* m_reply = nullptr;
    std::vector<ManifestAddon> m_addons;
};
