# Sylvania Launcher — Édition Linux 🐧

![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)
![Platform: Linux x86_64](https://img.shields.io/badge/Platform-Linux%20x86__64-informational)
![Version](https://img.shields.io/badge/version-v2.8-success)

🎮 **Launcher officiel pour le serveur World of Warcraft 3.3.5 — Sylvania**, version native Linux distribuée en **AppImage**.

> 🪟 **Vous êtes sous Windows ?** La version Windows se trouve sur la branche [`master`](https://github.com/BlaMacfly/SylvaniaLauncher/tree/master).
> Cette branche `linux` ne concerne que l'édition Linux.

## Aperçu

### Interface Principale
![Interface Principale](screenshots/main_window.png)

### Gestion des Serveurs
![Gestion des Serveurs](screenshots/servers.png)

### Réglages
![Réglages](screenshots/settings.png)

### Notes (Style Post-it)
![Notes](screenshots/notes.png)

## Spécificités de l'édition Linux

- 📦 **Format AppImage** : un seul fichier, aucune installation système requise.
- 🍷 **Wine-GE intégré et automatique** : au premier lancement du jeu, le launcher télécharge
  [Wine-GE](https://github.com/GloriousEggroll/wine-ge-custom) et l'installe dans un **préfixe Wine isolé**
  (aucun `wine` système requis, votre `~/.wine` n'est pas touché).
- 🎮 **DXVK** activé automatiquement si le patch HD fournit `d3d9.dll` (rendu Direct3D 9 → Vulkan).
- 🗂️ **Gestion des chemins insensible à la casse** (le système de fichiers Linux est sensible à la casse) :
  `Wow.exe` / `WoW.exe` / `wow.exe`, dossier `Data`/`data`, etc.
- 📚 **Extraction native via libzip** (pas de dépendance à PowerShell).
- 🔊 **Sons** via `QSoundEffect` (backend Qt Multimedia embarqué dans l'AppImage).

## Fonctionnalités

- 🎮 **Lancement du jeu** via Wine-GE, avec configuration automatique du realmlist
- 📥 **Téléchargement du client WoW 3.3.5** avec progression en temps réel
- 💎 **Patch HD Sylvania** : installation automatique (dossiers `Data`, `Interface` + DXVK)
- 🧩 **Gestion modulaire des patchs HD** : activez/désactivez chaque élément (arbres, eau, sorts…)
- 🧬 **ARAC** (All Races All Classes) : activable/désactivable
- 🖼️ **Fond d'écran aléatoire** : option ON/OFF
- ⚙️ **Auto-configuration** : génération du `Config.wtf` optimal
- 📊 **Statistiques de jeu** : temps de jeu, nombre de lancements
- 📝 **Notes personnelles** style post-it avec couleurs
- 🌐 **Internationalisation** (Français / Anglais)
- 🔄 **Gestion des serveurs** + migration automatique des anciens realmlist
- 🧱 **Gestionnaire d'AddOns**

## Version actuelle

**v2.8** — application native C++ / Qt 6, alignée sur les fonctionnalités de la version Windows.

### Nouveautés v2.8
- 💎 Refonte du gestionnaire de Patch HD (localisation robuste de la racine du client dans l'archive)
- 🧬 Nouveau bouton ARAC (All Races All Classes)
- 🖼️ Fond d'écran aléatoire en option ON/OFF
- 🔄 Migration automatique des anciens hôtes de realmlist au démarrage
- ⚡ Surveillance du processus de jeu non bloquante, extraction durcie

## Installation

1. **Téléchargez** l'AppImage depuis la [release v2.8](https://github.com/BlaMacfly/SylvaniaLauncher/releases/tag/v2.8) :
   [`SylvaniaLauncher-v2.8-x86_64.AppImage`](https://github.com/BlaMacfly/SylvaniaLauncher/releases/download/v2.8/SylvaniaLauncher-v2.8-x86_64.AppImage)

2. **Rendez-la exécutable** :
   ```bash
   chmod +x SylvaniaLauncher-v2.8-x86_64.AppImage
   ```
   *(ou : clic droit → Propriétés → Permissions → « Autoriser l'exécution du fichier comme un programme »)*

3. **Lancez-la** :
   ```bash
   ./SylvaniaLauncher-v2.8-x86_64.AppImage
   ```

Au premier lancement de WoW, Wine-GE est téléchargé et configuré automatiquement (prévoir quelques centaines de Mo).

## Configuration requise

- Linux **x86_64**, distribution récente (**2024+**, glibc ≥ 2.39 — l'AppImage est compilée sur Ubuntu 24.04 / Qt 6.4)
- GPU avec pilotes OpenGL/Vulkan à jour (Mesa, NVIDIA…)
- Connexion Internet (téléchargement de Wine-GE et du client)
- Permissions d'écriture sur le dossier du jeu (patchs, mises à jour)

## Emplacement des données

Toutes les données du launcher sont regroupées sous **un seul dossier** :

```
~/.local/share/Sylvania/Sylvania Launcher/
├── config.json     # préférences
├── data/           # données internes
├── wine-ge/        # moteur Wine
└── wineprefix/     # préfixe Wine isolé
```

## Désinstallation

Supprimez l'AppImage, puis (pour tout nettoyer, Wine et préfixe compris) :

```bash
rm -rf ~/.local/share/Sylvania/Sylvania\ Launcher/
```

## Compilation depuis les sources

### Méthode recommandée : Docker (reproductible)

L'image de build (Ubuntu 24.04 + Qt 6.4 + libzip) est versionnée dans le dépôt.

```bash
# 1) Construire l'image de build
docker build -t sylvania-appimage -f linux/Dockerfile linux

# 2) Produire l'AppImage (depuis la racine du dépôt) -> ./build-appimage/
docker run --rm -v "${PWD}:/src" sylvania-appimage
```

Résultat : `build-appimage/Sylvania_Launcher-x86_64.AppImage`.

### Méthode native (Debian/Ubuntu 24.04+)

```bash
sudo apt install qt6-base-dev qt6-multimedia-dev qt6-tools-dev qt6-tools-dev-tools \
                 qt6-l10n-tools libzip-dev cmake g++ libgl-dev libxkbcommon-dev \
                 libgstreamer-plugins-base1.0-dev file

./linux/build-appimage.sh
```

## Licence

Ce projet est sous licence **GNU GPL v3**. Vous êtes libre de copier, modifier et redistribuer ce logiciel tant que vous conservez cette même licence.

Voir le fichier [LICENSE](LICENSE) pour plus de détails.

## Contribuer

Les contributions sont les bienvenues !
1. Forkez le projet
2. Créez votre branche (`git checkout -b feature/AmazingFeature`)
3. Committez vos changements (`git commit -m 'Add some AmazingFeature'`)
4. Poussez vers la branche (`git push origin feature/AmazingFeature`)
5. Ouvrez une Pull Request **vers la branche `linux`**

## Auteur

© 2025 Sylvania - [sylvania-servergame.com](https://sylvania-servergame.com)
