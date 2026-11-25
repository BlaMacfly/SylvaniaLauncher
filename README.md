# Sylvania Launcher — Édition Windows 🪟

![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)
![Version](https://img.shields.io/badge/version-v3.0-success)

🎮 **Launcher officiel du serveur Sylvania — World of Warcraft 3.3.5a (WotLK)**

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
- 📥 **Bouton unique Installer / Jouer** : détecte le client et enchaîne automatiquement Installer → Téléchargement → Jouer
- 💎 **Patch HD Sylvania** : installation automatique du patch haute définition (Data, Interface)
- 🧬 **ARAC** (All Races All Classes) : activable/désactivable
- 🖼️ **Fond d'écran aléatoire** : option ON/OFF
- ⚙️ **Auto-Configuration** : génération automatique du `Config.wtf`
- 📊 **Statistiques de jeu** : temps de jeu, nombre de lancements
- 📝 **Notes personnelles** style post-it avec couleurs, **calendrier** et **rappels** (notifications Windows natives + son)
- 🧩 **Gestionnaire d'AddOns** : addons recommandés en un clic, catalogue, suppression des addons installés
- ⚙️ **Réglages** : chemin WoW (sélection libre), cache, AddOns, sons, taille de fenêtre
- 🔄 **Gestion des serveurs** : ajouter, modifier, supprimer

## Version actuelle

**v3.0** - Application native Rust (egui) — un seul exécutable, sans DLL

### Nouveautés v3.0
- 🦀 **Réécriture complète en Rust** (egui/eframe) à la place de C++/Qt : un unique `SylvaniaLauncher.exe` qui embarque fonds, logo, sons et traductions — plus aucune DLL Qt/FFmpeg à déployer. Extraction des archives en Rust pur (plus de PowerShell). Configuration, notes, statistiques et addons existants repris tels quels.
- 🎯 **Bouton principal à 3 états** : « Installer » quand le client est absent, progression pendant le téléchargement, « Jouer » quand il est détecté. Le bouton « Télécharger » redondant a été retiré.
- 📅 **Notes enrichies** : calendrier avec décoration des jours portant une note ou un rappel, rappels datés par note, notifications système natives (icône de zone de notification) + son, rattrapage des rappels manqués au démarrage.
- 🎨 **Refonte de l'interface** : système de boutons centralisé à 3 niveaux (primaire/secondaire/tertiaire), échelle d'espacement unique, navigation clavier ; thème sombre et thèmes dynamiques conservés.
- 📥 **Téléchargement du client fiabilisé** : flux écrit directement sur disque, vérification d'espace libre, téléchargement segmenté avec reprise et contrôle d'intégrité taille + SHA-256 quand ils sont connus.
- 🗑️ **Retrait du support Legion 7.3.5** : le launcher est de nouveau dédié à WotLK 3.3.5a. Les configurations et statistiques v3.0 sont migrées automatiquement (seules les données WotLK sont conservées).

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
Prérequis : [Rust](https://rustup.rs) (toolchain MSVC stable) et les Build Tools de Visual Studio.
```bash
# Depuis la racine du dépôt
cargo build --release

# Tests unitaires
cargo test
```
L'exécutable est produit dans `target/release/SylvaniaLauncher.exe`. Il est autonome : il suffit de le distribuer seul (assets et traductions sont intégrés).

### Organisation du code
- `src/core/` — logique métier sans interface (notes, rappels, addons, patch HD, téléchargement du client)
- `src/ui/` — fenêtres egui (principale, réglages, notes, serveurs, addons, téléchargement)
- `src/config.rs`, `src/stats.rs`, `src/i18n.rs`, `src/theme.rs` — configuration, statistiques, traductions FR/EN (`translations/en.json`), thèmes
- `Asset/` — images et sons intégrés à la compilation ; `resources/addons.json` — manifeste d'addons de secours

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
