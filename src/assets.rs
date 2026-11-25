//! Assets embedded in the executable (single-file distribution).

use eframe::egui;

pub static LOGO_PNG: &[u8] = include_bytes!("../Asset/sylvania_logo.png");
pub static ICON_PNG: &[u8] = include_bytes!("../Asset/LogoSylvania/LogoSylvania256.png");
pub static ADDONS_MANIFEST_JSON: &str = include_str!("../resources/addons.json");

pub static SOUND_LAUNCH: &[u8] = include_bytes!("../Asset/Sounds/launch.mp3");
pub static SOUND_TOGGLE: &[u8] = include_bytes!("../Asset/Sounds/toggle.mp3");
pub static SOUND_NOTIFICATION: &[u8] = include_bytes!("../Asset/Sounds/Notification.mp3");

/// Raw bytes of a background by base name (see constants::BACKGROUNDS).
pub fn background(name: &str) -> Option<&'static [u8]> {
    Some(match name {
        "Alliance" => include_bytes!("../Asset/Background/Alliance.jpg"),
        "Arbre de Vie" => include_bytes!("../Asset/Background/Arbre de Vie.jpg"),
        "Azeroth" => include_bytes!("../Asset/Background/Azeroth.png"),
        "Horde" => include_bytes!("../Asset/Background/Horde.jpg"),
        "Ilidan" => include_bytes!("../Asset/Background/Ilidan.jpg"),
        "Lich King" => include_bytes!("../Asset/Background/Lich King.jpg"),
        "Ragnaros" => include_bytes!("../Asset/Background/Ragnaros.jpg"),
        "Taverne" => include_bytes!("../Asset/Background/Taverne.png"),
        _ => return None,
    })
}

/// Decodes an embedded image into an egui texture-ready image.
pub fn decode_image(bytes: &[u8]) -> Option<egui::ColorImage> {
    let img = image::load_from_memory(bytes).ok()?.to_rgba8();
    let size = [img.width() as usize, img.height() as usize];
    Some(egui::ColorImage::from_rgba_unmultiplied(size, img.as_raw()))
}
