#pragma once

#include <QObject>
#include <QString>
#include <QMap>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <memory>

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
    void addSound(const QString& name, const QString& path);
    QString findSoundsDir() const;

    std::unique_ptr<QMediaPlayer> m_player;
    std::unique_ptr<QAudioOutput> m_audioOutput;
    QMap<QString, QString> m_sounds;
    QString m_soundsDir;
    bool m_muted = false;
    float m_volume = 0.5f;
};
