#include "NotesManager.h"
#include "PathUtils.h"
#include "Constants.h"

#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QDir>
#include <QUuid>
#include <QDebug>
#include <algorithm>

// Note serialization
QJsonObject Note::toJson() const {
    return QJsonObject{
        {"id", id},
        {"title", title},
        {"content", content},
        {"category", category},
        {"tags", QJsonArray::fromStringList(tags)},
        {"created_at", createdAt.toString(Qt::ISODate)},
        {"updated_at", updatedAt.toString(Qt::ISODate)},
        {"is_favorite", isFavorite},
        {"is_archived", isArchived}
    };
}

Note Note::fromJson(const QJsonObject& obj) {
    Note note;
    note.id = obj.value("id").toString();
    note.title = obj.value("title").toString();
    note.content = obj.value("content").toString();
    note.category = obj.value("category").toString("general");
    
    QJsonArray tagsArray = obj.value("tags").toArray();
    for (const QJsonValue& tag : tagsArray) {
        note.tags.append(tag.toString());
    }
    
    note.createdAt = QDateTime::fromString(obj.value("created_at").toString(), Qt::ISODate);
    note.updatedAt = QDateTime::fromString(obj.value("updated_at").toString(), Qt::ISODate);
    note.isFavorite = obj.value("is_favorite").toBool(false);
    note.isArchived = obj.value("is_archived").toBool(false);
    
    return note;
}

// Category serialization
QJsonObject NoteCategory::toJson() const {
    return QJsonObject{
        {"id", id},
        {"name", name},
        {"color", color}
    };
}

NoteCategory NoteCategory::fromJson(const QJsonObject& obj) {
    NoteCategory cat;
    cat.id = obj.value("id").toString();
    cat.name = obj.value("name").toString();
    cat.color = obj.value("color").toString("#4CAF50");
    return cat;
}

// NotesManager implementation
NotesManager::NotesManager(QObject* parent)
    : QObject(parent)
{
    m_dataDir = PathUtils::getDataDir();
    migrateOldNotes();
    loadNotes();
    loadCategories();
    
    if (m_categories.empty()) {
        createDefaultCategories();
    }
}

void NotesManager::createDefaultCategories() {
    m_categories = {
        {"general", "Général", "#4CAF50"},
        {"wow", "World of Warcraft", "#FF9800"},
        {"guides", "Guides", "#2196F3"},
        {"todo", "À faire", "#F44336"},
        {"ideas", "Idées", "#9C27B0"}
    };
    saveCategories();
}

QString NotesManager::getNotesFilePath() const {
    return m_dataDir + "/notes.json";
}

QString NotesManager::getCategoriesFilePath() const {
    return m_dataDir + "/note_categories.json";
}

void NotesManager::migrateOldNotes() {
    // Check if current notes file exists
    if (QFile::exists(getNotesFilePath())) {
        QFileInfo info(getNotesFilePath());
        if (info.size() > 2) {
            return; // Notes already exist
        }
    }
    
    // Try to migrate from old locations
    QStringList oldPaths = {
        QDir::homePath() + "/Documents/Sylvania Launcher/data/notes.json",
        QDir::currentPath() + "/data/notes.json"
    };
    
    for (const QString& oldPath : oldPaths) {
        if (QFile::exists(oldPath)) {
            QFileInfo info(oldPath);
            if (info.size() > 2) {
                QFile::copy(oldPath, getNotesFilePath());
                
                // Also copy categories if they exist
                QString oldCategories = QFileInfo(oldPath).dir().filePath("note_categories.json");
                if (QFile::exists(oldCategories)) {
                    QFile::copy(oldCategories, getCategoriesFilePath());
                }
                
                qDebug() << "Notes migrées depuis:" << oldPath;
                break;
            }
        }
    }
}

