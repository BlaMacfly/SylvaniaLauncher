#include "NotesWindow.h"
#include "NotesManager.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QCloseEvent>
#include <QScrollBar>
#include <QGraphicsDropShadowEffect>
#include <QFont>

NotesWindow::NotesWindow(NotesManager* notesManager, QWidget* parent)
    : QDialog(parent)
    , m_notesManager(notesManager)
{
    setWindowTitle("📝 Notes - Sylvania Launcher");
    setMinimumSize(800, 600);
    resize(900, 650);
    
    setupUi();
    applyStyle();
    loadNotes();
}

void NotesWindow::setupUi() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    
    // Header with title and add button
    QHBoxLayout* headerLayout = new QHBoxLayout();
    
    QLabel* titleLabel = new QLabel("📌 Mes Notes", this);
    titleLabel->setStyleSheet("color: #d4af37; font-size: 24px; font-weight: bold; border: none;");
    
    m_countLabel = new QLabel("0 notes", this);
    m_countLabel->setStyleSheet("color: #888888; font-size: 14px; border: none;");
    
    m_addButton = new QPushButton("+ Nouvelle Note", this);
    m_addButton->setMinimumSize(150, 45);
    m_addButton->setCursor(Qt::PointingHandCursor);
    m_addButton->setStyleSheet(R"(
        QPushButton {
            background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #4a7c3f, stop:0.5 #3a6a2f, stop:1 #2a5a1f);
            color: #ffffff;
            border: 2px solid #5a8c4f;
            border-radius: 10px;
            padding: 10px 20px;
            font-size: 14px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #5a8c4f, stop:0.5 #4a7c3f, stop:1 #3a6a2f);
        }
    )");
    
    headerLayout->addWidget(titleLabel);
    headerLayout->addWidget(m_countLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(m_addButton);
    mainLayout->addLayout(headerLayout);
    
    // Scroll area for notes grid
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setStyleSheet("QScrollArea { background: transparent; border: none; }");
    
    m_notesContainer = new QWidget();
    m_notesContainer->setStyleSheet("background: transparent;");
    m_notesGrid = new QGridLayout(m_notesContainer);
    m_notesGrid->setSpacing(20);
    m_notesGrid->setContentsMargins(10, 10, 10, 10);
    
    m_scrollArea->setWidget(m_notesContainer);
    mainLayout->addWidget(m_scrollArea, 1);
    
    // Connect signals
    connect(m_addButton, &QPushButton::clicked, this, &NotesWindow::onAddNote);
}

void NotesWindow::applyStyle() {
    setStyleSheet(R"(
        QDialog {
            background-color: #1a1a1a;
        }
    )");
}

void NotesWindow::loadNotes() {
    // Clear existing post-its
    QLayoutItem* item;
    while ((item = m_notesGrid->takeAt(0)) != nullptr) {
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }
    
    auto notes = m_notesManager->getAllNotes();
    int row = 0, col = 0;
    const int maxCols = 3;
    
    for (const auto& note : notes) {
        QString color = note.category.isEmpty() ? COLORS[0] : note.category;
        // Validate color or use default
        bool validColor = false;
        for (const char* c : COLORS) {
            if (color == c) { validColor = true; break; }
        }
        if (!validColor) color = COLORS[0];
        
        createPostIt(note.id, note.title, note.content, color, row, col);
        
        col++;
        if (col >= maxCols) {
            col = 0;
            row++;
        }
    }
    
    // Add stretch at bottom
    m_notesGrid->setRowStretch(row + 1, 1);
    
    m_countLabel->setText(QString("%1 note%2").arg(notes.size()).arg(notes.size() != 1 ? "s" : ""));
}

