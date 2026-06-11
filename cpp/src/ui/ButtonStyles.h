#pragma once

#include <QString>

class QPushButton;
class QWidget;

/**
 * @brief Single source of truth for the MAIN WINDOW button system (v3.0 UI
 *        overhaul).
 *
 * Every button on the main launcher window belongs to exactly one of three
 * levels and uses the ONE template of that level — no one-off stylesheets in
 * MainWindow:
 *
 *  - Primary:   the main action (Jouer/Installer). One per screen.
 *  - Secondary: navigation + common actions (Réglages, AddOns, Notes,
 *               Serveurs, Patch HD, bascule d'édition, Quitter).
 *  - Tertiary:  small toggles / icon-only controls (language switch).
 *               Icon-only buttons must carry a tooltip.
 *
 * Hover / pressed / disabled / focus states are defined once per level and
 * are identical for every button of that level. Colors come from the theme
 * tokens (edition accent + dynamic background palette), sizes and radius from
 * SylvaniaConstants.
 *
 * Scope note: secondary windows opened as separate dialogs (Notes, Download,
 * Settings, AddOns, Realmlist) still carry their own local stylesheets — they
 * are self-contained and not part of the main window's button hierarchy.
 * Routing them through this system is a deliberate follow-up, not a v3.0
 * requirement.
 */
namespace ButtonStyles {

/**
 * @brief Theme tokens driving the three button templates.
 *
 * Filled by MainWindow::applyTheme() from the dynamic background palette and
 * the active edition's accent. Only these values change between themes and
 * editions — the templates (layout, sizes, states) never do.
 */
struct ThemeTokens {
    // Primary gradient (top, middle, bottom) + border.
    QString primary1, primary2, primary3, primaryBorder;
    // Secondary gradient + border.
    QString secondary1, secondary2, secondary3, secondaryBorder;
    // Text colors.
    QString primaryText;
    QString secondaryText;
    // Edition/theme accent, used for the focus ring of every level.
    QString accent;
};

/** Sensible dark-theme defaults (WoW gold accent). */
ThemeTokens defaultTokens();

// Factories: create a button already carrying the level's geometry
// (min height, cursor, focus policy). Style is applied via the apply* calls.
QPushButton* makePrimary(const QString& text, QWidget* parent);
QPushButton* makeSecondary(const QString& text, QWidget* parent);
QPushButton* makeTertiary(const QString& text, QWidget* parent);

// Re-apply the level template with new tokens (called on every theme or
// edition change). Safe on null pointers.
void applyPrimary(QPushButton* btn, const ThemeTokens& tokens);
void applySecondary(QPushButton* btn, const ThemeTokens& tokens);
void applyTertiary(QPushButton* btn, const ThemeTokens& tokens);

}  // namespace ButtonStyles