void NotesManager::loadNotes() {
    m_notes.clear();

    QFile file(getNotesFilePath());
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        rebuildSearchIndex();
        return;
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    file.close();

    if (error.error != QJsonParseError::NoError || !doc.isArray()) {
        qWarning() << "Erreur de chargement des notes:" << error.errorString();
        rebuildSearchIndex();
        return;
    }

    QJsonArray array = doc.array();
    for (const QJsonValue& val : array) {
        if (val.isObject()) {
            m_notes.push_back(Note::fromJson(val.toObject()));
        }
    }

    rebuildSearchIndex();
    qDebug() << "Chargé" << m_notes.size() << "notes";
}

void NotesManager::appendSearchIndexFor(const Note& note) {
    SearchIndexEntry entry;
    entry.titleLower = note.title.toLower();
    entry.contentLower = note.content.toLower();
    entry.tagsLower.reserve(note.tags.size());
    for (const QString& tag : note.tags) {
        entry.tagsLower.append(tag.toLower());
    }
    m_searchIndex.push_back(std::move(entry));
}

void NotesManager::rebuildSearchIndex() {
    m_searchIndex.clear();
    m_searchIndex.reserve(m_notes.size());
    for (const Note& note : m_notes) {
        appendSearchIndexFor(note);
    }
}

bool NotesManager::saveNotes() {
    QDir dir(m_dataDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QJsonArray array;
    for (const Note& note : m_notes) {
        array.append(note.toJson());
    }

    QSaveFile file(getNotesFilePath());
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Impossible de sauvegarder les notes";
        return false;
    }

    file.write(QJsonDocument(array).toJson(QJsonDocument::Compact));
    if (!file.commit()) {
        qWarning() << "Échec du commit atomique des notes";
        return false;
    }
    return true;
}

void NotesManager::loadCategories() {
    m_categories.clear();
    
    QFile file(getCategoriesFilePath());
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return;
    }
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    file.close();
    
    if (error.error != QJsonParseError::NoError || !doc.isArray()) {
        return;
    }
    
    QJsonArray array = doc.array();
    for (const QJsonValue& val : array) {
        if (val.isObject()) {
            m_categories.push_back(NoteCategory::fromJson(val.toObject()));
        }
    }
}

bool NotesManager::saveCategories() {
    QJsonArray array;
    for (const NoteCategory& cat : m_categories) {
        array.append(cat.toJson());
    }

    QSaveFile file(getCategoriesFilePath());
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    file.write(QJsonDocument(array).toJson(QJsonDocument::Compact));
    return file.commit();
}

std::optional<QString> NotesManager::createNote(const QString& title, const QString& content,
                                                  const QString& category, const QStringList& tags) {
    if (title.trimmed().isEmpty() || content.trimmed().isEmpty()) {
        return std::nullopt;
    }
    
    Note note;
    note.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    note.title = title.trimmed();
    note.content = content.trimmed();
    note.category = category;
    note.tags = tags;
    note.createdAt = QDateTime::currentDateTime();
    note.updatedAt = QDateTime::currentDateTime();
    note.isFavorite = false;
    note.isArchived = false;
    
    m_notes.push_back(note);
    appendSearchIndexFor(note);

    if (saveNotes()) {
        emit noteCreated(note.id);
        emit notesChanged();
        return note.id;
    }

    m_notes.pop_back();
    m_searchIndex.pop_back();
    return std::nullopt;
}

bool NotesManager::updateNote(const QString& noteId, const QString& title,
                               const QString& content, const QString& category,
                               const QStringList& tags) {
    auto it = std::find_if(m_notes.begin(), m_notes.end(),
                           [&noteId](const Note& n) { return n.id == noteId; });
    
    if (it == m_notes.end()) {
        return false;
    }
    
    if (!title.isNull()) it->title = title.trimmed();
    if (!content.isNull()) it->content = content.trimmed();
    if (!category.isNull()) it->category = category;
    if (!tags.isEmpty()) it->tags = tags;
    it->updatedAt = QDateTime::currentDateTime();

    // Refresh the corresponding search-index entry in place.
    const size_t idx = static_cast<size_t>(std::distance(m_notes.begin(), it));
    SearchIndexEntry& entry = m_searchIndex[idx];
    entry.titleLower = it->title.toLower();
    entry.contentLower = it->content.toLower();
    entry.tagsLower.clear();
    entry.tagsLower.reserve(it->tags.size());
    for (const QString& tag : it->tags) {
        entry.tagsLower.append(tag.toLower());
    }

    if (saveNotes()) {
        emit noteUpdated(noteId);
        emit notesChanged();
        return true;
    }
    return false;
}

