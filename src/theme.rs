//! Visual theme: per-background palettes (ported 1:1 from the Qt launcher),
//! button tokens, fonts and the dark style used by every secondary window.

use crate::constants;
use eframe::egui::{self, Color32, FontFamily, FontId, RichText};

/// Panel chrome + the two button gradients for one background.
#[derive(Clone, Copy, Debug)]
pub struct Palette {
    pub panel_bg: Color32,
    pub panel_border: Color32,
    pub text_primary: Color32,
    pub text_secondary: Color32,
    /// Primary button gradient (top, middle, bottom) + border.
    pub p: [Color32; 3],
    pub p_border: Color32,
    /// Secondary button gradient + border.
    pub s: [Color32; 3],
    pub s_border: Color32,
}

/// Colours consumed by the three button levels (see ui::widgets).
#[derive(Clone, Copy, Debug)]
pub struct ThemeTokens {
    pub primary: [Color32; 3],
    pub primary_border: Color32,
    pub secondary: [Color32; 3],
    pub secondary_border: Color32,
    pub primary_text: Color32,
    pub secondary_text: Color32,
    pub accent: Color32,
}

/// The active theme of the main window (background name + derived colours).
#[derive(Clone, Debug)]
pub struct Theme {
    pub background: String,
    pub palette: Palette,
    pub tokens: ThemeTokens,
}

impl Theme {
    pub fn for_background(bg: &str) -> Self {
        let palette = palette_for(bg);
        let accent = hex(constants::ACCENT_COLOR);
        let tokens = ThemeTokens {
            primary: palette.p,
            primary_border: palette.p_border,
            secondary: palette.s,
            secondary_border: palette.s_border,
            primary_text: Color32::WHITE,
            secondary_text: if bg == "Azeroth" {
                Color32::WHITE
            } else {
                accent
            },
            accent,
        };
        Theme {
            background: bg.to_string(),
            palette,
            tokens,
        }
    }
}

impl Default for ThemeTokens {
    /// Dark-theme defaults (WoW gold accent) for secondary windows.
    fn default() -> Self {
        ThemeTokens {
            primary: [hex("#4a7c3f"), hex("#3a6a2f"), hex("#2a5a1f")],
            primary_border: hex("#5a8c4f"),
            secondary: [hex("#5a4a3d"), hex("#4a3a2d"), hex("#3a2a1d")],
            secondary_border: hex("#6a5a4d"),
            primary_text: Color32::WHITE,
            secondary_text: hex(constants::ACCENT_COLOR),
            accent: hex(constants::ACCENT_COLOR),
        }
    }
}

/// Parses "#rrggbb" or "rgba(r, g, b, a)" (alpha 0-255). Invalid → magenta.
pub fn hex(s: &str) -> Color32 {
    let s = s.trim();
    if let Some(inner) = s.strip_prefix("rgba(").and_then(|r| r.strip_suffix(')')) {
        let v: Vec<u8> = inner
            .split(',')
            .filter_map(|p| p.trim().parse().ok())
            .collect();
        if v.len() == 4 {
            return Color32::from_rgba_unmultiplied(v[0], v[1], v[2], v[3]);
        }
    } else if let Some(h) = s.strip_prefix('#')
        && h.len() == 6
        && let Ok(n) = u32::from_str_radix(h, 16)
    {
        return Color32::from_rgb((n >> 16) as u8, (n >> 8) as u8, n as u8);
    }
    Color32::from_rgb(255, 0, 255)
}

fn pal(c: [&str; 12]) -> Palette {
    Palette {
        panel_bg: hex(c[0]),
        panel_border: hex(c[1]),
        text_primary: hex(c[2]),
        text_secondary: hex(c[3]),
        p: [hex(c[4]), hex(c[5]), hex(c[6])],
        p_border: hex(c[7]),
        s: [hex(c[8]), hex(c[9]), hex(c[10])],
        s_border: hex(c[11]),
    }
}

