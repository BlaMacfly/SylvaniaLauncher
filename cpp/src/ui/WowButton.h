#pragma once

#include <QPushButton>

/**
 * @brief Custom WoW-styled button
 * 
 * A push button styled to match World of Warcraft's UI aesthetic
 * with gold/brown gradient, hover effects, and notification indicators.
 */
class WowButton : public QPushButton {
    Q_OBJECT

public:
    explicit WowButton(const QString& text, QWidget* parent = nullptr);
    ~WowButton() override = default;

    /**
     * @brief Set notification indicator state
     * @param hasNotification True to show notification highlight
     */
    void setNotification(bool hasNotification);

    /**
     * @brief Check if button has notification
     * @return True if notification is active
     */
    bool hasNotification() const;

private:
    void updateStyle();

    bool m_hasNotification = false;
};