bool NotesManager::deleteNote(const QString& noteId) {
    auto it = std::find_if(m_notes.begin(), m_notes.end(),
                           [&noteId](const Note& n) { return n.id == noteId; });
    
    if (it == m_notes.end()) {
        return false;
    }

    const size_t idx = static_cast<size_t>(std::distance(m_notes.begin(), it));
    m_notes.erase(it);
    m_searchIndex.erase(m_searchIndex.begin() + idx);

    if (saveNotes()) {
        emit noteDeleted(noteId);
        emit notesChanged();
        return true;
    }
    return false;
}

std::optional<Note> NotesManager::getNoteById(const QString& noteId) const {
    auto it = std::find_if(m_notes.begin(), m_notes.end(),
                           [&noteId](const Note& n) { return n.id == noteId; });
    
    if (it != m_notes.end()) {
        return *it;
    }
    return std::nullopt;
}

std::vector<Note> NotesManager::getAllNotes(bool includeArchived) const {
    if (includeArchived) {
        return m_notes;
    }
    
    std::vector<Note> result;
    std::copy_if(m_notes.begin(), m_notes.end(), std::back_inserter(result),
                 [](const Note& n) { return !n.isArchived; });
    return result;
}

std::vector<Note> NotesManager::getNotesByCategory(const QString& category, bool includeArchived) const {
    std::vector<Note> result;
    for (const Note& note : m_notes) {
        if (note.category == category && (includeArchived || !note.isArchived)) {
            result.push_back(note);
        }
    }
    return result;
}

std::vector<Note> NotesManager::getFavoriteNotes() const {
    std::vector<Note> result;
    std::copy_if(m_notes.begin(), m_notes.end(), std::back_inserter(result),
                 [](const Note& n) { return n.isFavorite && !n.isArchived; });
    return result;
}

std::vector<Note> NotesManager::searchNotes(const QString& query, bool includeArchived) const {
    if (query.isEmpty()) {
        return getAllNotes(includeArchived);
    }

    const QString lowerQuery = query.toLower();
    std::vector<Note> result;

    // H2: the search index holds pre-lowercased title/content/tags, so each
    // comparison is a single contains() rather than a toLower()+contains() pair.
    // Index size is always in sync with m_notes (rebuilt on every mutation).
    const size_t n = m_notes.size();
    for (size_t i = 0; i < n; ++i) {
        const Note& note = m_notes[i];
        if (!includeArchived && note.isArchived) continue;

        const SearchIndexEntry& idx = m_searchIndex[i];
        if (idx.titleLower.contains(lowerQuery) ||
            idx.contentLower.contains(lowerQuery)) {
            result.push_back(note);
            continue;
        }

        for (const QString& tag : idx.tagsLower) {
            if (tag.contains(lowerQuery)) {
                result.push_back(note);
                break;
            }
        }
    }

    return result;
}

bool NotesManager::toggleFavorite(const QString& noteId) {
    auto it = std::find_if(m_notes.begin(), m_notes.end(),
                           [&noteId](const Note& n) { return n.id == noteId; });
    
    if (it == m_notes.end()) return false;
    
    it->isFavorite = !it->isFavorite;
    it->updatedAt = QDateTime::currentDateTime();
    
    return saveNotes();
}

bool NotesManager::toggleArchive(const QString& noteId) {
    auto it = std::find_if(m_notes.begin(), m_notes.end(),
                           [&noteId](const Note& n) { return n.id == noteId; });
    
    if (it == m_notes.end()) return false;
    
    it->isArchived = !it->isArchived;
    it->updatedAt = QDateTime::currentDateTime();
    
    return saveNotes();
}

