# Sylvania Launcher — Édition Windows 🪟

![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)
![Version](https://img.shields.io/badge/version-v2.8-success)

🎮 **Launcher officiel pour le serveur World of Warcraft 3.3.5 - Sylvania**

> 🐧 **Vous êtes sous Linux ?** Une version native (AppImage + Wine-GE) est disponible sur la branche [`linux`](https://github.com/BlaMacfly/SylvaniaLauncher/tree/linux).

## Aperçu

### Interface Principale
![Interface Principale](screenshots/main_window.png)

### Gestion des Serveurs
![Gestion des Serveurs](screenshots/servers.png)

### Réglages
![Réglages](screenshots/settings.png)

### Notes (Style Post-it)
![Notes](screenshots/notes.png)

## Fonctionnalités

- 🎮 **Lancement du jeu** avec configuration automatique du realmlist
- 📥 **Téléchargement du client WoW** 3.3.5 avec progression en temps réel
- 💎 **Patch HD Sylvania** : Installation automatique du patch haute définition (Data, Interface)
- 🧬 **ARAC** (All Races All Classes) : activable/désactivable
- 🖼️ **Fond d'écran aléatoire** : option ON/OFF
- ⚙️ **Auto-Configuration** : Génération automatique du fichier `Config.wtf` optimal
- 📊 **Statistiques de jeu** : temps de jeu, nombre de lancements
- 📝 **Notes personnelles** style post-it avec couleurs
- ⚙️ **Réglages** : chemin WoW (sélection libre), cache, AddOns, sons
- 🔄 **Gestion des serveurs** : ajouter, modifier, supprimer

## Version actuelle

**v2.8** - Application native C++ / Qt6

### Nouveautés v2.8
- 💎 Refonte du gestionnaire de Patch HD (localisation robuste de la racine du client dans l'archive).
- 🧬 Nouveau bouton ARAC (All Races All Classes).
- 🖼️ Fond d'écran aléatoire en option ON/OFF.
- 🔄 Migration automatique des anciens hôtes de realmlist au démarrage.
- ⚡ Surveillance non bloquante du processus de jeu, extraction d'archives durcie et écritures atomiques.

### Nouveautés v2.7
- 🌐 Internationalisation (i18n) complète (Français/Anglais).
- 🎨 Personnalisation : Nouveaux thèmes dynamiques interchangeables.
- 📥 Nouveau bouton d'installation du pack de langue WoW (enUS).
- 🛠️ Gestion HD Correction de bugs
- 🚀 Performance : Optimisation majeure avec Qt 6.8.

### Nouveautés v2.5
- 🛠️ **Gestion modulaire HD** : Activez ou désactivez les éléments du patch HD (Arbres, Eau, Sorts, etc.) directement depuis les réglages.
- 🔍 **Détection intelligente** : Le launcher détecte maintenant si le patch HD est déjà installé pour éviter les erreurs de manipulation.
- ⚙️ **Stabilité du Build** : Amélioration du système de compilation sur Windows pour une meilleure robustesse.

## Téléchargement

- 🌐 [Télécharger depuis le site officiel](https://sylvania-servergame.com/launcher)
- 📦 [Release GitHub v2.8 — `SylvaniaLauncher_v2.8.zip`](https://github.com/BlaMacfly/SylvaniaLauncher/releases/download/v2.8/SylvaniaLauncher_v2.8.zip)

## Configuration requise

- Windows 10/11 (64-bit)
- 100 MB d'espace disque (launcher uniquement)
- Connexion Internet

### Build (Windows)
```bash
# Se placer dans le dossier source
cd cpp

# Créer un dossier de build
mkdir build
cd build

# Configurer avec CMake (Qt 6.8 recommandé)
cmake .. -G "MinGW Makefiles"

# Compiler
mingw32-make -j8
```

### Déploiement
Pour créer un package exécutable avec toutes ses dépendances :
```bash
windeployqt --compiler-runtime --multimedia SylvaniaLauncher.exe
```

> [!TIP]
> Pour réduire la taille du package (environ -15 Mo), vous pouvez supprimer manuellement les DLL FFmpeg (`avcodec-*.dll`, `avformat-*.dll`, etc.) après le déploiement. Le launcher est configuré pour utiliser le moteur natif Windows Media si ces fichiers sont absents.

## Licence

Ce projet est sous licence **GNU GPL v3**. Vous êtes libre de copier, modifier et redistribuer ce logiciel tant que vous conservez cette même licence.

Voir le fichier [LICENSE](LICENSE) pour plus de détails.

## Contribuer

Les contributions sont les bienvenues ! 
1. Forkez le projet
2. Créez votre branche (`git checkout -b feature/AmazingFeature`)
3. Committez vos changements (`git commit -m 'Add some AmazingFeature'`)
4. Pushez vers la branche (`git push origin feature/AmazingFeature`)
5. Ouvrez une Pull Request

## Auteur

© 2025 Sylvania - [sylvania-servergame.com](https://sylvania-servergame.com)