void NotesWindow::createPostIt(const QString& id, const QString& title, 
                               const QString& content, const QString& color, int row, int col) {
    QFrame* postIt = new QFrame(m_notesContainer);
    postIt->setObjectName(id);
    postIt->setFixedSize(260, 240);
    postIt->setCursor(Qt::PointingHandCursor);
    
    // Calculate text color based on background brightness
    QString textColor = "#333333";
    
    postIt->setStyleSheet(QString(R"(
        QFrame {
            background-color: %1;
            border-radius: 5px;
            border: none;
        }
        QFrame:hover {
            border: 2px solid #d4af37;
        }
    )").arg(color));
    
    // Add shadow effect
    QGraphicsDropShadowEffect* shadow = new QGraphicsDropShadowEffect(postIt);
    shadow->setBlurRadius(15);
    shadow->setOffset(3, 3);
    shadow->setColor(QColor(0, 0, 0, 100));
    postIt->setGraphicsEffect(shadow);
    
    QVBoxLayout* layout = new QVBoxLayout(postIt);
    layout->setContentsMargins(15, 12, 15, 12);
    layout->setSpacing(6);
    
    // Title
    QLabel* titleLabel = new QLabel(title.isEmpty() ? "Sans titre" : title, postIt);
    titleLabel->setStyleSheet(QString("color: %1; font-size: 14px; font-weight: bold; background: transparent; border: none;").arg(textColor));
    titleLabel->setWordWrap(true);
    titleLabel->setMaximumHeight(40);
    layout->addWidget(titleLabel);
    
    // Content preview - show more text
    QString preview = content.left(250);
    if (content.length() > 250) preview += "...";
    
    QLabel* contentLabel = new QLabel(preview, postIt);
    contentLabel->setStyleSheet(QString("color: %1; font-size: 11px; background: transparent; border: none; line-height: 1.3;").arg(textColor));
    contentLabel->setWordWrap(true);
    contentLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    layout->addWidget(contentLabel, 1);
    
    // Bottom row with character count
    QHBoxLayout* bottomLayout = new QHBoxLayout();
    
    // Character count indicator
    QLabel* countLabel = new QLabel(QString("%1 car.").arg(content.length()), postIt);
    countLabel->setStyleSheet(QString("color: %1; font-size: 9px; background: transparent; border: none;").arg(textColor));
    bottomLayout->addWidget(countLabel);
    
    bottomLayout->addStretch();
    
    // Edit button
    QPushButton* editBtn = new QPushButton("✏️ Modifier", postIt);
    editBtn->setFixedHeight(28);
    editBtn->setCursor(Qt::PointingHandCursor);
    editBtn->setStyleSheet(R"(
        QPushButton {
            background-color: rgba(100, 100, 100, 180);
            color: white;
            border-radius: 5px;
            font-size: 10px;
            padding: 5px 10px;
            border: none;
        }
        QPushButton:hover {
            background-color: rgba(120, 120, 120, 220);
        }
    )");
    bottomLayout->addWidget(editBtn);
    
    layout->addLayout(bottomLayout);
    
    // Delete button (small X in corner)
    QPushButton* deleteBtn = new QPushButton("×", postIt);
    deleteBtn->setFixedSize(25, 25);
    deleteBtn->setCursor(Qt::PointingHandCursor);
    deleteBtn->setStyleSheet(R"(
        QPushButton {
            background-color: rgba(200, 50, 50, 180);
            color: white;
            border-radius: 12px;
            font-size: 16px;
            font-weight: bold;
            border: none;
        }
        QPushButton:hover {
            background-color: rgba(255, 50, 50, 220);
        }
    )");
    deleteBtn->move(230, 5);
    
    // Connect edit button
    connect(editBtn, &QPushButton::clicked, [this, id]() {
        m_editingNoteId = id;
        auto noteOpt = m_notesManager->getNoteById(id);
        if (noteOpt) {
            m_selectedColor = noteOpt->category.isEmpty() ? COLORS[0] : noteOpt->category;
        }
        showEditDialog(id);
    });
    
    // Connect delete button
    connect(deleteBtn, &QPushButton::clicked, [this, id]() {
        onDeleteNote(id);
    });
    
    // Make the whole post-it clickable for viewing full content
    postIt->installEventFilter(this);
    postIt->setProperty("noteId", id);
    postIt->setProperty("noteTitle", title);
    postIt->setProperty("noteContent", content);
    postIt->setProperty("noteColor", color);
    
    // Connect click
    postIt->setProperty("noteId", id);
    
    m_notesGrid->addWidget(postIt, row, col);
    
    // Make post-it clickable
    connect(postIt, &QFrame::destroyed, [](){}); // Dummy to enable connect
    
    // Actually handle click via event filter workaround - simpler: use mouse tracking
    postIt->setMouseTracking(true);
}

bool NotesWindow::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::MouseButtonPress) {
        QFrame* frame = qobject_cast<QFrame*>(obj);
        if (frame) {
            QString noteId = frame->property("noteId").toString();
            if (!noteId.isEmpty()) {
                onNoteClicked(noteId);
                return true;
            }
        }
    }
    return QDialog::eventFilter(obj, event);
}