const std::vector<NoteCategory>& NotesManager::getCategories() const {
    return m_categories;
}

bool NotesManager::addCategory(const QString& id, const QString& name, const QString& color) {
    // Check for duplicates
    for (const NoteCategory& cat : m_categories) {
        if (cat.id == id) return false;
    }
    
    m_categories.push_back({id, name, color});
    return saveCategories();
}

NotesStats NotesManager::getStats() const {
    NotesStats stats;
    stats.totalNotes = static_cast<int>(m_notes.size());
    
    for (const Note& note : m_notes) {
        if (!note.isArchived) stats.activeNotes++;
        if (note.isFavorite) stats.favoriteNotes++;
        if (note.isArchived) stats.archivedNotes++;
        stats.categoryStats[note.category]++;
    }
    
    return stats;
}

QString NotesManager::exportNotes(const QString& exportPath) {
    QString path = exportPath;
    if (path.isEmpty()) {
        QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
        path = m_dataDir + "/notes_export_" + timestamp + ".json";
    }
    
    QJsonObject exportData{
        {"notes", QJsonDocument::fromJson(QFile(getNotesFilePath()).readAll()).array()},
        {"categories", QJsonDocument::fromJson(QFile(getCategoriesFilePath()).readAll()).array()},
        {"exported_at", QDateTime::currentDateTime().toString(Qt::ISODate)},
        {"version", "1.0"}
    };
    
    // Rebuild arrays from current data
    QJsonArray notesArray;
    for (const Note& note : m_notes) {
        notesArray.append(note.toJson());
    }
    exportData["notes"] = notesArray;
    
    QJsonArray catsArray;
    for (const NoteCategory& cat : m_categories) {
        catsArray.append(cat.toJson());
    }
    exportData["categories"] = catsArray;
    
    QSaveFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        // Indented kept here: this is a user-facing export and readability matters.
        file.write(QJsonDocument(exportData).toJson(QJsonDocument::Indented));
        if (file.commit()) {
            return path;
        }
    }

    return QString();
}

bool NotesManager::importNotes(const QString& importPath, bool merge) {
    QFile file(importPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    // C6: reject oversized imports before reading them into memory.
    if (file.size() <= 0 || file.size() > SylvaniaConstants::kMaxImportBytes) {
        qWarning() << "Import rejeté: taille hors limites (" << file.size() << ")";
        return false;
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    file.close();

    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }

    QJsonObject importData = doc.object();

    // C6: schema check — both arrays must be present, otherwise refuse the import
    // rather than silently importing nothing or partial data.
    if (!importData.value("notes").isArray() || !importData.value("categories").isArray()) {
        qWarning() << "Import rejeté: schéma invalide (notes/categories manquants)";
        return false;
    }

    if (!merge) {
        m_notes.clear();
        m_categories.clear();
    }

    // Import categories
    const QJsonArray catsArray = importData.value("categories").toArray();
    for (const QJsonValue& val : catsArray) {
        if (!val.isObject()) continue;
        NoteCategory cat = NoteCategory::fromJson(val.toObject());
        if (cat.id.isEmpty()) continue;
        bool exists = std::any_of(m_categories.begin(), m_categories.end(),
                                  [&cat](const NoteCategory& c) { return c.id == cat.id; });
        if (!exists) {
            m_categories.push_back(cat);
        }
    }

    // Import notes
    const QJsonArray notesArray = importData.value("notes").toArray();
    for (const QJsonValue& val : notesArray) {
        if (!val.isObject()) continue;
        Note note = Note::fromJson(val.toObject());
        if (note.id.isEmpty() || note.title.isEmpty()) continue;
        if (merge) {
            // Generate new ID to avoid conflicts
            note.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        }
        m_notes.push_back(note);
    }

    // Bulk import → rebuild the cache once rather than appending per note.
    rebuildSearchIndex();

    bool success = saveNotes() && saveCategories();
    if (success) {
        emit notesChanged();
    }
    return success;
}
