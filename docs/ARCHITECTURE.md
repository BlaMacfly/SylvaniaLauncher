# Sylvania Launcher Android — Document d'architecture

Portage du launcher C++/Qt « Sylvania Launcher » en application Android native,
exécutant le client WoW 3.3.5a via une pile **Wine-bionic + Box64 (WoW64)**.

> Statut : Phases 0, 1, 2.0 terminées (analyse + état de l'art + cartographie de
> la recette runtime). Chantier B (launcher Kotlin) en cours. Chantier A (runtime
> du jeu) non démarré.

---

## 1. Stratégie

- La **logique du launcher est portée nativement** en Kotlin (ce dépôt), d'après
  la spécification extraite du code C++ (Phase 0). Le launcher **ne tourne pas**
  sous Wine.
- **Seul `Wow.exe` s'exécute via la pile de traduction** : le launcher prépare
  l'environnement (realmlist, patch, intégrité) puis lance le client.
- Cible matérielle confirmée : **GPU Adreno**, **Android récent non rooté**,
  client **WoW 3.3.5a x86 32-bit**.

## 2. Pile runtime du jeu (Chantier A — recette cartographiée, pas encore intégrée)

Modèle retenu (vérifié dans Winlator-Bionic / Hangover / XaW64) — **pas** le
`box86 wine` initialement supposé :

```
Wow.exe (PE x86 32-bit)
  └─ Wine WoW64 (conteneur arm64ec, Proton/Wine ≥9 compilé bionic via NDK)
       └─ wowbox64.dll  (Box64 chargé IN-PROCESS, HODLL=wowbox64.dll)  [alt: libwow64fex.dll = FEX]
            └─ libs ARM natives + AdrenoTools → Turnip (Mesa/freedreno) → Vulkan → Adreno
  ⟶ rendu X11 → serveur X embarqué (GLSurfaceView + GLES3) → SurfaceView Android
```

Commande de lancement effective (chemin bionic arm64ec, **sans box64 externe ni
PRoot**) : `"<wine>/bin/wine" Wow.exe` avec `HODLL=wowbox64.dll`.

### Versions épinglées (vérifiées dans les dépôts de référence)

| Composant | Version | Source |
|---|---|---|
| NDK | r27.0.12077973 | `build.gradle` Winlator-Bionic |
| compile/min/target SDK | 34 / 26 / 28 | idem |
| Wine | Proton 9.0 (arm64ec) ; base build = Wine 10.7 (XaW64) | assets / XaW64 |
| Box64 / wowbox64 | 0.3.5 ; wowbox64 0.3.4 & 0.3.7 | XaW64 / assets |
| FEXCore (alt.) | 2505 / 2507 | assets |
| DXVK | défaut 1.10.1 ; arm64ec : 2.3.1 / 2.4.1 (gplasync dispo) | assets |
| VKD3D / D8VK | 2.12-0 / 1.0 | assets |
| Turnip (Adreno) | AdrenoTools turnip 25.1.0 (+ v762/v805) | assets |

Dépôts de référence (clonés dans `_runtime_reference/`, hors APK) :
`Pipetto-crypto/winlator`, `ar37-rs/xaw64-wine`.

### Packaging runtime (vérifié)

- `imagefs.txz` (rootfs bionic, git-LFS) embarqué en asset, extrait au 1er
  lancement vers `getFilesDir()/imagefs` (stockage interne, **aucune permission
  storage Android 11+**), via Apache Commons Compress (XZ/ZSTD).
- WINEPREFIX persistant par conteneur : `imagefs/home/xuser-{id}/.wine`.
- ⚠️ **Android 12+** : *phantom process killer* (signal 9) tue les process
  enfants — contournement nécessaire (réglage ADB côté utilisateur, comme
  Winlator/XaW64). Risque packaging réel.

## 3. Logique launcher portée (Chantier B — ce dépôt)

Réécriture Kotlin de la couche `core/` du C++ (UI Qt → Compose). Le format
`config.json` est **identique** au C++ → interchangeable.

