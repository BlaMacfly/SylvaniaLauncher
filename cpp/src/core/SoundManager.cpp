#include "SoundManager.h"
#include "PathUtils.h"

#include <QUrl>
#include <QFile>
#include <QDir>
#include <QDebug>

SoundManager::SoundManager(QObject* parent)
    : QObject(parent)
{
    initializeSounds();
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
        auto effect = std::make_shared<QSoundEffect>();
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
