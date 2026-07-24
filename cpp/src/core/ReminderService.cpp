#include "ReminderService.h"
#include "NotesManager.h"
#include "SoundManager.h"
#include "Constants.h"

#include <QSystemTrayIcon>
#include <QDateTime>

ReminderService::ReminderService(NotesManager* notes, SoundManager* sounds,
                                 QSystemTrayIcon* tray, QObject* parent)
    : QObject(parent)
    , m_notes(notes)
    , m_sounds(sounds)
    , m_tray(tray)
{
    m_timer.setInterval(SylvaniaConstants::kReminderCheckMs);
    connect(&m_timer, &QTimer::timeout, this, &ReminderService::checkDueReminders);
}

void ReminderService::start() {
    reportMissedReminders();
    m_timer.start();
}

void ReminderService::notify(const QString& title, const QString& body) {
    if (m_tray && QSystemTrayIcon::supportsMessages()) {
        m_tray->showMessage(title, body, QSystemTrayIcon::Information);
    }
    if (m_sounds) {
        m_sounds->play(QStringLiteral("notification"));
    }
}

void ReminderService::reportMissedReminders() {
    // Anything already due at startup was missed while the launcher was
    // closed: group into one notification, then mark done so the regular
    // poll doesn't re-fire them.
    const auto missed = m_notes->dueReminders(QDateTime::currentDateTime());
    if (missed.empty()) return;

    QStringList titles;
    for (const Note& note : missed) {
        titles << QStringLiteral("• %1 (%2)")
                      .arg(note.title, note.dueAt.toString(QStringLiteral("dd/MM HH:mm")));
        m_notes->markReminderDone(note.id);
    }

    notify(tr("Rappels manqués (%1)").arg(missed.size()),
           tr("Échéances passées pendant que le launcher était fermé :\n%1")
               .arg(titles.join(QLatin1Char('\n'))));
    emit remindersFired();
}

void ReminderService::checkDueReminders() {
    const auto due = m_notes->dueReminders(QDateTime::currentDateTime());
    if (due.empty()) return;

    for (const Note& note : due) {
        notify(tr("Rappel : %1").arg(note.title),
               note.content.left(200));
        m_notes->markReminderDone(note.id);
    }
    emit remindersFired();
}
