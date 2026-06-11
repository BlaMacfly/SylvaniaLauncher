#pragma once

#include <QDialog>
#include <QGridLayout>
#include <QScrollArea>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QFrame>
#include <QDate>
#include <QSet>
#include <vector>

class NotesManager;
class QCalendarWidget;
class QCheckBox;
class QDateTimeEdit;

/**
 * @brief Post-it style notes window
 *
 * Features:
 * - Visual grid of colorful post-it notes
 * - Click to edit, double-click to expand
 * - Simple add/delete functionality
 * - Multiple color options
 * - v3.0: calendar pane — days carrying a note or a reminder are decorated;
 *   clicking a day filters the grid to that day's notes
 * - v3.0: per-note reminders (due date/time) firing native notifications
 */
class NotesWindow : public QDialog {
    Q_OBJECT

public:
    explicit NotesWindow(NotesManager* notesManager, QWidget* parent = nullptr);

public slots:
    // Refresh grid + calendar (e.g. after a reminder fired in background).
    void refresh();

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
    void onCalendarDayClicked(const QDate& date);
    void onShowAllClicked();

private:
    void setupUi();
    void loadNotes();
    void refreshCalendarMarks();
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

    // Calendar pane (v3.0)
    QCalendarWidget* m_calendar = nullptr;
    QLabel* m_dayInfoLabel = nullptr;
    QPushButton* m_showAllButton = nullptr;
    QDate m_filterDate;            // invalid = show everything
    QSet<QDate> m_markedDates;     // dates currently decorated (for reset)

    // Edit dialog elements
    QDialog* m_editDialog = nullptr;
    QLineEdit* m_titleEdit = nullptr;
    QTextEdit* m_contentEdit = nullptr;
    QCheckBox* m_reminderCheck = nullptr;
    QDateTimeEdit* m_dueEdit = nullptr;
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
