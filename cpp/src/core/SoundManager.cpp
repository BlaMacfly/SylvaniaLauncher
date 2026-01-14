#include "SoundManager.h"
#include "PathUtils.h"

#include <QUrl>
#include <QFile>
#include <QDir>
#include <QDebug>

SoundManager::SoundManager(QObject* parent)
    : QObject(parent)
    , m_player(std::make_unique<QMediaPlayer>())
    , m_audioOutput(std::make_unique<QAudioOutput>())
{
    m_player->setAudioOutput(m_audioOutput.get());
    m_audioOutput->setVolume(m_volume);
    
    initializeSounds();
}

void SoundManager::initializeSounds() {
    m_soundsDir = findSoundsDir();
    
    if (!QDir(m_soundsDir).exists()) {
        qWarning() << "Répertoire des sons non trouvé:" << m_soundsDir;
        return;
    }
    
    // Try WAV files first, then MP3 as fallback
    QMap<QString, QStringList> soundFiles = {
        {"play", {"launch.wav", "launch.mp3"}},
        {"button", {"toggle.wav", "toggle.mp3"}},
        {"toggle", {"toggle.wav", "toggle.mp3"}},
        {"notification", {"Notification.wav", "Notification.mp3"}}
    };
    
    for (auto it = soundFiles.begin(); it != soundFiles.end(); ++it) {
        const QString& soundName = it.key();
        const QStringList& files = it.value();
        
        for (const QString& file : files) {
            QString fullPath = m_soundsDir + "/" + file;
            if (QFile::exists(fullPath)) {
                addSound(soundName, fullPath);
                qDebug() << "Son chargé:" << soundName << "->" << fullPath;
                break;
            }
        }
    }
}

QString SoundManager::findSoundsDir() const {
    return PathUtils::getSoundsDir();
}

void SoundManager::addSound(const QString& name, const QString& path) {
    if (QFile::exists(path)) {
        m_sounds[name] = path;
    } else {
        qWarning() << "Fichier son non trouvé:" << path;
    }
}

void SoundManager::play(const QString& soundName) {
    if (m_muted) {
        return;
    }
    
    if (!m_sounds.contains(soundName)) {
        qWarning() << "Son non trouvé:" << soundName;
        return;
    }
    
    QString soundPath = m_sounds[soundName];
    
    if (!QFile::exists(soundPath)) {
        qWarning() << "Fichier son introuvable:" << soundPath;
        return;
    }
    
    try {
        // Stop current playback before starting new
        m_player->stop();
        m_player->setSource(QUrl::fromLocalFile(soundPath));
        m_player->play();
    } catch (...) {
        qWarning() << "Erreur lors de la lecture du son:" << soundName;
    }
}

void SoundManager::setVolume(float volume) {
    m_volume = qBound(0.0f, volume, 1.0f);
    m_audioOutput->setVolume(m_volume);
}

float SoundManager::getVolume() const {
    return m_audioOutput->volume();
}

void SoundManager::setMuted(bool muted) {
    m_muted = muted;
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