pub fn palette_for(bg: &str) -> Palette {
    match bg {
        "Alliance" => pal([
            "rgba(0, 20, 50, 180)",
            "#15479a",
            "#ffffff",
            "#7ec8e3",
            "#1e5ab8",
            "#15479a",
            "#0f326a",
            "#3a7de4",
            "#3a4a5a",
            "#2a3a4a",
            "#1a2a3a",
            "#5a6a7a",
        ]),
        "Horde" => pal([
            "rgba(40, 0, 0, 180)",
            "#6a0dad",
            "#e0e0e0",
            "#ff6b6b",
            "#8b0000",
            "#660000",
            "#440000",
            "#b22222",
            "#4a2d2d",
            "#3a1d1d",
            "#2a0d0d",
            "#6a4d4d",
        ]),
        "Arbre de Vie" => pal([
            "rgba(10, 40, 10, 180)",
            "#2e8b57",
            "#f0fff0",
            "#98fb98",
            "#2e8b57",
            "#228b22",
            "#006400",
            "#3cb371",
            "#5a4a2d",
            "#4a3a1d",
            "#3a2a0d",
            "#7a6a4d",
        ]),
        "Ilidan" => pal([
            "rgba(20, 0, 30, 180)",
            "#32cd32",
            "#e6e6fa",
            "#7fff00",
            "#32cd32",
            "#228b22",
            "#006400",
            "#7fff00",
            "#3a2d4a",
            "#2a1d3a",
            "#1a0d2a",
            "#5a4d6a",
        ]),
        "Lich King" => pal([
            "rgba(0, 10, 20, 180)",
            "#4682b4",
            "#f0f8ff",
            "#87cefa",
            "#4682b4",
            "#5f9ea0",
            "#1e90ff",
            "#87ceeb",
            "#4a5a6a",
            "#3a4a5a",
            "#2a3a4a",
            "#6a7a8a",
        ]),
        "Ragnaros" => pal([
            "rgba(30, 10, 0, 180)",
            "#ff4500",
            "#ffebcd",
            "#ff8c00",
            "#ff4500",
            "#cd5c5c",
            "#8b0000",
            "#ff6347",
            "#5a2d1d",
            "#4a1d0d",
            "#3a0d00",
            "#7a4d3d",
        ]),
        "Taverne" => pal([
            "rgba(0, 0, 0, 150)",
            "#5a4a2d",
            "#ffffff",
            "#7ec8e3",
            "#4a7c3f",
            "#3a6a2f",
            "#2a5a1f",
            "#5a8c4f",
            "#5a4a3d",
            "#4a3a2d",
            "#3a2a1d",
            "#6a5a4d",
        ]),
        // Azeroth or default
        _ => pal([
            "rgba(230, 210, 170, 200)",
            "#5a4a2d",
            "#3a2a1d",
            "#5a4a2d",
            "#4a7c3f",
            "#3a6a2f",
            "#2a5a1f",
            "#5a8c4f",
            "#5a4a3d",
            "#4a3a2d",
            "#3a2a1d",
            "#6a5a4d",
        ]),
    }
}

// --- Fonts --------------------------------------------------------------------

/// Name of the bold font family registered by [`install_fonts`].
pub const BOLD: &str = "bold";

/// Uses Segoe UI (regular + bold) from Windows when available, keeping egui's
/// bundled fonts as fallback (emoji, symbols).
pub fn install_fonts(ctx: &egui::Context) {
    let mut fonts = egui::FontDefinitions::default();
    let windir = std::env::var("WINDIR").unwrap_or_else(|_| "C:\\Windows".into());
    let load = |file: &str| std::fs::read(format!("{windir}\\Fonts\\{file}")).ok();

    let fallbacks = fonts
        .families
        .get(&FontFamily::Proportional)
        .cloned()
        .unwrap_or_default();
    if let Some(regular) = load("segoeui.ttf") {
        fonts
            .font_data
            .insert("segoe".into(), egui::FontData::from_owned(regular).into());
        fonts
            .families
            .entry(FontFamily::Proportional)
            .or_default()
            .insert(0, "segoe".into());
    }
    let mut bold_family = Vec::new();
    if let Some(bold) = load("segoeuib.ttf") {
        fonts
            .font_data
            .insert("segoe-bold".into(), egui::FontData::from_owned(bold).into());
        bold_family.push("segoe-bold".to_string());
    }
    bold_family.extend(fallbacks);
    fonts
        .families
        .insert(FontFamily::Name(BOLD.into()), bold_family);
    // Last-resort fallback for symbols Segoe UI lacks (✓ ➕ ✏ 🗑 ✅ 🎲 📌 📅 ⏰…).
    if let Some(symbols) = load("seguisym.ttf") {
        fonts.font_data.insert(
            "segoe-symbol".into(),
            egui::FontData::from_owned(symbols).into(),
        );
        for fam in [FontFamily::Proportional, FontFamily::Name(BOLD.into())] {
            fonts
                .families
                .entry(fam)
                .or_default()
                .push("segoe-symbol".into());
        }
    }
    ctx.set_fonts(fonts);
}

/// Bold text at `size` px.
pub fn bold(text: impl Into<String>, size: f32) -> RichText {
    RichText::new(text).font(FontId::new(size, FontFamily::Name(BOLD.into())))
}

// --- Secondary windows ----------------------------------------------------------

/// Background colour of every secondary window (Qt: #1a1a2e).
pub const WINDOW_BG: Color32 = Color32::from_rgb(0x1a, 0x1a, 0x2e);
pub const GOLD: Color32 = Color32::from_rgb(0xd4, 0xaf, 0x37);
pub const TEXT_MUTED: Color32 = Color32::from_rgb(0x88, 0x88, 0x88);

/// Global dark style: gold accents, dark inputs. Applied once at startup.
pub fn apply_global_style(ctx: &egui::Context) {
    let mut style = (*ctx.global_style()).clone();
    style.visuals = egui::Visuals::dark();
    let v = &mut style.visuals;
    v.panel_fill = WINDOW_BG;
    v.window_fill = WINDOW_BG;
    v.extreme_bg_color = Color32::from_rgb(0x2a, 0x2a, 0x2a);
    v.selection.bg_fill = Color32::from_rgb(0x8a, 0x7a, 0x2d);
    v.selection.stroke.color = GOLD;
    v.hyperlink_color = GOLD;
    v.override_text_color = Some(Color32::from_rgb(0xee, 0xee, 0xee));
    style
        .text_styles
        .insert(egui::TextStyle::Body, FontId::proportional(13.0));
    style
        .text_styles
        .insert(egui::TextStyle::Button, FontId::proportional(13.0));
    style.spacing.item_spacing = egui::vec2(constants::SPACE_SM, constants::SPACE_SM);
    ctx.set_global_style(style);
}
