#include "WowButton.h"

WowButton::WowButton(QWidget* parent)
    : QPushButton(parent)
{
    setCursor(Qt::PointingHandCursor);
    updateLabel();
}

void WowButton::setState(State state) {
    m_state = state;
    if (m_state != State::Downloading) {
        m_progress = -1;
    }
    // Authoritative on enabled-ness: callers may have disabled the button for
    // an unrelated operation (e.g. an HD-patch install disables it directly),
    // so re-assert it on every setState — even when the state is unchanged —
    // otherwise the button could stay greyed out after such an operation ends.
    setEnabled(m_state != State::Downloading);
    updateLabel();
}

void WowButton::setDownloadProgress(int percent) {
    m_progress = percent;
    if (m_state == State::Downloading) {
        updateLabel();
    }
}

void WowButton::retranslate() {
    updateLabel();
}

void WowButton::updateLabel() {
    switch (m_state) {
    case State::Install:
        setText(tr("INSTALLER"));
        break;
    case State::Downloading:
        if (m_progress >= 0) {
            setText(tr("TÉLÉCHARGEMENT… %1%").arg(m_progress));
        } else {
            setText(tr("INSTALLATION…"));
        }
        break;
    case State::Play:
        setText(tr("JOUER"));
        break;
    }
}
