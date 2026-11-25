//! FR/EN translations. French strings are the source text (keys); the English
//! catalogue is `translations/en.json` (generated from the former Qt `.ts`),
//! embedded in the binary. A missing key falls back to the French text.

use std::collections::HashMap;
use std::sync::Arc;

static EN_JSON: &str = include_str!("../translations/en.json");

/// Cheap to clone (the catalogue is shared), so background tasks can carry one.
#[derive(Clone)]
pub struct I18n {
    lang: String,
    en: Arc<HashMap<String, String>>,
}

impl I18n {
    pub fn new(lang: &str) -> Self {
        let en = Arc::new(serde_json::from_str(EN_JSON).unwrap_or_default());
        I18n { lang: normalize(lang), en }
    }

    pub fn lang(&self) -> &str {
        &self.lang
    }

    pub fn set_lang(&mut self, lang: &str) {
        self.lang = normalize(lang);
    }

    pub fn is_en(&self) -> bool {
        self.lang == "en"
    }

    /// Translates a French source string.
    ///
    /// U+FE0F (emoji variation selector, e.g. in "🗑️") is stripped: egui's
    /// fonts have no glyph for it and would draw a box.
    pub fn t(&self, fr: &str) -> String {
        let s = match self.en.get(fr) {
            Some(en) if self.is_en() => en.as_str(),
            _ => fr,
        };
        s.replace('\u{FE0F}', "")
    }

    /// Translates then substitutes `%1`, `%2`, … (Qt-style placeholders).
    pub fn tf(&self, fr: &str, args: &[&dyn std::fmt::Display]) -> String {
        let mut s = self.t(fr);
        // Replace from the highest index down so %1 never eats %10.
        for (i, a) in args.iter().enumerate().rev() {
            s = s.replace(&format!("%{}", i + 1), &a.to_string());
        }
        s
    }
}

fn normalize(lang: &str) -> String {
    if lang == "en" { "en".into() } else { "fr".into() }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn catalogue_parses_and_translates() {
        let mut i = I18n::new("en");
        assert_eq!(i.t("NOTES"), "NOTES");
        assert_eq!(i.tf("Lancements: %1", &[&3]), "Launches: 3");
        i.set_lang("fr");
        assert_eq!(i.tf("Lancements: %1", &[&3]), "Lancements: 3");
        assert_eq!(i.t("chaîne inconnue"), "chaîne inconnue");
    }
}
