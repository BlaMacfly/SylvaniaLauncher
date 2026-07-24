#pragma once

#include <QPushButton>

/**
 * @brief Primary action button — a 3-state machine driven by the active
 *        edition's client detection (v3.0, Lot 2).
 *
 *  - Install:     client exe not found at the edition's path → "INSTALLER",
 *                 clicking starts the DownloadDialog flow.
 *  - Downloading: download/extraction in progress → label shows progress,
 *                 button disabled.
 *  - Play:        client detected → "JOUER", clicking launches the game.
 *
 * The visual style is owned by ButtonStyles (primary level); this class only
 * manages state, label and enabled-ness, so it never fights the central QSS.
 */
class WowButton : public QPushButton {
    Q_OBJECT

public:
    enum class State { Install, Downloading, Play };

    explicit WowButton(QWidget* parent = nullptr);
    ~WowButton() override = default;

    void setState(State state);
    State state() const { return m_state; }

    // Updates the label while in Downloading state. percent < 0 = unknown
    // (extraction / indeterminate phase).
    void setDownloadProgress(int percent);

    // Refresh the label after a language change.
    void retranslate();

private:
    void updateLabel();

    State m_state = State::Install;
    int m_progress = -1;
};
