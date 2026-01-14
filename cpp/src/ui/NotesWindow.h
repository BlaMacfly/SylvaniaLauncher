#pragma once

#include <QDialog>
#include <QGridLayout>
#include <QScrollArea>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QFrame>
#include <vector>

class NotesManager;

/**
 * @brief Post-it style notes window
 * 
 * Features:
 * - Visual grid of colorful post-it notes
 * - Click to edit, double-click to expand
 * - Simple add/delete functionality
 * - Multiple color options
 * - Drag to reorder (future)
 */
class NotesWindow : public QDialog {
    Q_OBJECT

public:
    explicit NotesWindow(NotesManager* notesManager, QWidget* parent = nullptr);

protected:
    void closeEvent(QCloseEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void onAddNote();
    void onNoteClicked(const QString& noteId);
    void onDeleteNote(const QString& noteId);
    void onSaveNote();
    void onCancelEdit();
    void onColorSelected(const QString& color);

private:
    void setupUi();
    void loadNotes();
    void createPostIt(const QString& id, const QString& title, 
                      const QString& content, const QString& color, int row, int col);
    void showEditDialog(const QString& noteId = "");
    void applyStyle();

    NotesManager* m_notesManager;
    
    // Main UI
    QScrollArea* m_scrollArea = nullptr;
    QWidget* m_notesContainer = nullptr;
    QGridLayout* m_notesGrid = nullptr;
    QPushButton* m_addButton = nullptr;
    QLabel* m_countLabel = nullptr;
    
    // Edit dialog elements
    QDialog* m_editDialog = nullptr;
    QLineEdit* m_titleEdit = nullptr;
    QTextEdit* m_contentEdit = nullptr;
    QString m_editingNoteId;
    QString m_selectedColor = "#FFE066"; // Default yellow
    
    // Color buttons
    std::vector<QPushButton*> m_colorButtons;
    
    // Post-it colors
    static constexpr const char* COLORS[] = {
        "#FFE066", // Yellow
        "#FF9F80", // Orange/Coral
        "#80D4FF", // Light Blue
        "#98FB98", // Pale Green
        "#DDA0DD", // Plum/Purple
        "#FFB6C1", // Light Pink
    };
};
