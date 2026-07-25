#include "SoundManager.h"
#include "PathUtils.h"

#include <QUrl>
#include <QFile>
#include <QDir>
#include <QDebug>
#include <QProcess>
#include <QProcessEnvironment>

#ifdef Q_OS_LINUX
namespace {

// How long a probe result is trusted before asking the audio server again.
constexpr qint64 kProbeIntervalMs = 2000;

} // namespace
#endif

SoundManager::SoundManager(QObject* parent)
    : QObject(parent)
    , m_mediaDevices(new QMediaDevices(this))
{
    initializeSounds();

    // Plugging a headset, or an audio server that only answers "which device
    // is the default" once it has settled, must not leave the launcher playing
    // into a device nobody is listening to. See refreshOutputDevice().
    connect(m_mediaDevices, &QMediaDevices::audioOutputsChanged, this, [this] {
        refreshOutputDevice();
#ifdef Q_OS_LINUX
        probeServerDefaultOutput();
#endif
    });

#ifdef Q_OS_LINUX
    probeServerDefaultOutput();
#endif
}

void SoundManager::initializeSounds() {
    m_soundsDir = findSoundsDir();

    if (!QDir(m_soundsDir).exists()) {
        qWarning() << "Répertoire des sons non trouvé:" << m_soundsDir;
        return;
    }

    // For QSoundEffect we only need WAV files
    QMap<QString, QStringList> soundFiles = {
        {"play", {"launch.wav"}},
        {"button", {"toggle.wav"}},
        {"toggle", {"toggle.wav"}},
        {"notification", {"Notification.wav"}}
    };

    for (auto it = soundFiles.begin(); it != soundFiles.end(); ++it) {
        const QString& soundName = it.key();
        const QStringList& files = it.value();

        for (const QString& file : files) {
            QString fullPath = m_soundsDir + "/" + file;
            if (QFile::exists(fullPath)) {
                m_soundPaths[soundName] = fullPath;
                qDebug() << "Son chargé:" << soundName << "->" << fullPath;
                break;
            }
        }
    }

    buildEffects(QMediaDevices::defaultAudioOutput());
}

void SoundManager::buildEffects(const QAudioDevice& device) {
    m_effects.clear();
    // An explicit device is what ends up in the audio stream's routing, so it
    // is recorded and compared later — see refreshOutputDevice().
    m_deviceId = device.id();

    for (auto it = m_soundPaths.begin(); it != m_soundPaths.end(); ++it)
        addSound(it.key(), it.value(), device);

    qDebug() << "Sons routés vers:"
             << (device.isNull() ? QStringLiteral("(périphérique par défaut)")
                                 : device.description());
}

QAudioDevice SoundManager::targetOutputDevice() const {
#ifdef Q_OS_LINUX
    // What the audio server told us WINS over Qt's own resolution — that is the
    // whole point of the probe, and Qt's answer must not undo it on the next
    // click. Stale ids simply no longer match and fall through.
    if (!m_serverDeviceId.isEmpty()) {
        for (const QAudioDevice& device : QMediaDevices::audioOutputs()) {
            if (device.id() == m_serverDeviceId) return device;
        }
    }
#endif
    return QMediaDevices::defaultAudioOutput();
}

void SoundManager::refreshOutputDevice() {
    const QAudioDevice device = targetOutputDevice();
    if (device.id() == m_deviceId) {
        return;  // still the same output: keep the loaded samples
    }

    // Qt binds a QSoundEffect to an output device ONCE, when the effect is
    // built. Building them all in the constructor therefore pins them to
    // whatever Qt believed the default was during startup. Rebuilding on the
    // device that is current now costs one string comparison per click.
    buildEffects(device);
}