void NotesWindow::onAddNote() {
    m_editingNoteId.clear();
    m_selectedColor = COLORS[0];
    showEditDialog();
}

void NotesWindow::onNoteClicked(const QString& noteId) {
    auto noteOpt = m_notesManager->getNoteById(noteId);
    if (!noteOpt) return;
    
    const Note& note = *noteOpt;
    QString color = note.category.isEmpty() ? COLORS[0] : note.category;
    QString textColor = "#333333";
    
    // Create view dialog
    QDialog* viewDialog = new QDialog(this);
    viewDialog->setWindowTitle("📄 " + note.title);
    viewDialog->setMinimumSize(500, 450);
    viewDialog->setStyleSheet(QString(R"(
        QDialog {
            background-color: %1;
        }
        QLabel {
            color: %2;
            border: none;
        }
        QTextEdit {
            background-color: transparent;
            color: %2;
            border: none;
            font-size: 14px;
        }
    )").arg(color).arg(textColor));
    
    QVBoxLayout* layout = new QVBoxLayout(viewDialog);
    layout->setSpacing(15);
    layout->setContentsMargins(25, 25, 25, 25);
    
    // Title
    QLabel* titleLabel = new QLabel(note.title, viewDialog);
    titleLabel->setStyleSheet(QString("color: %1; font-size: 22px; font-weight: bold;").arg(textColor));
    titleLabel->setWordWrap(true);
    layout->addWidget(titleLabel);
    
    // Separator
    QFrame* separator = new QFrame(viewDialog);
    separator->setFrameShape(QFrame::HLine);
    separator->setStyleSheet(QString("background-color: %1;").arg(textColor));
    separator->setFixedHeight(2);
    layout->addWidget(separator);
    
    // Content (read-only text edit for scrolling)
    QTextEdit* contentView = new QTextEdit(viewDialog);
    contentView->setPlainText(note.content);
    contentView->setReadOnly(true);
    contentView->setStyleSheet(QString("color: %1; font-size: 14px; background: transparent; border: none;").arg(textColor));
    layout->addWidget(contentView, 1);
    
    // Info and buttons
    QHBoxLayout* bottomLayout = new QHBoxLayout();
    
    QLabel* dateLabel = new QLabel(QString("Créée le: %1").arg(note.createdAt.toString("dd/MM/yyyy HH:mm")), viewDialog);
    dateLabel->setStyleSheet(QString("color: %1; font-size: 11px;").arg(textColor));
    bottomLayout->addWidget(dateLabel);
    
    bottomLayout->addStretch();
    
    // Edit button
    QPushButton* editBtnDialog = new QPushButton("✏️ Modifier", viewDialog);
    editBtnDialog->setMinimumSize(100, 35);
    editBtnDialog->setCursor(Qt::PointingHandCursor);
    editBtnDialog->setStyleSheet(R"(
        QPushButton {
            background-color: rgba(80, 80, 80, 200);
            color: white;
            border-radius: 5px;
            padding: 8px 15px;
            font-size: 12px;
        }
        QPushButton:hover {
            background-color: rgba(100, 100, 100, 220);
        }
    )");
    
    // Close button
    QPushButton* closeBtn = new QPushButton("Fermer", viewDialog);
    closeBtn->setMinimumSize(80, 35);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setStyleSheet(R"(
        QPushButton {
            background-color: rgba(60, 60, 60, 200);
            color: white;
            border-radius: 5px;
            padding: 8px 15px;
            font-size: 12px;
        }
        QPushButton:hover {
            background-color: rgba(80, 80, 80, 220);
        }
    )");
    
    bottomLayout->addWidget(editBtnDialog);
    bottomLayout->addWidget(closeBtn);
    layout->addLayout(bottomLayout);
    
    // Connect buttons
    connect(closeBtn, &QPushButton::clicked, viewDialog, &QDialog::accept);
    connect(editBtnDialog, &QPushButton::clicked, [this, viewDialog, noteId]() {
        viewDialog->accept();
        m_editingNoteId = noteId;
        auto noteOpt = m_notesManager->getNoteById(noteId);
        if (noteOpt) {
            m_selectedColor = noteOpt->category.isEmpty() ? COLORS[0] : noteOpt->category;
        }
        showEditDialog(noteId);
    });
    
    viewDialog->exec();
    viewDialog->deleteLater();
}

void NotesWindow::onDeleteNote(const QString& noteId) {
    auto result = QMessageBox::question(this, "Supprimer",
        "Supprimer cette note ?",
        QMessageBox::Yes | QMessageBox::No);
    
    if (result == QMessageBox::Yes) {
        m_notesManager->deleteNote(noteId);
        loadNotes();
    }
}

void NotesWindow::showEditDialog(const QString& noteId) {
    m_editDialog = new QDialog(this);
    m_editDialog->setWindowTitle(noteId.isEmpty() ? "Nouvelle Note" : "Modifier la Note");
    m_editDialog->setFixedSize(450, 400);
    m_editDialog->setStyleSheet(R"(
        QDialog {
            background-color: #1a1a1a;
        }
        QLabel {
            color: #d4af37;
            font-size: 13px;
            border: none;
        }
        QLineEdit {
            background-color: #2a2a2a;
            color: #ffffff;
            border: 1px solid #5a4a2d;
            border-radius: 5px;
            padding: 10px;
            font-size: 14px;
        }
        QTextEdit {
            background-color: #2a2a2a;
            color: #ffffff;
            border: 1px solid #5a4a2d;
            border-radius: 5px;
            padding: 10px;
            font-size: 13px;
        }
    )");
    
    QVBoxLayout* layout = new QVBoxLayout(m_editDialog);
    layout->setSpacing(15);
    layout->setContentsMargins(25, 25, 25, 25);
    
    // Title
    layout->addWidget(new QLabel("Titre:", m_editDialog));
    m_titleEdit = new QLineEdit(m_editDialog);
    m_titleEdit->setPlaceholderText("Titre de la note...");
    layout->addWidget(m_titleEdit);
    
    // Content
    layout->addWidget(new QLabel("Contenu:", m_editDialog));
    m_contentEdit = new QTextEdit(m_editDialog);
    m_contentEdit->setPlaceholderText("Écrivez votre note ici...");
    layout->addWidget(m_contentEdit, 1);
    
    // Color selection
    layout->addWidget(new QLabel("Couleur:", m_editDialog));
    QHBoxLayout* colorLayout = new QHBoxLayout();
    colorLayout->setSpacing(10);
    
    m_colorButtons.clear();
    for (const char* color : COLORS) {
        QPushButton* colorBtn = new QPushButton(m_editDialog);
        colorBtn->setFixedSize(35, 35);
        colorBtn->setCursor(Qt::PointingHandCursor);
        colorBtn->setProperty("color", QString(color));
        
        bool isSelected = (QString(color) == m_selectedColor);
        colorBtn->setStyleSheet(QString(R"(
            QPushButton {
                background-color: %1;
                border-radius: 17px;
                border: %2;
            }
            QPushButton:hover {
                border: 3px solid #d4af37;
            }
        )").arg(color).arg(isSelected ? "3px solid #d4af37" : "2px solid #555555"));
        
        connect(colorBtn, &QPushButton::clicked, [this, color, colorBtn]() {
            onColorSelected(QString(color));
            // Update all buttons
            for (auto* btn : m_colorButtons) {
                QString btnColor = btn->property("color").toString();
                bool sel = (btnColor == m_selectedColor);
                btn->setStyleSheet(QString(R"(
                    QPushButton {
                        background-color: %1;
                        border-radius: 17px;
                        border: %2;
                    }
                    QPushButton:hover {
                        border: 3px solid #d4af37;
                    }
                )").arg(btnColor).arg(sel ? "3px solid #d4af37" : "2px solid #555555"));
            }
        });
        
        m_colorButtons.push_back(colorBtn);
        colorLayout->addWidget(colorBtn);
    }
    colorLayout->addStretch();
    layout->addLayout(colorLayout);
    
    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    
    QPushButton* cancelBtn = new QPushButton("Annuler", m_editDialog);
    cancelBtn->setMinimumSize(100, 40);
    cancelBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #3a3a3a;
            color: #ffffff;
            border: 1px solid #5a5a5a;
            border-radius: 5px;
            padding: 10px 25px;
            font-size: 13px;
        }
        QPushButton:hover {
            background-color: #4a4a4a;
        }
    )");
    
    QPushButton* saveBtn = new QPushButton("Sauvegarder", m_editDialog);
    saveBtn->setMinimumSize(120, 40);
    saveBtn->setStyleSheet(R"(
        QPushButton {
            background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #5a4a2d, stop:0.5 #3a2a1d, stop:1 #2a1a0d);
            color: #d4af37;
            border: 1px solid #7a6a4d;
            border-radius: 5px;
            padding: 10px 25px;
            font-weight: bold;
            font-size: 13px;
        }
        QPushButton:hover {
            background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #6a5a3d, stop:0.5 #4a3a2d, stop:1 #3a2a1d);
        }
    )");
    
    buttonLayout->addWidget(cancelBtn);
    buttonLayout->addWidget(saveBtn);
    layout->addLayout(buttonLayout);
    
    // Load existing note data
    if (!noteId.isEmpty()) {
        auto noteOpt = m_notesManager->getNoteById(noteId);
        if (noteOpt) {
            m_titleEdit->setText(noteOpt->title);
            m_contentEdit->setPlainText(noteOpt->content);
        }
    }
    
    connect(cancelBtn, &QPushButton::clicked, m_editDialog, &QDialog::reject);
    connect(saveBtn, &QPushButton::clicked, this, &NotesWindow::onSaveNote);
    
    m_editDialog->exec();
    m_editDialog->deleteLater();
    m_editDialog = nullptr;
}

void NotesWindow::onColorSelected(const QString& color) {
    m_selectedColor = color;
}

void NotesWindow::onSaveNote() {
    QString title = m_titleEdit->text().trimmed();
    QString content = m_contentEdit->toPlainText().trimmed();
    
    if (title.isEmpty() && content.isEmpty()) {
        QMessageBox::warning(this, "Erreur", "La note ne peut pas être vide.");
        return;
    }
    
    if (title.isEmpty()) {
        title = content.left(30);
        if (content.length() > 30) title += "...";
    }
    
    if (m_editingNoteId.isEmpty()) {
        // Create new note - use color as category to store it
        m_notesManager->createNote(title, content, m_selectedColor);
    } else {
        // Update existing note
        m_notesManager->updateNote(m_editingNoteId, title, content, m_selectedColor);
    }
    
    if (m_editDialog) {
        m_editDialog->accept();
    }
    loadNotes();
}

void NotesWindow::onCancelEdit() {
    if (m_editDialog) {
        m_editDialog->reject();
    }
}

void NotesWindow::closeEvent(QCloseEvent* event) {
    QDialog::closeEvent(event);
}
