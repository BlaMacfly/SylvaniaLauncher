# Chantier A — Plan d'intégration du runtime (module greffé)

Décision (2026-05-31) : **module runtime greffé** depuis Winlator-bionic
(Pipetto-crypto) derrière l'interface `GameRuntime`, en gardant notre UI/logique
launcher Kotlin. Rootfs **téléchargé au 1er lancement** (push adb en proto).
Cible : **AYN Thor — Adreno 740, Android 13, arm64-v8a**.

## 1. Pourquoi on ne peut pas « juste adb push » le runtime

`imagefs.txz` et les Proton sont des **tar+xz contenant de nombreux symlinks**
(Wine : `wine64`→`wine`, libs, etc.). `adb push` ne recrée pas les symlinks et
`toybox tar` d'Android ne gère pas xz de façon fiable. → l'extraction DOIT passer
par notre code (Apache Commons Compress, comme Winlator `TarCompressorUtils`),
qui recrée tar + xz + symlinks. C'est donc le **premier brique** à porter.

## 2. Charge binaire (acquise, dans `_runtime_reference/_payload/` + assets clonés)

| Binaire | Taille | Source |
|---|---|---|
| imagefs.txz | 175 Mo | rootfs bionic de base |
| proton-9.0-arm64ec.txz | 62 Mo | **Wine arm64ec précompilé** |
| proton-9.0-x86_64.txz | 47 Mo | variante x86_64 |
| box64 / wowbox64 / fexcore | ~7 Mo | émulateurs (.tzst) |
| dxvk-2.4.1-arm64ec, vkd3d, d8vk | ~20 Mo | wrappers DirectX (arm64ec) |
| adrenotools-turnip25.1.0 + wrapper + extra_libs | ~28 Mo | driver Vulkan Turnip (Adreno) |
| wincomponents (direct3d, opengl…) | ~50 Mo | DLLs Windows |
| 6 .so audio (pulse/sndfile/ltdl) | ~1,7 Mo | jniLibs (les SEULES libs natives de l'app) |

Récupération LFS fiable : endpoint media
`https://media.githubusercontent.com/media/Pipetto-crypto/winlator/dev/app/src/main/assets/<f>`.

## 3. Sous-système à vendre (≈210 fichiers Java, `com.winlator.cmod.*`)

On conserve le package `com.winlator.cmod.*` (évite la réécriture massive
d'imports) comme **module/source-set runtime vendu**, et on n'y touche que pour
retirer les dépendances UI (fragments, R resources) non nécessaires.

| Package | Fichiers | Rôle | À garder |
|---|---|---|---|
| `xserver` | 94 | serveur X11 embarqué (WM, drawables, extensions DRI3/Present/Sync) | ✅ cœur affichage |
| `renderer` | 19 | GLRenderer (GLES 3.0) fenêtres X → textures | ✅ |
| `widget` | 14 | XServerView (GLSurfaceView), surfaces | ✅ (XServerView ; reste à trier) |
| `xconnector` | 9 | sockets Unix / protocole X | ✅ |
| `xenvironment` | 14 | ImageFs(+Installer), components (BionicProgramLauncher, XServer, SysVSharedMemory…) | ✅ |
| `box86_64` | 8 | presets/env Box64, rcfiles | ✅ |
| `contents` | 4 | ContentsManager (.tzst à la demande) | ✅ |
| `core` | 37 | EnvVars, ProcessHelper, TarCompressorUtils, FileUtils, GPUInformation, WineInfo… | ✅ (tri : enlever helpers UI) |
| `math` | 2 | utils | ✅ |
| `inputcontrols` | 9 | mapping tactile → clavier/souris | 🟡 plus tard (entrées) |
| modules racine `android_sysvshm`, `audio_plugin`, `input_controls` | — | natif (CMake) : SysV SHM, audio | ✅ sysvshm ; audio plus tard |

À NE PAS porter : `MainActivity`/fragments Winlator, gestion conteneurs UI,
écrans de réglages — remplacés par notre UI + `BionicGameRuntime`.

## 4. Recette d'environnement (vérifiée, `BionicProgramLauncherComponent`)

Lancement arm64ec, sans box64 externe ni proot :
`command = <wine>/bin/wine <Wow.exe>` avec `HODLL=wowbox64.dll`, et env :
```
HOME=/home/xuser  USER=xuser  DISPLAY=:0  WINEARCH=win64 (WoW64)
PREFIX=<root>/usr  PATH=<wine>/bin:<root>/usr/bin
LD_LIBRARY_PATH=<root>/usr/lib:/system/lib64
WINEPREFIX=<root>/home/xuser-{id}/.wine
BOX64_DYNAREC=1 BOX64_X11GLX=1 BOX64_MMAP32=1 BOX64_DYNAREC_SAFEFLAGS=2
TU_DEBUG=noconform,sysmem  WINEESYNC=1  MESA_SHADER_CACHE_*  DXVK_HUD=...
WINE_X11FORCEGLX=1  ANDROID_SYSVSHM_SERVER=...  LD_PRELOAD=libandroid-sysvshm.so
```

## 5. Séquence incrémentale (chaque étape = test réel + logs sur l'Ayn Thor)

1. **A1 — Extraction on-device** : porter `ImageFs` + `ImageFsInstaller` +
   `TarCompressorUtils` + `core` minimal ; faire extraire imagefs+proton dans
   `filesDir/imagefs`. *Test : l'arbre + symlinks existent (adb run-as).*
2. **A2 — Phantom killer** : neutraliser via adb (`settings put global
   settings_enable_monitor_phantom_procs false`, `device_config …`).
3. **A3 — Lancement headless** : porter `BionicProgramLauncherComponent` (+ deps
   `box86_64`, `contents`, `EnvVars`, `ProcessHelper`, `WineInfo`, `Container`
   minimal) ; exécuter `wine wineboot` / `wine cmd /c ver`. *Test : logcat/Wine
   logs montrent Wine WoW64 + wowbox64 qui démarrent.*
4. **A4 — Serveur X + SurfaceView** : porter `xserver`+`renderer`+`widget`+
   `xconnector` ; héberger une `XServerView` dans une activité ; lancer
   **`winecfg`/notepad**. *Test : la fenêtre s'affiche (screenshot).* ← jalon critique
5. **A5 — Turnip + DXVK** : activer AdrenoTools turnip 25.1.0 + dxvk arm64ec ;
   tester un D3D9 (testd3d). *Test : rendu Vulkan.*
6. **A6 — `Wow.exe`** : `BionicGameRuntime.launch()` branché sur le flux launcher
   (realmlist déjà écrit par le Chantier B). *Test : le client démarre.*
7. **A7 — Entrées + audio** : `inputcontrols` (tactile→clavier/souris) + audio.

## 6. Risques résiduels

- Couplage fort de `xserver`/`xenvironment` aux classes `Container`/`Shortcut`/
  `XServerDisplayActivity` → on portera un `Container` minimal.
- Performances frame-pacing X→Surface (goulot documenté).
- Phantom-process killer (Android 13) — neutralisation requise.
- Licences : Winlator MIT, Wine LGPL, Box64 MIT, DXVK zlib, Mesa MIT — compatibles GPLv3.
