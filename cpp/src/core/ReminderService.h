#pragma once

#include <QObject>
#include <QTimer>

class NotesManager;
class SoundManager;
class QSystemTrayIcon;

/**
 * @brief Watches note reminders and fires native notifications (v3.0, Lot 3).
 *
 * A QTimer (SylvaniaConstants::kReminderCheckMs) polls NotesManager for due
 * reminders; each one triggers a Windows toast via QSystemTrayIcon::showMessage
 * plus the notification sound (SoundManager), then is marked done so it never
 * fires twice. Reminders that came due while the launcher was closed are
 * reported once at startup as "missed" (grouped in a single notification).
 *
 * The service works while the launcher sits in the background: the tray icon
 * lives in MainWindow and stays active for the whole app lifetime.
 */
class ReminderService : public QObject {
    Q_OBJECT

public:
    ReminderService(NotesManager* notes, SoundManager* sounds,
                    QSystemTrayIcon* tray, QObject* parent = nullptr);

    // Starts the polling timer and immediately reports reminders missed
    // while the launcher was closed.
    void start();

signals:
    // Emitted whenever at least one reminder fired (lets the UI refresh).
    void remindersFired();

private slots:
    void checkDueReminders();

private:
    void notify(const QString& title, const QString& body);
    void reportMissedReminders();

    NotesManager* m_notes;
    SoundManager* m_sounds;
    QSystemTrayIcon* m_tray;
    QTimer m_timer;
};
