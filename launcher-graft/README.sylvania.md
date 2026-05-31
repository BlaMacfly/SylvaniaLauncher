# Sylvania Launcher — Android

Port Android natif du launcher C++/Qt « Sylvania Launcher » pour World of
Warcraft 3.3.5a. La logique du launcher (realmlist, patch HD, intégrité,
config) est réécrite en Kotlin ; le client `Wow.exe` s'exécutera via une pile
**Wine-bionic + Box64 (WoW64)** sur GPU Adreno, sans root.

Voir **[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)** pour la conception complète
(pile runtime, versions épinglées, risques) et la cartographie portable/à-réécrire.

## État

| Phase | Statut |
|---|---|
| 0 — Analyse du projet C++ source | ✅ |
| 1 — État de l'art runtime (sourcé) | ✅ |
| 2.0 — Cartographie de la recette (Winlator-Bionic / XaW64) | ✅ |
| B — Squelette APK + logique launcher Kotlin | ✅ build vérifié (`test` 6/6, `assembleDebug` OK) |
| A — Runtime du jeu (Wine-bionic, serveur X) | 🚧 A1–A3 faits : rootfs extrait + **`wine-9.0` tourne sur l'AYN Thor** (wineboot OK). Reste serveur X (A4) |

## Structure

```
app/src/main/java/com/sylvania/launcher/
  core/            logique métier portée du C++ (testable hors jeu)
    config/        ConfigManager + modèle config.json
    realmlist/     écriture realmlist.wtf + migration legacy
    patch/         HdPatchManager, Config.wtf, toggle HD modulaire
    io/            extraction ZIP
    launch/        GameLauncher + interface GameRuntime (couture vers Chantier A)
  ui/              MainActivity (Compose) + ViewModel
app/src/test/      tests unitaires JVM de la logique pure
docs/ARCHITECTURE.md
_runtime_reference/  clones upstream (Winlator-Bionic, XaW64) — non versionnés
```

## Build

Prérequis : JDK 17, Android SDK (platform 34, NDK r27.0.12077973). Android Studio
génère le wrapper Gradle automatiquement à l'import.

```bash
./gradlew test            # tests logique (sans device) — 6/6 OK
./gradlew assembleDebug   # APK debug → app/build/outputs/apk/debug/app-debug.apk (~11 Mo)
```

Build **vérifié** le 2026-05-31 (JDK 17.0.19, Android SDK platform-34 /
build-tools 34.0.0 / NDK r27.0.12077973, Gradle 8.9, AGP 8.5.2, Kotlin 2.0.20).

> ⚠️ **Piège Windows** : si le build échoue avec `ClassNotFoundException: Files`,
> c'est qu'une entrée du `PATH` système contient un guillemet parasite
> (ex. `C:\Program Files\nodejs"`), que Gradle injecte non échappé dans le
> `-Djava.library.path` du test worker. Corriger le `PATH` (retirer le `"`) ou
> lancer Gradle avec un PATH nettoyé. Voir [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Licence

GPL v3 (héritée du projet source). © Sylvania — sylvania-servergame.com
