#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""
GUI module for Sylvania Launcher - Simplified version
Handles the graphical user interface for the launcher
"""

import os
import sys
import time
import json
import logging
import webbrowser
import subprocess
import zipfile
import requests
import threading
from pathlib import Path
from game_tracker import GameTimeTracker
from sound_manager import SoundManager
from version import VERSION, VERSION_INFO

from PySide6.QtCore import (
    Qt, QUrl, QSize, QSizeF, QTimer, Signal, Slot
)
from PySide6.QtMultimedia import (
    QMediaPlayer, QAudioOutput
)
from PySide6.QtGui import (
    QIcon, QPixmap, QColor, QPainter, QBrush, QPen, QFont, QFontMetrics, QAction
)
from PySide6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QLabel, QPushButton, QFileDialog, QMessageBox, QProgressBar,
    QSystemTrayIcon, QMenu, QDialog, QLineEdit, QFormLayout, QCheckBox,
    QStackedWidget, QSizePolicy, QSpacerItem, QGroupBox, QSpinBox,
    QButtonGroup, QRadioButton, QFrame
)

from config import Config
# L'importation d'aria2_downloader a été supprimée car nous utilisons maintenant un téléchargement direct

# Get the absolute path to the assets directory
def get_assets_dir():
    """Obtient le chemin des assets, fonctionne en développement et après compilation"""
    # En mode développement
    if os.path.exists(os.path.join(os.path.dirname(os.path.abspath(__file__)), "Asset")):
        return os.path.join(os.path.dirname(os.path.abspath(__file__)), "Asset")
    
    # En mode compilé (PyInstaller)
    base_path = getattr(sys, '_MEIPASS', os.path.dirname(os.path.abspath(__file__)))
    if os.path.exists(os.path.join(base_path, "Asset")):
        return os.path.join(base_path, "Asset")
    
    # Dernier recours: chercher dans le répertoire parent
    if os.path.exists(os.path.join(os.path.dirname(base_path), "Asset")):
        return os.path.join(os.path.dirname(base_path), "Asset")
    
    # Si tout échoue, retourner le chemin par défaut et logger l'erreur
    print(f"ATTENTION: Impossible de trouver le dossier Asset. Chemins essayés: \n" 
          f"1. {os.path.join(os.path.dirname(os.path.abspath(__file__)), 'Asset')}\n"
          f"2. {os.path.join(base_path, 'Asset')}\n"
          f"3. {os.path.join(os.path.dirname(base_path), 'Asset')}")
    return os.path.join(os.path.dirname(os.path.abspath(__file__)), "Asset")

ASSETS_DIR = get_assets_dir()

# La classe DownloadProgressDialog a été supprimée car elle a été remplacée
# par la classe DownloadDialog dans le module download_client.py


class WowButton(QPushButton):
    """Custom button styled like World of Warcraft buttons"""
    
    def __init__(self, text, parent=None):
        super().__init__(text, parent)
        self.setMinimumHeight(40)
        self.setCursor(Qt.PointingHandCursor)
        self.setStyleSheet("""
            QPushButton {
                background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, 
                                                stop:0 #5a4a2d, stop:0.5 #3a2a1d, stop:1 #2a1a0d);
                color: #e0c080;
                border: 2px solid #7a6a4d;
                border-radius: 5px;
                padding: 5px 15px;
                text-align: center;
                font-size: 14px;
                font-weight: bold;
            }
            QPushButton:hover {
                background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, 
                                                stop:0 #7a6a4d, stop:0.5 #5a4a2d, stop:1 #3a2a1d);
                border: 2px solid #9a8a6d;
            }
            QPushButton:pressed {
                background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, 
                                                stop:0 #3a2a1d, stop:0.5 #2a1a0d, stop:1 #1a0a00);
                border: 2px solid #5a4a2d;
            }
            QPushButton:disabled {
                background-color: #444444;
                color: #888888;
                border: 2px solid #555555;
            }
        """)


class MainWindow(QMainWindow):
    """Main window for the launcher"""
    
    def __init__(self):
        """Initialize the main window"""
        super().__init__()
        
        # Initialize configuration
        self.config = Config()
        
        # Le téléchargement BitTorrent a été remplacé par un téléchargement direct
        # self.torrent_downloader = Aria2Downloader(self.config)
        
        # Initialize game time tracker
        self.game_tracker = GameTimeTracker()
        
        # Initialize sound manager
        self.sound_manager = SoundManager()
        
        # Le téléchargement BitTorrent a été remplacé par un téléchargement direct
        # self.torrent_downloader = Aria2Downloader(self.config)
        
        # Variables pour le suivi du temps de jeu
        self.game_process = None
        self.start_time = None
        self.is_game_running = False
        
        # Set up window properties
        self.setWindowTitle("Sylvania Launcher - World of Warcraft 3.3.5")
        self.setWindowIcon(QIcon(os.path.join(ASSETS_DIR, "sylvania_logo.ico")))
        self.setMinimumSize(900, 600)
        # Utiliser un chemin absolu pour l'image de fond avec la bonne casse
        background_path = os.path.join(ASSETS_DIR, "Background.jpg").replace("\\", "/")
        self.setStyleSheet(f"""
            QMainWindow {{
                background-image: url({background_path});
                background-position: center;
                background-repeat: no-repeat;
                background-color: #000000;
            }}
            QLabel {{
                color: #e0c080;
                font-size: 14px;
                font-weight: bold;
            }}
            QProgressBar {{
                border: 1px solid #7a6a4d;
                border-radius: 3px;
                background-color: #2a1a0d;
                color: #e0c080;
                text-align: center;
            }}
            QProgressBar::chunk {{
                background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, 
                                              stop:0 #e0c080, stop:1 #7a6a4d);
            }}
        """)
        
        # Set up UI
        self.setup_ui()
        
        # Connect downloader signals
        self.connect_downloader_signals()
        
        # Check if WoW is installed
        self.check_wow_installed()
    
    def setup_ui(self):
        """Set up the main window UI"""
        # Définir la taille de la fenêtre principale
        self.setMinimumSize(1024, 768)
        
        # Créer un widget central
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        
        # Créer un layout principal
        main_layout = QVBoxLayout(central_widget)
        main_layout.setContentsMargins(20, 20, 20, 20)
        
        # Définir l'image d'arrière-plan sur la fenêtre principale
        background_path = os.path.join(ASSETS_DIR, "Background.jpg")
        # Convertir les backslashes en slashes pour les URL CSS
        background_path_url = background_path.replace('\\', '/')
        self.setStyleSheet(f"""
            QMainWindow {{
                background-image: url({background_path_url});
                background-position: center;
                background-repeat: no-repeat;
                background-attachment: fixed;
                background-color: #000000;
            }}
        """)
        
        # Créer un widget semi-transparent pour les éléments d'interface
        ui_container = QWidget()
        ui_container.setStyleSheet("""
            QWidget {
                background-color: rgba(0, 0, 0, 150); 
                border-radius: 10px;
            }
            QLabel#logo_label {
                background-color: transparent;
            }
        """)
        
        # Créer un layout pour les éléments d'interface
        ui_layout = QVBoxLayout(ui_container)
        ui_layout.setContentsMargins(20, 20, 20, 20)
        
        # Ajouter le conteneur d'interface au layout principal
        main_layout.addWidget(ui_container)
        
        # Créer un layout principal pour le contenu
        content_layout = QHBoxLayout()
        
        # Left panel with logo and buttons
        left_panel = QWidget()
        left_panel.setMaximumWidth(250)
        left_panel.setStyleSheet("background-color: transparent;")
        left_layout = QVBoxLayout(left_panel)
        left_layout.setContentsMargins(0, 0, 0, 0)
        
        # Créer un conteneur spécial pour le logo avec un fond transparent
        logo_container = QWidget()
        logo_container.setStyleSheet("background-color: transparent;")
        logo_layout = QVBoxLayout(logo_container)
        logo_layout.setContentsMargins(0, 0, 0, 0)
        
        # Créer un label pour le logo avec un identifiant spécifique
        logo_label = QLabel()
        logo_label.setObjectName("logo_label")
        
        # Charger l'image du logo JPEG
        logo_pixmap = QPixmap(os.path.join(ASSETS_DIR, "sylvania_logo.jpeg"))
        
        # Si l'image existe, l'utiliser
        if not logo_pixmap.isNull():
            # Ajuster la taille du logo pour qu'il s'adapte bien à l'interface
            logo_label.setPixmap(logo_pixmap.scaled(250, 100, Qt.KeepAspectRatio, Qt.SmoothTransformation))
        else:
            # Fallback au cas où l'image ne serait pas trouvée
            logo_label.setText("SYLVANIA WOW")
            logo_label.setStyleSheet("""
                font-size: 24px; 
                color: #FFD700; 
                font-weight: bold; 
                font-family: 'Arial Black', sans-serif;
                text-shadow: 2px 2px 4px #000000;
            """)
        
        # Centrer le logo dans son conteneur
        logo_label.setAlignment(Qt.AlignCenter)
        
        # Ajouter le logo au conteneur transparent
        logo_layout.addWidget(logo_label)
        
        # Ajouter le conteneur transparent au début du layout gauche
        left_layout.addWidget(logo_container)
        
        # Create buttons
        self.play_button = WowButton("Jouer", self)
        self.download_button = WowButton("Télécharger le client", self)
        self.settings_button = WowButton("Réglages", self)
        self.addons_button = WowButton("Addons", self)
        self.quit_button = WowButton("Quitter", self)
        
        # Add buttons to layout
        left_layout.addWidget(self.play_button)
        left_layout.addWidget(self.download_button)
        left_layout.addWidget(self.settings_button)
        left_layout.addWidget(self.addons_button)
        left_layout.addStretch()
        left_layout.addWidget(self.quit_button)
        
        # Right panel with status
        right_panel = QWidget()
        right_layout = QVBoxLayout(right_panel)
        
        # Status label
        self.status_label = QLabel("Bienvenue sur le Launcher Sylvania WoW 3.3.5")
        self.status_label.setAlignment(Qt.AlignCenter)
        self.status_label.setStyleSheet("font-size: 16px; color: #e0c080; font-weight: bold;")
        right_layout.addWidget(self.status_label)
        
        # Section Realmlist (affichage uniquement)
        realmlist_frame = QWidget()
        realmlist_frame.setStyleSheet("""
            background-color: rgba(0, 0, 0, 100);
            border: 1px solid #7a6a4d;
            border-radius: 5px;
            padding: 10px;
        """)
        realmlist_layout = QVBoxLayout(realmlist_frame)
        
        # Titre de la section
        realmlist_title = QLabel("Serveur actuel")
        realmlist_title.setAlignment(Qt.AlignCenter)
        realmlist_title.setStyleSheet("font-size: 14px; color: #e0c080; font-weight: bold;")
        realmlist_layout.addWidget(realmlist_title)
        
        # Récupérer les entrées realmlist
        realmlist_entries = self.config.get_realmlist_entries()
        active_index = self.config.get_active_realmlist_index()
        
        # Affichage du serveur actuel
        self.active_server_label = QLabel()
        self.active_server_label.setAlignment(Qt.AlignCenter)
        self.active_server_label.setStyleSheet("""
            color: #e0c080;
            font-size: 15px;
            font-weight: bold;
            padding: 5px;
            background-color: rgba(58, 42, 29, 100);
            border-radius: 3px;
        """)
        
        # Affichage de l'adresse du serveur
        self.active_server_address = QLabel()
        self.active_server_address.setAlignment(Qt.AlignCenter)
        self.active_server_address.setStyleSheet("""
            color: #a08060;
            font-size: 12px;
            font-style: italic;
            padding: 3px;
        """)
        
        # Mettre à jour les labels avec les informations du serveur actuel
        self._update_server_display()
        
        # Ajouter les labels au layout
        realmlist_layout.addWidget(self.active_server_label)
        realmlist_layout.addWidget(self.active_server_address)
        
        # Ajouter un lien vers les réglages
        settings_link = QLabel("<a href='#' style='color: #a08060; text-decoration: none;'>Changer de serveur</a>")
        settings_link.setAlignment(Qt.AlignCenter)
        settings_link.setOpenExternalLinks(False)
        settings_link.linkActivated.connect(self._on_settings_button_clicked)
        realmlist_layout.addWidget(settings_link)
        
        # Ajouter la section realmlist au layout principal
        right_layout.addWidget(realmlist_frame)
        
        # Section des statistiques de jeu
        stats_frame = QWidget()
        stats_frame.setStyleSheet("""
            background-color: rgba(0, 0, 0, 100);
            border: 1px solid #7a6a4d;
            border-radius: 5px;
            padding: 10px;
        """)
        stats_layout = QVBoxLayout(stats_frame)
        
        # Titre de la section
        stats_title = QLabel("Statistiques de jeu")
        stats_title.setAlignment(Qt.AlignCenter)
        stats_title.setStyleSheet("font-size: 14px; color: #e0c080; font-weight: bold;")
        stats_layout.addWidget(stats_title)
        
        # Temps de jeu
        self.playtime_label = QLabel(f"Temps de jeu: {self.game_tracker.get_formatted_time()}")
        self.playtime_label.setStyleSheet("color: #e0c080;")
        stats_layout.addWidget(self.playtime_label)
        
        # Nombre de lancements
        self.launches_label = QLabel(f"Lancements: {self.game_tracker.get_launches()}")
        self.launches_label.setStyleSheet("color: #e0c080;")
        stats_layout.addWidget(self.launches_label)
        
        # Dernière session
        self.last_played_label = QLabel(f"Dernière session: {self.game_tracker.get_last_played()}")
        self.last_played_label.setStyleSheet("color: #e0c080;")
        stats_layout.addWidget(self.last_played_label)
        
        # Ajouter la section des statistiques au layout principal
        right_layout.addWidget(stats_frame)
        
        # Nous gardons les éléments de progression pour la compatibilité avec le code existant,
        # mais nous les masquons pour qu'ils ne soient pas visibles dans l'interface principale
        
        # Créer des widgets invisibles pour la compatibilité avec le code existant
        self.download_progress = QProgressBar()
        self.download_progress.setVisible(False)  # Masquer la barre de progression
        
        self.speed_label = QLabel("Vitesse: 0 KB/s")
        self.speed_label.setVisible(False)  # Masquer l'étiquette de vitesse
        
        self.peers_label = QLabel("Pairs: 0")
        self.peers_label.setVisible(False)  # Masquer l'étiquette de pairs
        right_layout.addStretch()
        
        # Add panels to content layout
        content_layout.addWidget(left_panel)
        content_layout.addWidget(right_panel, 1)  # Right panel takes more space
        
        # Add content layout to UI layout
        ui_layout.addLayout(content_layout, 1)
        
        # Footer with version
        footer_layout = QHBoxLayout()
        footer_layout.addStretch()
        version_label = QLabel(VERSION_INFO)
        version_label.setStyleSheet("color: #e0c080; font-size: 12px;")
        footer_layout.addWidget(version_label)
        ui_layout.addLayout(footer_layout)
        
        # Connect button signals
        self.connect_button_signals()
    
    def connect_button_signals(self):
        """Connect button signals to their slots"""
        # Connecter le bouton Jouer avec son son spécifique
        self.play_button.clicked.connect(self._on_play_button_clicked)
        
        # Connecter les autres boutons avec le son générique
        self.download_button.clicked.connect(self._on_download_button_clicked)
        self.settings_button.clicked.connect(self._on_settings_button_clicked)
        self.addons_button.clicked.connect(self._on_addons_button_clicked)
        self.quit_button.clicked.connect(self._on_quit_button_clicked)
    
    def _on_play_button_clicked(self):
        """Gérer le clic sur le bouton Jouer avec son son spécifique"""
        self.sound_manager.play_sound("play")
        self.play_game()
    
    def _on_download_button_clicked(self):
        """Gérer le clic sur le bouton Télécharger avec son son générique"""
        self.sound_manager.play_sound("button")
        self.download_client()
        
    def download_client(self):
        """Télécharger le client WoW en utilisant le module download_client"""
        try:
            # Importer le module de téléchargement
            from download_client import DownloadDialog
            
            # Jouer le son du bouton
            self.sound_manager.play_sound("button")
            
            # Sélectionner un emplacement pour installer le client
            directory = QFileDialog.getExistingDirectory(
                self, "Sélectionner un emplacement pour installer WoW", 
                os.path.expanduser("~")
            )
            
            if not directory:
                return  # L'utilisateur a annulé la sélection
            
            # Enregistrer l'emplacement dans la configuration
            self.config.set("wow_path", directory)
            self.config.save()
            
            # Mettre à jour le statut
            self.status_label.setText("Préparation du téléchargement...")
            
            # Créer et afficher la fenêtre de téléchargement en passant l'emplacement déjà sélectionné
            download_dialog = DownloadDialog(self, destination=directory)
            result = download_dialog.exec_()
            
            # Après le téléchargement, vérifier si WoW est installé
            self.check_wow_installed()
            
            # Mettre à jour le statut en fonction du résultat
            if result == QDialog.Accepted:
                self.status_label.setText("Téléchargement terminé avec succès!")
            else:
                self.status_label.setText("Téléchargement annulé ou incomplet")
        except Exception as e:
            # Afficher un message d'erreur en cas de problème
            QMessageBox.critical(
                self,
                "Erreur de téléchargement",
                f"Une erreur est survenue lors du téléchargement: {str(e)}"
            )
            logging.error(f"Erreur de téléchargement: {str(e)}")
            self.status_label.setText("Erreur lors du téléchargement du client")
            
            with open(zip_path, 'wb') as f:
                for data in response.iter_content(block_size):
                    # Vérifier si l'annulation a été demandée
                    if self.download_info.get("should_cancel", False):
                        raise Exception("Téléchargement annulé par l'utilisateur")
                        
                    if not data:
                        continue
                        
                    f.write(data)
                    downloaded += len(data)
                    downloaded_mb = downloaded / (1024 * 1024)
                    
                    # Calculer la vitesse de téléchargement
                    elapsed = time.time() - start_time
                    if elapsed > 0:
                        speed = downloaded / elapsed / 1024  # KB/s
                        QTimer.singleShot(0, lambda s=speed: self.progress_dialog.update_speed(f"Vitesse: {s:.1f} KB/s"))
                    
                    # Mettre à jour la barre de progression
                    QTimer.singleShot(0, lambda d=downloaded_mb: self.progress_dialog.update_progress(int(d)))
                    QTimer.singleShot(0, lambda d=downloaded_mb, t=total_size_mb: 
                                    self.progress_dialog.update_progress(int(d), int(t), f"{int(d * 100 / t)}% - {d:.1f} / {t:.1f} MB"))
                    
                    # Mettre à jour le statut
                    QTimer.singleShot(0, lambda d=downloaded_mb, t=total_size_mb: 
                                    self.progress_dialog.update_status(f"Téléchargement: {d:.1f} MB / {t:.1f} MB"))
            
            # Vérifier si l'annulation a été demandée
            if self.download_info.get("should_cancel", False):
                raise Exception("Téléchargement annulé par l'utilisateur")
            
            # Extraire le fichier ZIP
            QTimer.singleShot(0, lambda: self.progress_dialog.update_status("Extraction du client WoW..."))
            QTimer.singleShot(0, lambda: self.progress_dialog.update_progress(0, 100, "0% - 0 / 0 fichiers"))
            QTimer.singleShot(0, lambda: self.progress_dialog.update_speed("Extraction en cours..."))
            
            with zipfile.ZipFile(zip_path, 'r') as zip_ref:
                # Compter le nombre total de fichiers
                file_list = zip_ref.namelist()
                total_files = len(file_list)
                extracted = 0
                
                # Extraire chaque fichier avec progression
                for file in file_list:
                    # Vérifier si l'annulation a été demandée
                    if self.download_info.get("should_cancel", False):
                        raise Exception("Extraction annulée par l'utilisateur")
                        
                    zip_ref.extract(file, destination)
                    extracted += 1
                    progress = int(100 * extracted / total_files)
                    
                    # Mettre à jour la barre de progression toutes les 10 fichiers pour éviter de surcharger l'interface
                    if extracted % 10 == 0 or extracted == total_files:
                        QTimer.singleShot(0, lambda p=progress: self.progress_dialog.update_progress(p))
                        QTimer.singleShot(0, lambda e=extracted, t=total_files: 
                                        self.progress_dialog.update_progress(None, None, f"{int(e * 100 / t)}% - {e} / {t} fichiers"))
                        QTimer.singleShot(0, lambda e=extracted, t=total_files: 
                                        self.progress_dialog.update_status(f"Extraction: {e} / {t} fichiers"))
            
            # Supprimer le fichier ZIP temporaire
            os.remove(zip_path)
            
            # Mettre à jour le statut final
            QTimer.singleShot(0, lambda: self.progress_dialog.update_status("Installation terminée avec succès!"))
            QTimer.singleShot(0, lambda: self.progress_dialog.update_progress(100, None, "100% - Installation complète"))
            QTimer.singleShot(0, lambda: self.progress_dialog.update_speed("Terminé"))
            
            # Fermer la fenêtre de progression après un court délai
            QTimer.singleShot(2000, lambda: self.progress_dialog.accept() if hasattr(self, "progress_dialog") else None)
            
            # Mettre à jour l'interface principale pour indiquer que WoW est installé
            QTimer.singleShot(0, self.check_wow_installed)
            
        except Exception as e:
            error_msg = f"Erreur lors du téléchargement ou de l'extraction: {str(e)}"
            logging.error(error_msg)
            
            # Afficher l'erreur dans la fenêtre de progression si elle est ouverte
            if hasattr(self, "progress_dialog") and self.progress_dialog.isVisible():
                QTimer.singleShot(0, lambda: self.progress_dialog.update_status(error_msg))
                QTimer.singleShot(0, lambda: self.progress_dialog.update_progress(0))
                QTimer.singleShot(0, lambda: self.progress_dialog.update_speed("Erreur"))
                
                # Fermer la fenêtre après un court délai
                QTimer.singleShot(3000, lambda: self.progress_dialog.reject() if hasattr(self, "progress_dialog") else None)
            
            # Afficher l'erreur dans la fenêtre principale
            QTimer.singleShot(0, lambda: self.status_label.setText(error_msg))
    
    def _on_download_cancelled(self):
        """Gérer l'annulation du téléchargement"""
        if hasattr(self, "download_info"):
            self.download_info["should_cancel"] = True
            self.status_label.setText("Téléchargement annulé")
            
            # Fermer la fenêtre de progression
            if hasattr(self, "progress_dialog") and self.progress_dialog.isVisible():
                self.progress_dialog.close()
            
            # Mettre à jour le statut
            self.progress_dialog.update_status(f"Téléchargement en cours... {self.progress_value}%")
    
    def _on_download_cancelled(self):
        """Gérer l'annulation du téléchargement"""
        # Arrêter le timer de progression
        if hasattr(self, "progress_timer") and self.progress_timer.isActive():
            self.progress_timer.stop()
        
        # Arrêter le téléchargement
        self.torrent_downloader.stop_download()
        
        # Afficher un message
        self.status_label.setText("Téléchargement annulé")
    
    def _on_download_complete(self, success, message):
        """Gérer la fin du téléchargement"""
        if success:
            # Mettre à jour l'interface pour indiquer que WoW est installé
            self.check_wow_installed()
            
            # Afficher un message de succès
            QMessageBox.information(self, "Téléchargement terminé", "Le client WoW a été téléchargé et installé avec succès.")
        else:
            # Afficher un message d'erreur
            QMessageBox.critical(self, "Erreur", f"Erreur lors du téléchargement: {message}")
        
        # Fermer la fenêtre de progression si elle est encore ouverte
        if hasattr(self, "progress_dialog") and self.progress_dialog.isVisible():
            self.progress_dialog.accept()
    
    def _download_and_extract(self, url, destination):
        """Télécharger et extraire le client WoW"""
        try:
            # Créer le dossier de destination s'il n'existe pas
            if not os.path.exists(destination):
                os.makedirs(destination)
            
            # Nom du fichier ZIP temporaire
            zip_path = os.path.join(destination, "wow_client.zip")
            
            # Préparation du téléchargement
            QTimer.singleShot(0, lambda: self.progress_dialog.update_status("Préparation du téléchargement..."))
            QTimer.singleShot(0, lambda: self.progress_dialog.update_progress(0, 100, "0% - 0 / 100 MB"))
            QTimer.singleShot(0, lambda: self.progress_dialog.update_speed("Vitesse: 0 KB/s"))
            QTimer.singleShot(0, lambda: self.progress_dialog.update_peers("Pairs: 0"))
            
            # Télécharger avec progression
            response = requests.get(url, stream=True)
            total_size = int(response.headers.get('content-length', 0))
            total_size_mb = total_size / (1024 * 1024)  # Convertir en MB
            block_size = 1024 * 1024  # 1 MB
            downloaded = 0
            start_time = time.time()
            
            # Configurer la barre de progression pour le téléchargement
            QTimer.singleShot(0, lambda: self.progress_dialog.update_progress(0, int(total_size_mb)))
            QTimer.singleShot(0, lambda: self.progress_dialog.update_status("Téléchargement du client WoW..."))
            
            with open(zip_path, 'wb') as f:
                for data in response.iter_content(block_size):
                    # Vérifier si l'annulation a été demandée
                    if self.download_info.get("should_cancel", False):
                        raise Exception("Téléchargement annulé par l'utilisateur")
                        
                    if not data:
                        continue
                        
                    f.write(data)
                    downloaded += len(data)
                    downloaded_mb = downloaded / (1024 * 1024)
                    
                    # Calculer la vitesse de téléchargement
                    elapsed = time.time() - start_time
                    if elapsed > 0:
                        speed = downloaded / elapsed / 1024  # KB/s
                        QTimer.singleShot(0, lambda s=speed: self.progress_dialog.update_speed(f"Vitesse: {s:.1f} KB/s"))
                    
                    # Mettre à jour la barre de progression
                    QTimer.singleShot(0, lambda d=downloaded_mb: self.progress_dialog.update_progress(int(d)))
                    QTimer.singleShot(0, lambda d=downloaded_mb, t=total_size_mb: 
                                    self.progress_dialog.update_progress(int(d), None, f"{int(d * 100 / t)}% - {d:.1f} / {t:.1f} MB"))
                    
                    # Mettre à jour le statut
                    QTimer.singleShot(0, lambda d=downloaded_mb, t=total_size_mb: 
                                    self.progress_dialog.update_status(f"Téléchargement: {d:.1f} MB / {t:.1f} MB"))
            
            # Vérifier si l'annulation a été demandée
            if self.download_info.get("should_cancel", False):
                raise Exception("Téléchargement annulé par l'utilisateur")
            
            # Extraire le fichier ZIP
            QTimer.singleShot(0, lambda: self.progress_dialog.update_status("Extraction du client WoW..."))
            QTimer.singleShot(0, lambda: self.progress_dialog.update_progress(0, 100, "0% - 0 / 0 fichiers"))
            QTimer.singleShot(0, lambda: self.progress_dialog.update_speed("Extraction en cours..."))
            
            with zipfile.ZipFile(zip_path, 'r') as zip_ref:
                # Compter le nombre total de fichiers
                file_list = zip_ref.namelist()
                total_files = len(file_list)
                extracted = 0
                
                # Extraire chaque fichier avec progression
                for file in file_list:
                    # Vérifier si l'annulation a été demandée
                    if self.download_info.get("should_cancel", False):
                        raise Exception("Extraction annulée par l'utilisateur")
                        
                    zip_ref.extract(file, destination)
                    extracted += 1
                    progress = int(100 * extracted / total_files)
                    
                    # Mettre à jour la barre de progression toutes les 10 fichiers pour éviter de surcharger l'interface
                    if extracted % 10 == 0 or extracted == total_files:
                        QTimer.singleShot(0, lambda p=progress: self.progress_dialog.update_progress(p))
                        QTimer.singleShot(0, lambda e=extracted, t=total_files: 
                                        self.progress_dialog.update_progress(None, None, f"{int(e * 100 / t)}% - {e} / {t} fichiers"))
                        QTimer.singleShot(0, lambda e=extracted, t=total_files: 
                                        self.progress_dialog.update_status(f"Extraction: {e} / {t} fichiers"))
            
            # Supprimer le fichier ZIP temporaire
            os.remove(zip_path)
            
            # Mettre à jour le statut final
            QTimer.singleShot(0, lambda: self.progress_dialog.update_status("Installation terminée avec succès!"))
            QTimer.singleShot(0, lambda: self.progress_dialog.update_progress(100, None, "100% - Installation complète"))
            QTimer.singleShot(0, lambda: self.progress_dialog.update_speed("Terminé"))
            QTimer.singleShot(0, lambda: self.progress_dialog.update_peers(""))
            
            # Fermer la fenêtre de progression après un court délai
            QTimer.singleShot(2000, lambda: self.progress_dialog.accept() if hasattr(self, "progress_dialog") else None)
            
            # Mettre à jour l'interface principale pour indiquer que WoW est installé
            QTimer.singleShot(0, self.check_wow_installed)
            
        except Exception as e:
            error_msg = f"Erreur lors du téléchargement ou de l'extraction: {str(e)}"
            logging.error(error_msg)
            
            # Afficher l'erreur dans la fenêtre de progression si elle est ouverte
            if hasattr(self, "progress_dialog") and self.progress_dialog.isVisible():
                QTimer.singleShot(0, lambda: self.progress_dialog.update_status(error_msg))
                QTimer.singleShot(0, lambda: self.progress_dialog.update_progress(0))
                QTimer.singleShot(0, lambda: self.progress_dialog.update_speed("Erreur"))
                QTimer.singleShot(0, lambda: self.progress_dialog.update_peers(""))
                
                # Fermer la fenêtre après un court délai
                QTimer.singleShot(3000, lambda: self.progress_dialog.reject() if hasattr(self, "progress_dialog") else None)
            
            # Afficher l'erreur dans la fenêtre principale
            QTimer.singleShot(0, lambda: self.status_label.setText(error_msg))
    
    def _on_settings_button_clicked(self):
        """Gérer le clic sur le bouton Réglages avec son son générique"""
        self.sound_manager.play_sound("button")
        self.open_settings()
    
    def _on_addons_button_clicked(self):
        """Gérer le clic sur le bouton Addons avec son son générique"""
        self.sound_manager.play_sound("button")
        self.open_addons()
    
    def _on_quit_button_clicked(self):
        """Gérer le clic sur le bouton Quitter avec son son générique"""
        self.sound_manager.play_sound("button")
        self.close()
    
    def connect_downloader_signals(self):
        """Connect downloader signals to UI updates"""
        # Connecter les signaux à la fenêtre de progression
        if hasattr(self, "progress_dialog"):
            print("Connexion des signaux à la fenêtre de progression...")
            
            # Connecter le signal de progression du téléchargement
            self.torrent_downloader.download_progress.connect(
                self._on_download_progress
            )
            
            # Connecter le signal de vitesse de téléchargement
            self.torrent_downloader.download_speed.connect(
                self._on_download_speed
            )
            
            # Connecter le signal de statut du téléchargement
            self.torrent_downloader.download_status.connect(
                self._on_download_status
            )
            
            # Connecter le signal de nombre de pairs
            self.torrent_downloader.peers_count.connect(
                self._on_peers_count
            )
            
            # Connecter le signal de fin de téléchargement
            self.torrent_downloader.download_complete.connect(self._on_download_complete)
    
    def _on_download_progress(self, current, total):
        """Gérer le signal de progression du téléchargement"""
        print(f"Progression: {current} / {total} MB")
        if hasattr(self, "progress_dialog") and self.progress_dialog.isVisible():
            self.progress_dialog.update_progress(current, total, f"{int(current * 100 / total) if total > 0 else 0}% - {current} / {total} MB")
    
    def _on_download_speed(self, speed):
        """Gérer le signal de vitesse de téléchargement"""
        print(f"Vitesse: {speed:.1f} KB/s")
        if hasattr(self, "progress_dialog") and self.progress_dialog.isVisible():
            self.progress_dialog.update_speed(f"Vitesse: {speed:.1f} KB/s")
    
    def _on_download_status(self, status):
        """Gérer le signal de statut du téléchargement"""
        print(f"Statut: {status}")
        if hasattr(self, "progress_dialog") and self.progress_dialog.isVisible():
            self.progress_dialog.update_status(status)
    
    def _on_peers_count(self, count):
        """Gérer le signal de nombre de pairs"""
        print(f"Pairs: {count}")
        if hasattr(self, "progress_dialog") and self.progress_dialog.isVisible():
            self.progress_dialog.update_peers(f"Pairs: {count}")
    
    def check_wow_installed(self):
        """Check if WoW is installed and update UI accordingly"""
        wow_path = self.config.get("wow_path")
        
        if wow_path and os.path.exists(os.path.join(wow_path, "Wow.exe")):
            # Récupérer le realmlist actif
            active_index = self.config.get_active_realmlist_index()
            realmlist_entries = self.config.get_realmlist_entries()
            if realmlist_entries and len(realmlist_entries) > active_index:
                active_entry = realmlist_entries[active_index]
                if active_entry.get("name"):
                    self.status_label.setText(f"World of Warcraft 3.3.5 est installé. Prêt à jouer sur {active_entry.get('name')}!")
                else:
                    self.status_label.setText("World of Warcraft 3.3.5 est installé. Prêt à jouer!")
            else:
                self.status_label.setText("World of Warcraft 3.3.5 est installé. Prêt à jouer!")
                
            self.play_button.setEnabled(True)
            
            # Mettre à jour l'affichage du serveur actuel
            self._update_server_display()
        else:
            self.status_label.setText("World of Warcraft 3.3.5 n'est pas installé.")
            self.play_button.setEnabled(False)
            
            # Prompt for download after a short delay
            QTimer.singleShot(500, self.prompt_download)
    
    def prompt_download(self):
        """Prompt user to download WoW client"""
        # Create custom message box with simplified options
        msgBox = QMessageBox(self)
        msgBox.setWindowTitle("Client WoW non trouvé")
        msgBox.setText("Le client World of Warcraft 3.3.5 n'a pas été trouvé.")
        msgBox.setInformativeText("Veuillez sélectionner un dossier existant contenant WoW ou choisir un emplacement où installer le client.")
        
        # Add buttons
        browseButton = msgBox.addButton("Parcourir", QMessageBox.ActionRole)
        installButton = msgBox.addButton("Installer", QMessageBox.ActionRole)
        cancelButton = msgBox.addButton("Annuler", QMessageBox.RejectRole)
        
        result = msgBox.exec()
        
        # Handle button clicks
        clicked_button = msgBox.clickedButton()
        
        if clicked_button == browseButton:
            # Chercher un client WoW existant
            QTimer.singleShot(100, self._handle_browse_wow_directory)
                
        elif clicked_button == installButton:
            # Sélectionner un emplacement pour installer le client
            directory = QFileDialog.getExistingDirectory(
                self, "Sélectionner un emplacement pour installer WoW", 
                os.path.expanduser("~")
            )
            
            if directory:
                self.config.set("wow_path", directory)
                self.config.save()
                
                # Mettre à jour le statut
                self.status_label.setText("Préparation du téléchargement...")
                
                # Créer et afficher directement la fenêtre de téléchargement en passant l'emplacement déjà sélectionné
                from download_client import DownloadDialog
                download_dialog = DownloadDialog(self, destination=directory)
                result = download_dialog.exec_()
                
                # Après le téléchargement, vérifier si WoW est installé
                self.check_wow_installed()
                
                # Mettre à jour le statut en fonction du résultat
                if result == QDialog.Accepted:
                    self.status_label.setText("Téléchargement terminé avec succès!")
                else:
                    self.status_label.setText("Téléchargement annulé ou incomplet")
    
    def select_wow_directory(self):
        """Open dialog to select WoW installation directory"""
        directory = QFileDialog.getExistingDirectory(
            self, "Sélectionner le dossier d'installation de WoW", 
            os.path.expanduser("~")
        )
        if directory:
            self.config.set("wow_path", directory)
            
    def _handle_browse_wow_directory(self):
        """Handle browsing for WoW directory after dialog closes"""
        # This method is called after the dialog closes to avoid UI issues
        result = self.browse_wow_directory()
        if result:
            # If we found a valid WoW directory, update UI and enable play button
            self.check_wow_installed()
    
    def browse_wow_directory(self):
        """Browse for an existing WoW installation directory"""
        directory = QFileDialog.getExistingDirectory(
            self, "Sélectionner le dossier d'installation de WoW", 
            os.path.expanduser("~")
        )
        
        if not directory:
            return False
            
        # Check if this is a valid WoW directory
        wow_exe = os.path.join(directory, "Wow.exe")
        if os.path.exists(wow_exe):
            # Valid WoW directory found
            self.config.set("wow_path", directory)
            
            # Update realmlist
            self.config.update_realmlist(self.config.get("realmlist"))
            
            QMessageBox.information(self, "Client trouvé", 
                                 "Le client World of Warcraft 3.3.5 a été trouvé avec succès !")
            return True
        else:
            # Not a valid WoW directory
            QMessageBox.warning(self, "Dossier invalide", 
                              "Le dossier sélectionné ne semble pas contenir une installation valide de World of Warcraft 3.3.5.\n\n"
                              "Veuillez sélectionner un dossier contenant Wow.exe ou télécharger le client.")
            return False
    
    def play_game(self):
        """Launch the game"""
        wow_path = self.config.get("wow_path")
        
        if not wow_path or not os.path.exists(os.path.join(wow_path, "Wow.exe")):
            QMessageBox.warning(self, "Erreur", "World of Warcraft 3.3.5 n'est pas installé.")
            return
        
        # Launch the game
        try:
            # Incrémenter le compteur de lancements
            self.game_tracker.increment_launches()
            
            # Mettre à jour la date de la dernière session
            self.game_tracker.update_last_played()
            
            # Mettre à jour l'interface
            self.update_stats_display()
            
            # Enregistrer l'heure de départ
            self.start_time = time.time()
            
            # Lancer le jeu
            self.game_process = subprocess.Popen(os.path.join(wow_path, "Wow.exe"), cwd=wow_path)
            self.is_game_running = True
            self.status_label.setText("Jeu en cours...")
            
            # Minimiser le launcher
            self.showMinimized()
            
            # Démarrer le timer pour vérifier si le jeu est toujours en cours
            QTimer.singleShot(1000, self.check_game_status)
            
        except Exception as e:
            QMessageBox.critical(self, "Erreur", f"Erreur lors du lancement du jeu: {str(e)}")
    
    def check_game_status(self):
        """Vérifier si le jeu est toujours en cours d'exécution"""
        if not self.is_game_running:
            return
        
        # Vérifier si le processus est toujours en cours d'exécution
        if self.game_process and self.game_process.poll() is not None:
            # Le jeu s'est terminé
            self.is_game_running = False
            
            # Calculer le temps écoulé
            elapsed_time = int(time.time() - self.start_time)
            
            # Ajouter le temps de jeu aux statistiques
            self.game_tracker.add_time(elapsed_time)
            
            # Mettre à jour l'interface
            self.update_stats_display()
            self.status_label.setText("Prêt à jouer!")
            
            # Restaurer la fenêtre du launcher
            self.showNormal()
        else:
            # Le jeu est toujours en cours, vérifier à nouveau dans 1 seconde
            QTimer.singleShot(1000, self.check_game_status)
    
    def update_stats_display(self):
        """Mettre à jour l'affichage des statistiques de jeu"""
        self.playtime_label.setText(f"Temps de jeu: {self.game_tracker.get_formatted_time()}")
        self.launches_label.setText(f"Lancements: {self.game_tracker.get_launches()}")
        self.last_played_label.setText(f"Dernière session: {self.game_tracker.get_last_played()}")
        
    def _update_server_display(self):
        """Mettre à jour l'affichage du serveur actuel"""
        # Vérifier si les labels existent
        if not hasattr(self, "active_server_label") or not hasattr(self, "active_server_address"):
            return
            
        # Récupérer les entrées realmlist
        realmlist_entries = self.config.get_realmlist_entries()
        active_index = self.config.get_active_realmlist_index()
        
        # Mettre à jour les labels
        if active_index < len(realmlist_entries):
            entry = realmlist_entries[active_index]
            server_name = entry.get("name", "Serveur par défaut")
            server_address = entry.get("address", "")
            
            self.active_server_label.setText(server_name)
            self.active_server_address.setText(server_address)
        else:
            self.active_server_label.setText("Aucun serveur configuré")
            self.active_server_address.setText("")
            
    def _update_realmlist_ui(self):
        """Mettre à jour l'interface de realmlist"""
        # Mettre à jour l'affichage du serveur actuel
        self._update_server_display()
                
    # Cette méthode n'est plus nécessaire car nous n'avons plus de boutons radio
    # Elle est conservée pour référence mais n'est plus utilisée
    def _on_realmlist_changed(self, button):
        """Gérer le changement de realmlist (méthode dépréciée)"""
        pass
    
    def open_settings(self):
        """Open settings dialog"""
        # Créer une fenêtre de dialogue pour les réglages
        settings_dialog = QDialog(self)
        settings_dialog.setWindowTitle("Réglages")
        settings_dialog.setMinimumWidth(600)
        settings_dialog.setStyleSheet("""
            QDialog {
                background-color: #1a1a1a;
            }
            QGroupBox {
                border: 1px solid #7a6a4d;
                border-radius: 5px;
                margin-top: 1ex;
                color: #e0c080;
                font-weight: bold;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                subcontrol-position: top center;
                padding: 0 3px;
            }
            QLabel {
                color: #e0c080;
            }
            QLineEdit {
                background-color: #2a2a2a;
                color: #e0c080;
                border: 1px solid #7a6a4d;
                border-radius: 3px;
                padding: 2px;
            }
            QPushButton {
                background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, 
                                            stop:0 #5a4a2d, stop:0.5 #3a2a1d, stop:1 #2a1a0d);
                color: #e0c080;
                border: 1px solid #7a6a4d;
                border-radius: 3px;
                padding: 5px 10px;
            }
            QPushButton:hover {
                background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, 
                                            stop:0 #7a6a4d, stop:0.5 #5a4a2d, stop:1 #3a2a1d);
            }
            QPushButton:pressed {
                background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, 
                                            stop:0 #3a2a1d, stop:0.5 #2a1a0d, stop:1 #1a0a00);
            }
            QPushButton:disabled {
                background-color: #444444;
                color: #888888;
                border: 1px solid #555555;
            }
            QRadioButton {
                color: #e0c080;
            }
            QRadioButton::indicator {
                width: 13px;
                height: 13px;
            }
            QRadioButton::indicator::unchecked {
                border: 1px solid #7a6a4d;
                border-radius: 6px;
                background-color: #2a2a2a;
            }
            QRadioButton::indicator::checked {
                border: 1px solid #7a6a4d;
                border-radius: 6px;
                background-color: #e0c080;
            }
        """)
        
        # Créer un layout pour la fenêtre
        layout = QVBoxLayout(settings_dialog)
        layout.setSpacing(15)
        
        # Section Chemin d'installation
        path_group = QGroupBox("Chemin d'installation de World of Warcraft")
        path_layout = QVBoxLayout(path_group)
        
        # Affichage du chemin actuel
        path_info_layout = QHBoxLayout()
        path_label = QLabel("Chemin actuel:")
        path_info_layout.addWidget(path_label)
        
        self.settings_path_edit = QLineEdit()
        self.settings_path_edit.setText(self.config.get("wow_path", ""))
        self.settings_path_edit.setReadOnly(True)
        path_info_layout.addWidget(self.settings_path_edit, 1)
        
        browse_button = QPushButton("Parcourir")
        browse_button.clicked.connect(self._browse_from_settings)
        path_info_layout.addWidget(browse_button)
        
        path_layout.addLayout(path_info_layout)
        
        # Ajouter des informations supplémentaires
        path_info = QLabel("Sélectionnez le dossier contenant Wow.exe ou choisissez un emplacement pour installer le client.")
        path_info.setWordWrap(True)
        path_layout.addWidget(path_info)
        
        # Ajouter le groupe au layout principal
        layout.addWidget(path_group)
        
        # Section Realmlist
        realmlist_group = QGroupBox("Gestion des Realmlist")
        realmlist_layout = QVBoxLayout(realmlist_group)
        
        # Récupérer les entrées realmlist existantes
        realmlist_entries = self.config.get_realmlist_entries()
        active_index = self.config.get_active_realmlist_index()
        
        # Créer les widgets pour chaque entrée realmlist
        self.realmlist_widgets = []
        
        # Texte d'information
        realmlist_info = QLabel("Configurez jusqu'à 2 adresses de serveur différentes et basculez entre elles facilement.")
        realmlist_info.setWordWrap(True)
        realmlist_layout.addWidget(realmlist_info)
        
        # Créer un layout pour les options radio
        radio_layout = QHBoxLayout()
        self.realmlist_radio_group = QButtonGroup(settings_dialog)
        
        # Créer les widgets pour chaque entrée
        for i, entry in enumerate(realmlist_entries):
            # Layout pour cette entrée
            entry_layout = QVBoxLayout()
            entry_layout.setSpacing(5)
            
            # Titre avec bouton radio
            radio_button = QRadioButton(f"Realmlist {i+1}")
            radio_button.setChecked(i == active_index)
            self.realmlist_radio_group.addButton(radio_button, i)
            radio_layout.addWidget(radio_button)
            
            # Ajouter le layout radio au layout principal des realmlist
            if i == 0:
                realmlist_layout.addLayout(radio_layout)
            
            # Layout pour le nom
            name_layout = QHBoxLayout()
            name_label = QLabel("Nom:")
            name_layout.addWidget(name_label)
            
            name_edit = QLineEdit()
            name_edit.setText(entry.get("name", ""))
            name_layout.addWidget(name_edit)
            entry_layout.addLayout(name_layout)
            
            # Layout pour l'adresse
            address_layout = QHBoxLayout()
            address_label = QLabel("Adresse:")
            address_layout.addWidget(address_label)
            
            address_edit = QLineEdit()
            address_edit.setText(entry.get("address", ""))
            address_layout.addWidget(address_edit)
            entry_layout.addLayout(address_layout)
            
            # Ajouter un séparateur horizontal si ce n'est pas la dernière entrée
            if i < len(realmlist_entries) - 1:
                separator = QFrame()
                separator.setFrameShape(QFrame.HLine)
                separator.setFrameShadow(QFrame.Sunken)
                separator.setStyleSheet("background-color: #7a6a4d;")
                entry_layout.addWidget(separator)
            
            # Ajouter les widgets à la liste pour pouvoir les récupérer plus tard
            self.realmlist_widgets.append({
                "radio": radio_button,
                "name": name_edit,
                "address": address_edit
            })
            
            # Ajouter le layout de cette entrée au layout principal des realmlist
            realmlist_layout.addLayout(entry_layout)
        
        # Ajouter le groupe au layout principal
        layout.addWidget(realmlist_group)
        
        # Espace réservé pour d'autres paramètres futurs
        layout.addStretch()
        
        # Boutons OK/Annuler
        buttons_layout = QHBoxLayout()
        buttons_layout.addStretch()
        
        ok_button = QPushButton("OK")
        ok_button.clicked.connect(lambda: self._save_settings(settings_dialog))
        buttons_layout.addWidget(ok_button)
        
        cancel_button = QPushButton("Annuler")
        cancel_button.clicked.connect(settings_dialog.reject)
        buttons_layout.addWidget(cancel_button)
        
        layout.addLayout(buttons_layout)
        
        # Afficher la fenêtre
        settings_dialog.exec_()
    
    def _browse_from_settings(self):
        """Browse for WoW directory from settings dialog"""
        directory = QFileDialog.getExistingDirectory(
            self, "Sélectionner le dossier d'installation de WoW", 
            self.config.get("wow_path", os.path.expanduser("~"))
        )
        
        if directory:
            # Vérifier si c'est un dossier WoW valide
            wow_exe = os.path.join(directory, "Wow.exe")
            if os.path.exists(wow_exe):
                self.settings_path_edit.setText(directory)
                QMessageBox.information(self, "Client trouvé", 
                                      "Le client World of Warcraft 3.3.5 a été trouvé avec succès !")
            else:
                # Demander confirmation pour utiliser ce dossier quand même
                reply = QMessageBox.question(
                    self,
                    "Dossier sans client WoW",
                    "Ce dossier ne contient pas Wow.exe. Voulez-vous l'utiliser quand même pour installer le client ?",
                    QMessageBox.Yes | QMessageBox.No,
                    QMessageBox.No
                )
                
                if reply == QMessageBox.Yes:
                    self.settings_path_edit.setText(directory)
    
    def _save_settings(self, dialog):
        """Save settings and close dialog"""
        # Sauvegarder le chemin WoW
        wow_path = self.settings_path_edit.text()
        if wow_path:
            self.config.set("wow_path", wow_path)
        
        # Sauvegarder les entrées realmlist
        active_changed = False
        for i, widget_set in enumerate(self.realmlist_widgets):
            name = widget_set["name"].text().strip()
            address = widget_set["address"].text().strip()
            active = widget_set["radio"].isChecked()
            
            # Vérifier si l'entrée active a changé
            if active and i != self.config.get_active_realmlist_index():
                active_changed = True
            
            # Sauvegarder cette entrée
            self.config.set_realmlist_entry(i, name, address, active)
        
        # Sauvegarder la configuration
        self.config.save()
        
        # Mettre à jour l'interface
        self.check_wow_installed()
        
        # Mettre à jour l'affichage du serveur actuel
        if hasattr(self, "active_server_label") and hasattr(self, "active_server_address"):
            self._update_server_display()
        
        # Fermer la fenêtre
        dialog.accept()
    
    def open_addons(self):
        """Open addons directory in file explorer"""
        wow_path = self.config.get("wow_path")
        
        if not wow_path or not os.path.exists(os.path.join(wow_path, "Wow.exe")):
            # Client WoW non trouvé, demander à l'utilisateur de le localiser
            reply = QMessageBox.question(
                self,
                "Client WoW non trouvé",
                "Le client World of Warcraft n'a pas été trouvé. Voulez-vous le localiser maintenant ?",
                QMessageBox.Yes | QMessageBox.No,
                QMessageBox.Yes
            )
            
            if reply == QMessageBox.Yes:
                self.open_settings()
                # Vérifier à nouveau après les réglages
                wow_path = self.config.get("wow_path")
                if not wow_path or not os.path.exists(os.path.join(wow_path, "Wow.exe")):
                    return  # L'utilisateur n'a pas sélectionné de client valide
            else:
                return
        
        # Chemin vers le répertoire des addons pour WoW 3.3.5
        addons_path = os.path.join(wow_path, "Interface", "AddOns")
        
        # Créer le répertoire Interface s'il n'existe pas
        interface_path = os.path.join(wow_path, "Interface")
        if not os.path.exists(interface_path):
            try:
                os.makedirs(interface_path)
                self.status_label.setText("Répertoire Interface créé.")
            except Exception as e:
                QMessageBox.critical(self, "Erreur", f"Impossible de créer le répertoire Interface: {str(e)}")
                return
        
        # Créer le répertoire s'il n'existe pas
        if not os.path.exists(addons_path):
            try:
                os.makedirs(addons_path)
                self.status_label.setText("Répertoire AddOns créé.")
            except Exception as e:
                QMessageBox.critical(self, "Erreur", f"Impossible de créer le répertoire AddOns: {str(e)}")
                return
        
        # Afficher le chemin pour débogage
        self.status_label.setText(f"Chemin AddOns: {addons_path}")
        
        # Convertir le chemin en chemin absolu complet
        addons_path_abs = os.path.abspath(addons_path)
        
        # Vérifier que le chemin existe
        if not os.path.exists(addons_path_abs):
            QMessageBox.warning(self, "Attention", f"Le répertoire {addons_path_abs} n'existe pas encore. Il sera créé.")
        
        # Ouvrir le répertoire dans l'explorateur de fichiers
        try:
            # Utiliser la commande appropriée pour Windows avec le chemin absolu
            # Utiliser os.startfile qui est plus fiable pour ouvrir des dossiers sur Windows
            os.startfile(addons_path_abs)
            self.status_label.setText(f"Répertoire AddOns ouvert: {addons_path_abs}")
        except Exception as e:
            QMessageBox.critical(self, "Erreur", f"Impossible d'ouvrir le répertoire AddOns: {str(e)}\nChemin: {addons_path_abs}")
    
    def open_forum(self):
        """Open forum in web browser"""
        try:
            webbrowser.open("https://sylvania-project.org/forum")
        except Exception as e:
            QMessageBox.warning(self, "Erreur", f"Impossible d'ouvrir le forum: {str(e)}")
    
    def update_download_progress(self, current, total):
        """Update download progress bar"""
        if total > 0:
            # Convert to MB for display
            current_mb = current / (1024 * 1024)
            total_mb = total / (1024 * 1024)
            
            # Update progress bar
            self.download_progress.setRange(0, int(total_mb))
            self.download_progress.setValue(int(current_mb))
            self.download_progress.setFormat(f"{current_mb:.1f} MB / {total_mb:.1f} MB")
    
    # Les anciennes fonctions liées au téléchargement ont été supprimées
    # car elles ont été remplacées par la nouvelle implémentation qui utilise
    # la classe DownloadDialog du module download_client.py
