# Sylvania Launcher (Android)

Portage Android natif du launcher **Sylvania** : il permet de jouer à **World of Warcraft 3.3.5a (Wrath of the Lich King)** sur le serveur privé Sylvania directement depuis un smartphone ou une console portable Android, **sans PC** — y compris **à la manette**.

Le launcher offre une page d'accueil (serveur, statistiques, téléchargement, addons), installe le runtime, configure le client et lance `Wow.exe` jusqu'en jeu.

---

## ✅ Statut

**WoW 3.3.5a est jouable de bout en bout**, vérifié sur **AYN (Snapdragon 8 Gen 2 / Adreno 740, Android 13)** :
connexion au realm → personnage → **en jeu dans le monde**, UI complète, **~78 FPS** (pilote **Turnip**), et **contrôle complet à la manette** via ConsolePort.

> Point technique clé : sur GPU Adreno, il **faut** le pilote **Turnip (Mesa) via AdrenoTools**. Le pilote Qualcomm propriétaire fait échouer une allocation mémoire DXVK et fige le rendu à l'écran de login. Le launcher force Turnip automatiquement.

---

## 🚀 Fonctionnalités du launcher

Page d'accueil (fond thématique + logo Sylvania), avec :

- **JOUER** — prépare le runtime et lance WoW jusqu'au monde.
- **TÉLÉCHARGER** — télécharge le client WoW 3.3.5a (~19 Go) depuis le serveur et l'installe (extraction + génération de `Config.wtf`).
- **HD** — télécharge et applique le Patch HD (textures/UI haute définition).
- **Liste des Addons** — installe / **supprime** des addons recommandés (Bagnon, AtlasLoot, DBM, ElvUI, WeakAuras…).
- **RÉGLAGES** — chemin du client, langue FR/EN (écrit la locale de `Config.wtf`), thème de fond (8 thèmes), fond aléatoire.
- **Changer de Serveur** — gestion des realmlists (sélection / ajout).
- **Statistiques** — temps de jeu, nombre de lancements, dernière session.

Configuration persistée dans `config.json` (interchangeable avec le build Windows).

---

## 🎮 Manette (ConsolePort)

Le client 3.3.5a n'a aucune API manette native. Le launcher pré-installe **ConsolePortLK** (backport de ConsolePort pour 3.3.5a) et **un profil de manette Winlator pré-mappé** (auto-activé au lancement) qui convertit la manette → les touches attendues par ConsolePort.

**Mise en route (une seule fois, en jeu) :** ouvre le clavier (bouton **Retour** ou **tap 4 doigts** → **Keyboard**), tape **`/cp`**, puis choisis **Xbox**. La manette pilote alors WoW : sticks (déplacement + caméra), boutons (sorts, saut), gâchettes (modificateurs).

→ Idéal sur une console portable Android (manette intégrée) ou avec une manette Xbox/XInput en Bluetooth/USB.

---

## 🧱 Comment ça marche

Base **Winlator-bionic** + logique launcher Sylvania greffée. Pile d'exécution :

- **Wine (Proton 9.0 arm64ec, mode WoW64)** — Wine tourne en natif ARM64 (pas d'émulation x86 du host).
- **wowbox64 (box64)** — émule uniquement le code i386 de `Wow.exe`.
- **DXVK** — traduit Direct3D 9 → Vulkan.
- **Turnip (Mesa) via AdrenoTools** — pilote Vulkan open source pour Adreno.
- **Serveur X embarqué** rendu dans une `SurfaceView`.

---

## 📱 Matériel recommandé

### Appareil

| | Recommandé |
|---|---|
| **SoC / GPU** | Snapdragon avec **Adreno 6xx ou 7xx** (compatible Turnip) |
| **Testé** | AYN — Snapdragon 8 Gen 2, Adreno 740 |
| **Android** | 11+ (ARM64) |
| **Vulkan** | 1.1+ |
| **RAM** | 8 Go minimum, 12 Go confortable |
| **Stockage** | ~30 Go libres pour le client WoW |

> ⚠️ Certains SoC très récents (ex. Snapdragon 8 Elite) ou les Adreno 7xx « allégés » (735, 732, 720, 710…) ne sont pas pris en charge par Turnip — un autre pilote (VirGL/Vortek) serait alors nécessaire et les performances varient.

### Contrôle

- **Manette** (recommandé sur handheld) : manette intégrée ou manette **Xbox/XInput** (Bluetooth/USB) — voir la section ConsolePort.
- **Clavier + souris Bluetooth** (alternative confortable, ex. **Logitech K380 / Pebble Keys 2 K380s** + **Pebble Mouse 2 M350s**, ou un **clavier pliable sans fil avec pavé tactile**).
- **Sans accessoire** : l'écran fait office de **touchpad** (1 doigt = curseur, tap = clic) ; le menu (Retour / tap 4 doigts → **Keyboard**) ouvre le clavier Android.

---

## 🕹️ Premier lancement

1. Installer l'APK et ouvrir le launcher.
2. Au premier **JOUER**, le runtime (imagefs + Proton) s'extrait (quelques minutes).
3. Obtenir le client **WoW 3.3.5a** : via **TÉLÉCHARGER**, ou en plaçant un client existant dans `Téléchargements/SylvaniaLauncher/wotlk` (modifiable dans **RÉGLAGES**).
4. Le realmlist est écrit automatiquement (`sylvania-servergame.com`).
5. **JOUER** → écran de connexion → en jeu. Puis `/cp` → Xbox pour la manette.

---

## 🛠️ Build (développeurs)

- JDK 17, Android SDK (build-tools 35), NDK r27.
- Placer les assets runtime (`imagefs.txz`, `proton-9.0-arm64ec.txz`, pilotes graphiques `.tzst`, driver `adrenotools-turnip25.1.0.tzst`, etc.) dans `app/src/main/assets/` (gros binaires non versionnés).
- Compiler : `./gradlew assembleDebug` → `app/build/outputs/apk/debug/app-debug.apk`.

---

## 🙏 Crédits

Construit au-dessus d'un travail open source remarquable :

- **Winlator** — [brunodev85/winlator](https://github.com/brunodev85/winlator)
- **Wine / Proton** — [winehq.org](https://www.winehq.org/)
- **Box64 / wowbox64** — [ptitSeb/box64](https://github.com/ptitSeb/box64)
- **Mesa (Turnip / Zink)** — [mesa3d.org](https://www.mesa3d.org)
- **DXVK** — [doitsujin/dxvk](https://github.com/doitsujin/dxvk)
- **AdrenoTools** — [bylaws/adrenotools](https://github.com/bylaws/adrenotools)
- **ConsolePortLK** — [leoaviana/ConsolePortLK](https://github.com/leoaviana/ConsolePortLK)

World of Warcraft est une marque de Blizzard Entertainment. Ce projet n'est ni affilié ni approuvé par Blizzard. À usage avec un serveur privé dont vous possédez les droits/accès.
