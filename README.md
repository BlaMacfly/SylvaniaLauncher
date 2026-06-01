# Sylvania Launcher (Android)

Portage Android natif du launcher **Sylvania** : il permet de jouer à **World of Warcraft 3.3.5a (Wrath of the Lich King)** sur le serveur privé Sylvania directement depuis un smartphone ou une console portable Android, sans PC.

Le launcher installe le runtime, configure le client et lance `Wow.exe` jusqu'à l'écran de connexion puis en jeu.

---

## ✅ Statut

**WoW 3.3.5a tourne en jeu (in-world)** — vérifié sur **AYN Thor** (Snapdragon 8 Gen 2 / Adreno 740, Android 13) :
connexion au realm → authentification → sélection du personnage → entrée dans le monde, UI complète, **~78 FPS** au démarrage avec le pilote **Turnip**.

> Point technique clé : sur GPU Adreno, il **faut** le pilote **Turnip (Mesa) via AdrenoTools**. Le pilote Qualcomm propriétaire fait échouer une allocation mémoire DXVK et bloque le rendu à l'écran de login. Le launcher force désormais Turnip automatiquement.

---

## 🧱 Comment ça marche

Le projet est bâti sur une base **Winlator-bionic** dans laquelle est greffée la logique du launcher Sylvania. La pile d'exécution :

- **Wine (Proton 9.0 arm64ec, mode WoW64)** — pas d'émulation x86 du host, Wine tourne en natif ARM64.
- **wowbox64 (box64)** — émule uniquement le code i386 de `Wow.exe`.
- **DXVK** — traduit Direct3D 9 → Vulkan.
- **Turnip (Mesa) via AdrenoTools** — pilote Vulkan open source pour Adreno.
- **Serveur X embarqué** rendu dans une `SurfaceView`.

---

## 🎮 Matériel recommandé

### Appareil

| | Recommandé |
|---|---|
| **SoC / GPU** | Snapdragon avec **Adreno 6xx ou 7xx** (compatible Turnip) |
| **Testé** | AYN Thor — Snapdragon 8 Gen 2, Adreno 740 |
| **Android** | 11+ (ARM64) |
| **Vulkan** | 1.1+ |
| **RAM** | 8 Go minimum, 12 Go confortable |
| **Stockage** | ~30 Go libres pour le client WoW |

> ⚠️ Certains SoC très récents (ex. Snapdragon 8 Elite) ou les Adreno 7xx « allégés » (735, 732, 720, 710…) ne sont pas pris en charge par Turnip — dans ce cas, un autre pilote (VirGL/Vortek) est nécessaire et les performances varient.

### Périphériques Bluetooth (fortement conseillés)

WoW est un jeu PC : la saisie des identifiants et le contrôle souris/clavier rendent un **clavier + souris Bluetooth** bien plus confortables que le tactile (qui reste fonctionnel via le touchpad émulé et le clavier Android).

- **Clavier** : **Logitech K380** (ou son successeur **Pebble Keys 2 K380s**)
  *Compact, multi-appareils (jusqu'à 3 en Bluetooth), excellent support Android, autonomie longue.*
- **Souris** : **Logitech Pebble Mouse 2 M350s**
  *Fine, silencieuse, nomade, appairage Bluetooth multi-appareils.*
- **Option ultra-nomade** : **clavier sans fil pliable avec pavé tactile intégré**
  *Clavier + souris en un seul accessoire pliable — idéal pour jouer en déplacement sans rien d'autre à transporter.*

---

## ⌨️ Contrôles (sans périphérique externe)

- **Souris** : l'écran fait office de **touchpad** — un doigt déplace le curseur, un tap = clic gauche.
- **Menu** : bouton **Retour** (ou **tap à 4 doigts**) ouvre le menu latéral.
- **Clavier** : dans ce menu, **« Keyboard »** affiche le clavier Android pour taper (identifiants, chat…).

Avec un clavier/souris Bluetooth appairés, le contrôle est direct, comme sur PC.

---

## 🚀 Premier lancement

1. Installer l'APK et l'ouvrir.
2. Le launcher extrait le runtime (imagefs + Proton) au premier démarrage (quelques minutes).
3. Placer le client **WoW 3.3.5a** dans `Téléchargements/SylvaniaLauncher/wotlk` (ou utiliser le téléchargement intégré à venir).
4. Le realmlist est écrit automatiquement vers `sylvania-servergame.com`.
5. Le jeu se lance jusqu'à l'écran de connexion.

---

## 🛠️ Build (développeurs)

- JDK 17, Android SDK (build-tools 35), NDK r27.
- Placer les assets runtime (`imagefs.txz`, `proton-9.0-arm64ec.txz`, pilotes graphiques `.tzst`, etc.) dans `app/src/main/assets/` (gros binaires non versionnés).
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

World of Warcraft est une marque de Blizzard Entertainment. Ce projet n'est ni affilié ni approuvé par Blizzard. À usage avec un serveur privé dont vous possédez les droits/accès.
