#include "ButtonStyles.h"
#include "Constants.h"

#include <QPushButton>

namespace ButtonStyles {

ThemeTokens defaultTokens() {
    ThemeTokens t;
    t.primary1 = "#4a7c3f"; t.primary2 = "#3a6a2f"; t.primary3 = "#2a5a1f";
    t.primaryBorder = "#5a8c4f";
    t.secondary1 = "#5a4a3d"; t.secondary2 = "#4a3a2d"; t.secondary3 = "#3a2a1d";
    t.secondaryBorder = "#6a5a4d";
    t.primaryText = "#ffffff";
    t.secondaryText = "#d4af37";
    t.accent = "#d4af37";
    return t;
}

namespace {

// The one QSS template shared by all three levels. Levels differ only in the
// token values and font size passed in — never in structure or states.
QString levelQss(const QString& g1, const QString& g2, const QString& g3,
                 const QString& border, const QString& text,
                 const QString& accent, int fontPx) {
    return QString(R"(
        QPushButton {
            background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 %1, stop:0.5 %2, stop:1 %3);
            color: %5;
            border: 2px solid %4;
            border-radius: %7px;
            padding: %8px %9px;
            font-size: %10px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 %4, stop:0.5 %1, stop:1 %2);
        }
        QPushButton:pressed {
            background-color: %3;
        }
        QPushButton:focus {
            border: 2px solid %6;
        }
        QPushButton:disabled {
            background-color: #3a3a3a;
            color: #8a8a8a;
            border: 2px solid #555555;
        }
    )")
        .arg(g1, g2, g3, border, text, accent)
        .arg(SylvaniaConstants::kButtonRadiusPx)
        .arg(SylvaniaConstants::kSpaceSm)
        .arg(SylvaniaConstants::kSpaceMd)
        .arg(fontPx);
}

QPushButton* makeButton(const QString& text, QWidget* parent, int minHeight) {
    auto* btn = new QPushButton(text, parent);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setMinimumHeight(minHeight);
    btn->setFocusPolicy(Qt::StrongFocus);  // keyboard navigation
    return btn;
}

}  // namespace

QPushButton* makePrimary(const QString& text, QWidget* parent) {
    auto* btn = makeButton(text, parent, SylvaniaConstants::kButtonHeightPrimary);
    applyPrimary(btn, defaultTokens());
    return btn;
}

QPushButton* makeSecondary(const QString& text, QWidget* parent) {
    auto* btn = makeButton(text, parent, SylvaniaConstants::kButtonHeightSecondary);
    applySecondary(btn, defaultTokens());
    return btn;
}

QPushButton* makeTertiary(const QString& text, QWidget* parent) {
    auto* btn = makeButton(text, parent, SylvaniaConstants::kButtonHeightTertiary);
    applyTertiary(btn, defaultTokens());
    return btn;
}

void applyPrimary(QPushButton* btn, const ThemeTokens& t) {
    if (!btn) return;
    btn->setStyleSheet(levelQss(t.primary1, t.primary2, t.primary3,
                                t.primaryBorder, t.primaryText, t.accent, 16));
}

void applySecondary(QPushButton* btn, const ThemeTokens& t) {
    if (!btn) return;
    btn->setStyleSheet(levelQss(t.secondary1, t.secondary2, t.secondary3,
                                t.secondaryBorder, t.secondaryText, t.accent, 12));
}

void applyTertiary(QPushButton* btn, const ThemeTokens& t) {
    if (!btn) return;
    btn->setStyleSheet(levelQss(t.secondary1, t.secondary2, t.secondary3,
                                t.secondaryBorder, t.secondaryText, t.accent, 10));
}

}  // namespace ButtonStyles
