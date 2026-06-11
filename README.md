# Sylvania Launcher — Édition Windows 🪟

![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)
![Version](https://img.shields.io/badge/version-v3.0-success)

🎮 **Launcher officiel du serveur Sylvania — WoW 3.3.5a (WotLK) et WoW Legion 7.3.5, deux launchers en un**

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

- 🔁 **2 launchers en 1** : bascule WotLK 3.3.5a ⇄ Legion 7.3.5 sans redémarrage (logo, icône de barre des tâches, fond, accent, realmlist, stats et addons suivent l'édition active)
- 🎮 **Lancement du jeu** avec configuration automatique du realmlist (3.3.5) ou du portal (Legion)
- 📥 **Bouton unique Installer / Jouer** : détecte le client de l'édition active et enchaîne automatiquement Installer → Téléchargement → Jouer
- 💎 **Patch HD Sylvania** (WotLK uniquement) : installation automatique du patch haute définition (Data, Interface)
- 🧬 **ARAC** (All Races All Classes) : activable/désactivable
- 🖼️ **Fond d'écran aléatoire** (WotLK) : option ON/OFF — Legion a son thème fel dédié
- ⚙️ **Auto-Configuration** : génération automatique du `Config.wtf` adapté à l'édition
- 📊 **Statistiques de jeu** par édition : temps de jeu, nombre de lancements
- 📝 **Notes personnelles** style post-it avec couleurs, **calendrier** et **rappels** (notifications Windows natives + son)
- 🧩 **Gestionnaire d'AddOns** par édition : addons recommandés en un clic, catalogue (3.3.5), suppression des addons installés
- ⚙️ **Réglages** : chemin WoW (sélection libre), cache, AddOns, sons, taille de fenêtre
- 🔄 **Gestion des serveurs** : ajouter, modifier, supprimer

## Version actuelle

**v3.0** - Application native C++ / Qt6

### Nouveautés v3.0
- 🔁 **Support WoW Legion 7.3.5** : nouvelle notion d'« édition de jeu » (`GameEdition`) — chemins, realmlist/portal, téléchargement, manifeste d'addons et thème distincts par édition, bascule in-process depuis l'interface.
- 🎯 **Bouton principal à 3 états** : « Installer » quand le client de l'édition active est absent, progression pendant le téléchargement, « Jouer » quand il est détecté. Le bouton « Télécharger » redondant a été retiré.
- 📅 **Notes enrichies** : calendrier avec décoration des jours portant une note ou un rappel, rappels datés par note, notifications système natives (icône de zone de notification) + son, rattrapage des rappels manqués au démarrage.
- 🎨 **Refonte de l'interface** : système de boutons centralisé à 3 niveaux (primaire/secondaire/tertiaire), échelle d'espacement unique, disposition stable entre les deux éditions, navigation clavier ; thème sombre et thèmes dynamiques conservés.
- 🔐 **Téléchargement Legion sécurisé** : flux `.tar.gz` écrit directement sur disque, vérification d'espace libre, intégrité taille + SHA-256 **bloquante** (pas de hash attendu ⇒ pas d'extraction), garde anti path-traversal sur les entrées de l'archive.

> [!NOTE]
> Client Legion entièrement configuré : URL, taille + SHA-256 du `Legion7.3.5.tar.gz` (intégrité vérifiée avant extraction), portal d'authentification (`164.132.43.2`, écrit dans `WTF/Config.wtf`) et exécutable (`Wow.exe`). Seul le manifeste d'addons recommandés Legion reste un placeholder (repli sur une liste embarquée vide) — à pointer vers la vraie URL côté serveur quand elle existera.

### Nouveautés v2.9
- 🐛 **Corrections de bugs** : fuites de `QNetworkReply`/`QProcess`, course sur le suivi du processus de jeu, et écriture atomique du `Config.wtf` lors du changement de langue.
- 🖼️ **Nouveau logo** : icône de l'exécutable resynchronisée avec le logo Sylvania actuel (plus aucune ancienne icône).
- 🗑️ **Suppression d'addons** : onglet « Installés » listant les addons (nom via `.toc`, version, taille) avec sélection multiple et suppression. Les addons posés par le launcher (multi-dossiers, ex. ConsolePortLK) sont retirés d'un seul coup grâce au registre local.
- 🪟 **Fenêtre redimensionnable** : fenêtre responsive, taille minimale cohérente, géométrie sauvegardée/restaurée (et bornée à l'écran visible), gestion High-DPI, et bouton « Réinitialiser la taille de la fenêtre ».
- 🎮 **Addons recommandés (ConsolePortLK & HealBot)** : nouvel onglet piloté par un manifeste JSON distant (avec copie embarquée de secours). Installation/mise à jour/désinstallation en un clic, barre de progression, états *Non installé / Installé (vX) / Mise à jour dispo*. Ajouter un addon = éditer le manifeste côté serveur, sans recompiler.

> [!NOTE]
> Côté serveur, déposer le manifeste `addons.json` à l'adresse `https://sylvania-servergame.com/launcher/addons.json` (les archives des addons sont déjà servies par `download_addon.php`). En son absence, le launcher utilise la copie embarquée.

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
- 📦 [Releases GitHub](https://github.com/BlaMacfly/SylvaniaLauncher/releases) (dernière version : v3.0)

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

© 2026 Sylvania - [sylvania-servergame.com](https://sylvania-servergame.com)
