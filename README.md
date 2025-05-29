# Sylvania Launcher - World of Warcraft 3.3.5

Un launcher immersif et moderne pour World of Warcraft 3.3.5, développé en Python avec PySide6.

![Sylvania Launcher](Asset/sylvania_logo.png)

## Fonctionnalités

- **Téléchargement automatique** du client WoW 3.3.5 depuis le serveur Sylvania
- **Interface immersive** avec fond animé et effets sonores
- **Gestion du realmlist** pour se connecter facilement au serveur
- **Reprise automatique** des téléchargements en cas d'interruption
- **Design inspiré** de l'univers World of Warcraft

## Installation

### Prérequis

- Python 3.8 ou supérieur
- pip (gestionnaire de paquets Python)

### Installation des dépendances

```bash
pip install -r requirements.txt
```

Les dépendances principales sont :
- PySide6 - Framework d'interface graphique
- requests - Gestion des téléchargements HTTP
- tqdm - Barre de progression pour la console

## Utilisation

Pour lancer l'application, exécutez simplement :

```bash
python main.py
```

### Premier lancement

Lors du premier lancement, le launcher vous proposera de télécharger le client WoW 3.3.5 si celui-ci n'est pas déjà installé. Vous devrez sélectionner un dossier d'installation.

### Configuration

Dans les paramètres, vous pouvez :
- Modifier l'emplacement du dossier WoW
- Changer le realmlist pour vous connecter à différents serveurs
- Activer/désactiver les mises à jour automatiques

## Structure du projet

- `main.py` - Point d'entrée de l'application
- `gui.py` - Interface graphique principale
- `downloader.py` - Gestion des téléchargements et extraction
- `config.py` - Gestion de la configuration
- `Asset/` - Ressources graphiques (icônes, fond d'écran)
- `Sounds/` - Effets sonores

## Compilation en exécutable

Pour créer un exécutable Windows (.exe), vous pouvez utiliser PyInstaller :

```bash
pip install pyinstaller
pyinstaller --name "SylvaniaLauncher" --windowed --icon=Asset/sylvania_logo.ico --add-data "Asset;Asset" --add-data "Sounds;Sounds" main.py
```

L'exécutable sera créé dans le dossier `dist/SylvaniaLauncher/`.

## Licence

Ce projet est destiné à un usage privé pour le serveur Sylvania WoW 3.3.5.
