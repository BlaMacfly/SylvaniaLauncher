// Embeds the Windows icon + version metadata into SylvaniaLauncher.exe.
fn main() {
    println!("cargo:rerun-if-changed=Asset/LogoSylvania/LogoSylvania256.ico");
    println!("cargo:rerun-if-changed=translations/en.json");

    if std::env::var("CARGO_CFG_TARGET_OS").as_deref() == Ok("windows") {
        let mut res = winresource::WindowsResource::new();
        res.set_icon("Asset/LogoSylvania/LogoSylvania256.ico")
            .set("ProductName", "Sylvania Launcher")
            .set("FileDescription", "Sylvania Launcher - World of Warcraft 3.3.5a")
            .set("CompanyName", "Sylvania")
            .set("LegalCopyright", "Copyright (C) 2026 Sylvania - GPLv3")
            .set("OriginalFilename", "SylvaniaLauncher.exe")
            .set("InternalName", "SylvaniaLauncher");
        if let Err(e) = res.compile() {
            // Never fail the build over cosmetics (e.g. rc.exe missing).
            println!("cargo:warning=icône Windows non intégrée: {e}");
        }
    }
}
