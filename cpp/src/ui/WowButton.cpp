#include "WowButton.h"
#include <QCursor>

WowButton::WowButton(const QString& text, QWidget* parent)
    : QPushButton(text, parent)
{
    setFixedSize(80, 80);
    setCursor(Qt::PointingHandCursor);
    updateStyle();
}

void WowButton::setNotification(bool hasNotification) {
    if (m_hasNotification != hasNotification) {
        m_hasNotification = hasNotification;
        updateStyle();
    }
}

bool WowButton::hasNotification() const {
    return m_hasNotification;
}

void WowButton::updateStyle() {
    QString notificationStyle;
    if (m_hasNotification) {
        notificationStyle = R"(
            QPushButton {
                border: 2px solid #ff9900 !important;
            }
        )";
    }

    setStyleSheet(notificationStyle + R"(
        QPushButton {
            background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #5a4a2d, stop:0.5 #3a2a1d, stop:1 #2a1a0d);
            color: #d4af37;
            border: 1px solid #7a6a4d;
            border-radius: 15px;
            padding: 8px;
            text-align: center;
            font-size: 12px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #6a5a3d, stop:0.5 #4a3a2d, stop:1 #3a2a1d);
            border: 1px solid #8a7a5d;
        }
        QPushButton:pressed {
            background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #2a1a0d, stop:0.5 #3a2a1d, stop:1 #5a4a2d);
            padding-left: 9px;
            padding-top: 9px;
        }
        QPushButton:disabled {
            background-color: #444444;
            color: #888888;
            border: 2px solid #555555;
        }
    )");
}
