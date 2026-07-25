#pragma once

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QMap>
#include <QAudioDevice>
#include <QElapsedTimer>
#include <QMediaDevices>
#include <QSoundEffect>
#include <memory>

class QProcess;

/**
 * @brief Sound manager for the launcher
 * 
 * Handles audio playback for UI sounds (button clicks, notifications, etc.)
 * Uses Qt Multimedia for cross-platform audio support.
 */
class SoundManager : public QObject {
    Q_OBJECT

public:
    explicit SoundManager(QObject* parent = nullptr);
    ~SoundManager() override = default;

    /**
     * @brief Play a sound by name
     * @param soundName Name of the sound (e.g., "play", "button", "toggle")
     */
    void play(const QString& soundName);

    /**
     * @brief Set the master volume
     * @param volume Volume level (0.0 to 1.0)
     */
    void setVolume(float volume);

    /**
     * @brief Get the current volume
     * @return Current volume (0.0 to 1.0)
     */
    float getVolume() const;

    /**
     * @brief Set muted state
     * @param muted True to mute, false to unmute
     */
    void setMuted(bool muted);

    /**
     * @brief Check if sound is muted
     * @return True if muted
     */
    bool isMuted() const;

    /**
     * @brief Set enabled state (inverse of muted)
     * @param enabled True to enable sounds
     */
    void setEnabled(bool enabled);

    /**
     * @brief Check if sounds are enabled
     * @return True if enabled
     */
    bool isEnabled() const;

private:
    void initializeSounds();
    void addSound(const QString& name, const QString& path,
                  const QAudioDevice& device);
    QString findSoundsDir() const;

    /// The output the sounds SHOULD go to: what the audio server reported if we
    /// managed to ask it, Qt's own default otherwise.
    QAudioDevice targetOutputDevice() const;

    /// Rebuild every effect on targetOutputDevice(), when that is no longer the
    /// device they were built on. No-op otherwise.
    void refreshOutputDevice();
    void buildEffects(const QAudioDevice& device);

#ifdef Q_OS_LINUX
    /// Ask the audio server (pactl) which sink is really the default, and move
    /// the effects there when Qt disagrees. Asynchronous; a no-op when pactl or
    /// PulseAudio/PipeWire is absent.
    void probeServerDefaultOutput();
#endif

    QMap<QString, QString> m_soundPaths;   ///< sound name -> .wav path
    QMap<QString, std::shared_ptr<QSoundEffect>> m_effects;
    QMediaDevices* m_mediaDevices;         ///< emits audioOutputsChanged
    QByteArray m_deviceId;                 ///< device the effects are bound to
#ifdef Q_OS_LINUX
    QByteArray m_serverDeviceId;           ///< sink the audio server calls default
    QProcess* m_probe = nullptr;           ///< running pactl probe, if any
    QElapsedTimer m_lastProbe;             ///< throttles the probe
#endif
    QString m_soundsDir;
    bool m_muted = false;
    float m_volume = 0.5f;
};