| Module Kotlin | Port de | Rôle |
|---|---|---|
| `core/Constants.kt` | `Constants.h` | URLs serveur, realmlist canonique |
| `core/config/LauncherConfig.kt` | schéma `config.json` | modèle sérialisable |
| `core/config/ConfigManager.kt` | `ConfigManager` | load/save JSON, migration legacy, realmlist |
| `core/realmlist/RealmlistWriter.kt` | `updateRealmlist`/`migrateLegacyRealmlist` | écriture realmlist.wtf (pur, testé) |
| `core/patch/WowConfigWriter.kt` | `generateConfigWtf` | génération Config.wtf (pur, testé) |
| `core/patch/HdPatchManager.kt` | `HdPatchManager` | download + SHA-256 + extract + migrate |
| `core/patch/HdComponentToggle.kt` | helpers `SettingsDialog` | toggle .mpq ↔ .disabled (pur, testé) |
| `core/io/ZipExtractor.kt` | PowerShell `Expand-Archive` | extraction ZIP (commons-compress + anti Zip-Slip) |
| `core/launch/GameLauncher.kt` | `MainWindow::playGame` | flux : vérif → realmlist → runtime |
| `ui/*` | Qt Widgets | UI Compose minimale + ViewModel |

Remplacements des dépendances Windows : PowerShell → commons-compress ;
robocopy → `copyRecursively` ; tasklist → (futur) suivi PID ; QNetwork → OkHttp ;
QStandardPaths → `filesDir`/`cacheDir`.

### Couture d'intégration vers le Chantier A

`core/launch/GameRuntime` est l'interface que le runtime implémentera.
`NotIntegratedRuntime` (placeholder actuel) signale l'indisponibilité au lieu de
simuler un lancement. Quand Chantier A sera prêt, une implémentation
`BionicGameRuntime` assemblera l'environnement (§2) et exécutera `wine Wow.exe`.

## 4. Build & test

Prérequis : **JDK 17**, **Android SDK** (platform 34, NDK r27.0.12077973),
Android Studio recommandé.

```bash
./gradlew test            # tests unitaires JVM (logique pure, sans device)
./gradlew assembleDebug   # → app/build/outputs/apk/debug/app-debug.apk
```

### Statut de build — ✅ VÉRIFIÉ (2026-05-31)

Toolchain installée et build réel effectué sur la machine de dev :

| Outil | Version | Installation |
|---|---|---|
| JDK | Microsoft OpenJDK 17.0.19 | `winget install Microsoft.OpenJDK.17` |
| Android cmdline-tools | 20.0 | `commandlinetools-win-14742923_latest.zip` → `C:\Android\Sdk\cmdline-tools\latest` |
| SDK | platform-34, build-tools 34.0.0, platform-tools | `sdkmanager` |
| NDK | r27.0.12077973 | `sdkmanager "ndk;27.0.12077973"` |
| Gradle | 8.9 | wrapper généré (`gradle wrapper`) |

Résultats :
- `./gradlew test` → **6/6 tests OK** (`LauncherLogicTest` : realmlist, Config.wtf, migration legacy, toggle HD).
- `./gradlew assembleDebug` → **`app-debug.apk` ~11 Mo** généré.

> ⚠️ **Piège Windows rencontré** : le test worker échouait avec
> `ClassNotFoundException: Files`. Cause : une entrée du `PATH` système contenait
> un guillemet parasite (`C:\Program Files\nodejs"`) que Gradle injecte non
> échappé dans le `-Djava.library.path` du worker → `java.exe` interprète
> « Files » (de « Program Files ») comme classe principale. **Fix** : nettoyer le
> `PATH` (retirer le `"`) avant d'invoquer Gradle. Ce n'est pas un défaut du code.

## 5. Risques (priorité décroissante)

1. 🔴 Compiler Wine/Proton arm64ec pour bionic (obstacle #1 — Chantier A).
2. 🟠 Pont d'affichage X→SurfaceView (serveur X embarqué GLES3 ; frame-pacing).
3. 🟢 Logique launcher (ce dépôt) — faible risque, testable hors jeu.
4. 🟢 wowbox64 + redirection libs — mécanisme vérifié.
5. 🟢 Turnip + DXVK Adreno (AdrenoTools, sans root).
6. 🟠 Audio (ALSA→android_aserver / PulseAudio) + entrées (InputControls).
7. 🟠 Packaging APK — vérifié ; ⚠️ phantom-process Android 12+.
