#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""
Script de téléchargement du client WoW pour le Launcher Sylvania
"""

import os
import sys
import time
import requests
import zipfile
import threading
from PySide6.QtWidgets import (
    QApplication, QDialog, QVBoxLayout, QHBoxLayout, 
    QLabel, QProgressBar, QPushButton, QFileDialog, QMessageBox
)
from PySide6.QtCore import Qt, QTimer

class DownloadDialog(QDialog):
    """Fenêtre de téléchargement du client WoW"""
    
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Téléchargement du client WoW")
        self.setMinimumWidth(500)
        self.setMinimumHeight(200)
        self.setWindowFlags(Qt.Dialog | Qt.WindowTitleHint | Qt.WindowCloseButtonHint)
        self.setModal(True)
        
        # URL du client WoW
        self.client_url = "https://dl.way-of-elendil.fr/WINDOWS_World_of_Warcraft_335a.zip"
        
        # Variables pour le téléchargement
        self.download_thread = None
        self.should_cancel = False
        self.destination = None
        
        # Créer l'interface
        self.setup_ui()
        
        # Démarrer automatiquement le téléchargement après un court délai
        QTimer.singleShot(100, self.prompt_for_directory)
    
    def setup_ui(self):
        """Configurer l'interface utilisateur"""
        layout = QVBoxLayout(self)
        
        # Étiquette de statut
        self.status_label = QLabel("Prêt à télécharger")
        self.status_label.setAlignment(Qt.AlignCenter)
        layout.addWidget(self.status_label)
        
        # Barre de progression avec style personnalisé
        self.progress_bar = QProgressBar()
        self.progress_bar.setRange(0, 100)
        self.progress_bar.setValue(0)
        
        # Appliquer un style CSS pour avoir une barre bleue
        self.progress_bar.setStyleSheet("""
            QProgressBar {
                border: 1px solid gray;
                border-radius: 3px;
                background: #222222;
                text-align: center;
                height: 20px;
            }
            QProgressBar::chunk {
                background-color: #0078d7;
                width: 1px;
            }
        """)
        
        layout.addWidget(self.progress_bar)
        
        # Informations supplémentaires
        info_layout = QHBoxLayout()
        
        # Vitesse de téléchargement
        self.speed_label = QLabel("Vitesse: 0 KB/s")
        info_layout.addWidget(self.speed_label)
        
        # Taille téléchargée
        self.size_label = QLabel("0 MB / 0 MB")
        info_layout.addWidget(self.size_label)
        
        layout.addLayout(info_layout)
        
        # Bouton d'annulation
        self.cancel_button = QPushButton("Annuler")
        self.cancel_button.clicked.connect(self.cancel_download)
        
        button_layout = QHBoxLayout()
        button_layout.addStretch()
        button_layout.addWidget(self.cancel_button)
        
        layout.addLayout(button_layout)
    
    def prompt_for_directory(self):
        """Demander à l'utilisateur de choisir un emplacement d'installation et démarrer le téléchargement"""
        # Demander à l'utilisateur de choisir un emplacement pour l'installation
        directory = QFileDialog.getExistingDirectory(
            self, "Sélectionner un emplacement pour installer WoW", 
            os.path.expanduser("~")
        )
        
        if not directory:
            self.reject()  # L'utilisateur a annulé, fermer la fenêtre
            return
        
        # Enregistrer le répertoire choisi
        self.destination = directory
        
        # Afficher un message de confirmation
        msg_box = QMessageBox(self)
        msg_box.setWindowTitle("Téléchargement")
        msg_box.setText(f"Le client WoW sera installé dans: {directory}\nLe téléchargement va commencer.")
        msg_box.setIcon(QMessageBox.Information)
        msg_box.setStandardButtons(QMessageBox.Ok)
        msg_box.exec_()
        
        # Démarrer le téléchargement
        self.start_download(directory)
    
    def start_download(self, directory=None):
        """Démarrer le téléchargement du client WoW"""
        # Utiliser le répertoire fourni ou celui choisi précédemment
        if directory is None:
            if self.destination is None:
                self.prompt_for_directory()
                return
            directory = self.destination
        
        # Activer le bouton d'annulation
        self.cancel_button.setEnabled(True)
        
        # Réinitialiser le drapeau d'annulation
        self.should_cancel = False
        
        # Mettre à jour l'interface avant de lancer le thread
        self.status_label.setText("Préparation du téléchargement...")
        self.progress_bar.setValue(0)
        
        # Lancer le téléchargement dans un thread séparé
        self.download_thread = threading.Thread(
            target=self._download_and_extract, 
            args=(self.client_url, directory)
        )
        self.download_thread.daemon = True
        self.download_thread.start()
    
    def _update_ui_from_thread(self, status_text, progress_value, speed_text=None, size_text=None):
        """Mettre à jour l'interface utilisateur depuis un thread"""
        QTimer.singleShot(0, lambda: self._update_ui(status_text, progress_value, speed_text, size_text))
    
    def _update_ui(self, status_text=None, progress_value=None, speed_text=None, size_text=None):
        """Mettre à jour l'interface utilisateur"""
        if status_text is not None:
            self.status_label.setText(status_text)
        
        if progress_value is not None:
            self.progress_bar.setValue(progress_value)
        
        if speed_text is not None:
            self.speed_label.setText(speed_text)
        
        if size_text is not None:
            self.size_label.setText(size_text)
    
    def cancel_download(self):
        """Annuler le téléchargement en cours"""
        self.should_cancel = True
        self.status_label.setText("Annulation du téléchargement...")
    
    def _download_and_extract(self, url, destination):
        """Télécharger et extraire le client WoW"""
        try:
            # Créer le dossier de destination s'il n'existe pas
            if not os.path.exists(destination):
                os.makedirs(destination)
            
            # Nom du fichier ZIP temporaire
            zip_path = os.path.join(destination, "wow_client.zip")
            
            # Mettre à jour l'interface utilisateur
            self._update_ui_from_thread("Connexion au serveur...", 0)
            
            # Télécharger le fichier
            try:
                # Désactiver la vérification SSL pour le débogage
                response = requests.get(url, stream=True, verify=False)
                
                # Vérifier si la réponse est valide
                if response.status_code != 200:
                    raise Exception(f"Erreur de téléchargement: {response.status_code} - {response.reason}")
                
                # Obtenir la taille totale du fichier
                total_size = int(response.headers.get('content-length', 0))
                total_size_mb = total_size / (1024 * 1024)  # Convertir en MB
                
                # Mettre à jour l'interface utilisateur
                self._update_ui_from_thread(
                    f"Téléchargement du client WoW ({total_size_mb:.2f} MB)...", 
                    0,
                    "Vitesse: 0 KB/s",
                    f"0 MB / {total_size_mb:.2f} MB"
                )
                
                # Télécharger le fichier par morceaux avec mise à jour de la progression
                downloaded = 0
                start_time = time.time()
                block_size = 1024 * 1024  # 1 MB
                
                with open(zip_path, 'wb') as f:
                    for chunk in response.iter_content(chunk_size=block_size):
                        if self.should_cancel:
                            raise Exception("Téléchargement annulé par l'utilisateur")
                        
                        if chunk:  # Filtrer les keep-alive
                            f.write(chunk)
                            downloaded += len(chunk)
                            downloaded_mb = downloaded / (1024 * 1024)
                            
                            # Calculer la vitesse de téléchargement
                            elapsed = time.time() - start_time
                            if elapsed > 0:
                                speed = downloaded / elapsed / 1024  # KB/s
                                if speed > 1024:
                                    speed_text = f"Vitesse: {speed/1024:.2f} MB/s"
                                else:
                                    speed_text = f"Vitesse: {speed:.2f} KB/s"
                            else:
                                speed_text = "Vitesse: Calcul en cours..."
                            
                            # Calculer le pourcentage de progression
                            progress = int(100 * downloaded / total_size) if total_size > 0 else 0
                            
                            # Mettre à jour l'interface utilisateur
                            self._update_ui_from_thread(
                                f"Téléchargement en cours...", 
                                progress,
                                speed_text,
                                f"{downloaded_mb:.2f} MB / {total_size_mb:.2f} MB"
                            )
                
                # Vérifier si l'annulation a été demandée
                if self.should_cancel:
                    raise Exception("Téléchargement annulé par l'utilisateur")
                
                # Mettre à jour l'interface utilisateur
                self._update_ui_from_thread(
                    "Téléchargement terminé, extraction en cours...", 
                    100,
                    "Extraction...",
                    f"{total_size_mb:.2f} MB / {total_size_mb:.2f} MB"
                )
                
                # Extraire le fichier ZIP
                with zipfile.ZipFile(zip_path, 'r') as zip_ref:
                    # Compter le nombre total de fichiers
                    file_list = zip_ref.namelist()
                    total_files = len(file_list)
                    extracted = 0
                    
                    # Extraire chaque fichier avec progression
                    for file in file_list:
                        if self.should_cancel:
                            raise Exception("Extraction annulée par l'utilisateur")
                        
                        zip_ref.extract(file, destination)
                        extracted += 1
                        
                        # Mettre à jour la progression toutes les 10 fichiers pour éviter de surcharger l'interface
                        if extracted % 10 == 0 or extracted == total_files:
                            progress = int(100 * extracted / total_files)
                            self._update_ui_from_thread(
                                f"Extraction: {extracted} / {total_files} fichiers", 
                                progress,
                                "Extraction en cours...",
                                f"{extracted} / {total_files} fichiers"
                            )
                
                # Supprimer le fichier ZIP temporaire
                if os.path.exists(zip_path):
                    os.remove(zip_path)
                
                # Mettre à jour l'interface utilisateur
                self._update_ui_from_thread(
                    "Installation terminée avec succès!", 
                    100,
                    "Terminé",
                    "Installation complète"
                )
                
                # Fermer la fenêtre après un court délai
                QTimer.singleShot(2000, self.accept)
                
            except requests.exceptions.RequestException as e:
                raise Exception(f"Erreur de connexion: {str(e)}")
            
        except Exception as e:
            error_msg = f"Erreur: {str(e)}"
            self._update_ui_from_thread(error_msg, 0, "Erreur", "")
            
            # Afficher un message d'erreur
            QTimer.singleShot(0, lambda: QMessageBox.critical(self, "Erreur", error_msg))
            
            # Fermer la fenêtre après un court délai
            QTimer.singleShot(3000, self.reject)

# Pour tester le script directement
if __name__ == "__main__":
    app = QApplication(sys.argv)
    dialog = DownloadDialog()
    dialog.exec_()
    sys.exit(app.exec_())
