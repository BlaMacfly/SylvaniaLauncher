//! UI sounds ("play", "button", "toggle", "notification"), played on a
//! dedicated audio thread so the UI never blocks and a missing audio device is
//! harmless.

use crate::assets;
use std::sync::mpsc::{self, Sender};

pub struct Sounds {
    tx: Option<Sender<&'static [u8]>>,
    enabled: bool,
}

impl Sounds {
    pub fn new(enabled: bool) -> Self {
        let (tx, rx) = mpsc::channel::<&'static [u8]>();
        let spawned = std::thread::Builder::new()
            .name("audio".into())
            .spawn(move || {
                let Ok(mut stream) = rodio::DeviceSinkBuilder::open_default_sink() else {
                    log::warn!("Aucun périphérique audio: sons désactivés");
                    return;
                };
                stream.log_on_drop(false);
                let mut current: Option<rodio::Player> = None;
                while let Ok(bytes) = rx.recv() {
                    // Stop current playback before starting new (like the Qt player).
                    if let Some(s) = current.take() {
                        s.stop();
                    }
                    let Ok(source) = rodio::Decoder::try_from(std::io::Cursor::new(bytes)) else {
                        continue;
                    };
                    let sink = rodio::Player::connect_new(stream.mixer());
                    sink.set_volume(0.5);
                    sink.append(source);
                    current = Some(sink);
                }
            })
            .is_ok();
        Sounds {
            tx: spawned.then_some(tx),
            enabled,
        }
    }

    pub fn set_enabled(&mut self, enabled: bool) {
        self.enabled = enabled;
    }

    pub fn is_enabled(&self) -> bool {
        self.enabled
    }

    /// Plays a named UI sound; unknown names and disabled sound are no-ops.
    pub fn play(&self, name: &str) {
        if !self.enabled {
            return;
        }
        let bytes = match name {
            "play" => assets::SOUND_LAUNCH,
            "button" | "toggle" => assets::SOUND_TOGGLE,
            "notification" => assets::SOUND_NOTIFICATION,
            _ => return,
        };
        if let Some(tx) = &self.tx {
            let _ = tx.send(bytes);
        }
    }
}