#ifdef Q_OS_LINUX
void SoundManager::probeServerDefaultOutput() {
    // Qt resolves "which output is the default" through its PulseAudio backend,
    // and the version bundled in the AppImage (6.5.3) gets it WRONG under
    // PipeWire: it falls back to the FIRST sink of the list instead of the
    // server's default, which sent every launcher sound into a headset while
    // the speakers stayed silent. The audio server itself is authoritative and
    // version-proof, and QAudioDevice::id() is exactly the PulseAudio sink
    // name, so its answer maps straight onto Qt's device list.
    //
    // Entirely optional: without pactl (or without PulseAudio/PipeWire) the
    // probe yields nothing and Qt's own answer stands. Asynchronous on purpose
    // — the UI thread must never wait on an external process.
    if (m_probe) {
        return;  // one in flight is enough
    }
    m_lastProbe.start();

    // The lambdas hold the process themselves: a crashed pactl fires BOTH
    // errorOccurred and finished, and reading the member in the second one
    // would dereference what the first already cleared.
    auto* probe = new QProcess(this);
    m_probe = probe;

    connect(probe, &QProcess::finished, this,
            [this, probe](int exitCode, QProcess::ExitStatus status) {
        const QByteArray name = probe->readAllStandardOutput().trimmed();
        if (m_probe == probe) m_probe = nullptr;
        probe->deleteLater();

        if (status != QProcess::NormalExit || exitCode != 0) return;
        // Pulse answers "@DEFAULT_SINK@" when nothing explicit is configured;
        // there is then nothing to correct.
        if (name.isEmpty() || name.startsWith('@') || name == m_serverDeviceId) {
            return;
        }
        m_serverDeviceId = name;
        refreshOutputDevice();
    });
    connect(probe, &QProcess::errorOccurred, this,
            [this, probe](QProcess::ProcessError error) {
        // Anything else also fires finished(), the single reporting path.
        if (error != QProcess::FailedToStart) return;
        if (m_probe == probe) m_probe = nullptr;
        probe->deleteLater();   // pactl absent: keep Qt's answer, no warning
    });

    // Inside the AppImage these point at the BUNDLED libraries (an older
    // libpulse among them). A host pactl must resolve its own — same reason
    // WineManager scrubs them before every Wine call.
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.remove(QStringLiteral("LD_LIBRARY_PATH"));
    env.remove(QStringLiteral("LD_PRELOAD"));
    probe->setProcessEnvironment(env);

    probe->start(QStringLiteral("pactl"), {QStringLiteral("get-default-sink")});
}
#endif // Q_OS_LINUX

QString SoundManager::findSoundsDir() const {
    return PathUtils::getSoundsDir();
}

void SoundManager::addSound(const QString& name, const QString& path,
                            const QAudioDevice& device) {
    if (QFile::exists(path)) {
        // A null device would make QSoundEffect ask for a sink named "" instead
        // of falling back to the server default: only pass one when it is real.
        auto effect = device.isNull() ? std::make_shared<QSoundEffect>()
                                      : std::make_shared<QSoundEffect>(device);
        effect->setSource(QUrl::fromLocalFile(path));
        effect->setVolume(m_volume);
        m_effects[name] = effect;
    } else {
        qWarning() << "Fichier son non trouvé:" << path;
    }
}

void SoundManager::play(const QString& soundName) {
    if (m_muted) {
        return;
    }

    // Cheap (one id comparison) unless the default output actually moved.
    refreshOutputDevice();
#ifdef Q_OS_LINUX
    // Switching output from the desktop's audio applet changes no device LIST,
    // so audioOutputsChanged never fires: re-ask, throttled, on user activity.
    // The answer lands after this click — the NEXT one is routed correctly.
    if (!m_lastProbe.isValid() || m_lastProbe.elapsed() > kProbeIntervalMs) {
        probeServerDefaultOutput();
    }
#endif

    if (!m_effects.contains(soundName)) {
        qWarning() << "Son non trouvé:" << soundName;
        return;
    }
    
    try {
        m_effects[soundName]->play();
    } catch (...) {
        qWarning() << "Erreur lors de la lecture du son:" << soundName;
    }
}

void SoundManager::setVolume(float volume) {
    m_volume = qBound(0.0f, volume, 1.0f);
    for (auto& effect : m_effects) {
        effect->setVolume(m_volume);
    }
}

float SoundManager::getVolume() const {
    return m_volume;
}

void SoundManager::setMuted(bool muted) {
    m_muted = muted;
    // We don't stop currently playing sounds on mute, just prevent new ones
}

bool SoundManager::isMuted() const {
    return m_muted;
}

void SoundManager::setEnabled(bool enabled) {
    setMuted(!enabled);
}

bool SoundManager::isEnabled() const {
    return !m_muted;
}
