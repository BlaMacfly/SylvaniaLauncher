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
from PySide6.QtCore import Qt, QTimer, Signal, Slot

class DownloadDialog(QDialog):
    """Fenêtre de téléchargement du client WoW"""
    
    # Signaux pour la communication entre threads
    update_status = Signal(str)
    update_progress = Signal(int)
    update_speed = Signal(str)
    update_size = Signal(str)
    
    def __init__(self, parent=None, destination=None):
        super().__init__(parent)
        self.setWindowTitle("Téléchargement du client WoW")
        self.setMinimumWidth(500)
        self.setMinimumHeight(200)
        self.setWindowFlags(Qt.Dialog | Qt.WindowTitleHint | Qt.WindowCloseButtonHint)
        self.setModal(True)
        
        # URL du client WoW
        self.client_url = "https://sylvania-servergame.com/launcher-download.php"
        
        # Variables pour le téléchargement
        self.download_thread = None
        self.should_cancel = False
        self.destination = destination
        
        # Connecter les signaux aux slots
        self.update_status.connect(self.set_status_text)
        self.update_progress.connect(self.set_progress_value)
        self.update_speed.connect(self.set_speed_text)
        self.update_size.connect(self.set_size_text)
        
        # Créer l'interface
        self.setup_ui()
        
        # Démarrer automatiquement le téléchargement après un court délai
        if self.destination:
            # Si un emplacement est déjà fourni, démarrer directement le téléchargement
            QTimer.singleShot(100, lambda: self.start_download(self.destination))
        else:
            # Sinon, demander à l'utilisateur de sélectionner un emplacement
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
        
        # Démarrer directement le téléchargement sans afficher de boîte de dialogue supplémentaire
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
        # Utiliser les signaux pour communiquer avec le thread principal
        if status_text is not None:
            self.update_status.emit(status_text)
        
        if progress_value is not None:
            self.update_progress.emit(progress_value)
        
        if speed_text is not None:
            self.update_speed.emit(speed_text)
        
        if size_text is not None:
            self.update_size.emit(size_text)
    
    @Slot(str)
    def set_status_text(self, text):
        """Slot pour mettre à jour le texte de statut"""
        self.status_label.setText(text)
        QApplication.processEvents()
    
    @Slot(int)
    def set_progress_value(self, value):
        """Slot pour mettre à jour la valeur de la barre de progression"""
        self.progress_bar.setValue(value)
        QApplication.processEvents()
    
    @Slot(str)
    def set_speed_text(self, text):
        """Slot pour mettre à jour le texte de vitesse"""
        self.speed_label.setText(text)
        QApplication.processEvents()
    
    @Slot(str)
    def set_size_text(self, text):
        """Slot pour mettre à jour le texte de taille"""
        self.size_label.setText(text)
        QApplication.processEvents()
    
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
        """Télécharger et extraire le client WoW avec gestion des instabilités réseau"""
        try:
            # Créer le dossier de destination s'il n'existe pas
            if not os.path.exists(destination):
                os.makedirs(destination)
            
            # Nom du fichier ZIP temporaire
            zip_path = os.path.join(destination, "wow_client.zip")
            
            # Mettre à jour l'interface utilisateur
            self._update_ui_from_thread("Connexion au serveur...", 0)
            
            # Variables pour la gestion des reprises de téléchargement
            max_retries = 5  # Nombre maximal de tentatives en cas d'erreur
            retry_delay = 3  # Délai en secondes entre les tentatives
            retry_count = 0   # Compteur de tentatives
            downloaded = 0     # Octets déjà téléchargés
            total_size = 0     # Taille totale du fichier
            total_size_mb = 0  # Taille totale en MB
            
            # Vérifier si un téléchargement partiel existe déjà
            if os.path.exists(zip_path):
                # Obtenir la taille du fichier existant
                downloaded = os.path.getsize(zip_path)
                if downloaded > 0:
                    self._update_ui_from_thread(
                        f"Reprise du téléchargement à partir de {downloaded / (1024 * 1024):.2f} MB...", 
                        0
                    )
            
            # Boucle de tentatives pour gérer les interruptions réseau
            while retry_count < max_retries:
                try:
                    # Configurer les headers pour reprendre le téléchargement si nécessaire
                    headers = {}
                    if downloaded > 0:
                        headers['Range'] = f'bytes={downloaded}-'
                    
                    # Désactiver la vérification SSL pour le débogage et augmenter le timeout
                    response = requests.get(url, stream=True, verify=False, headers=headers, timeout=30)
                    
                    # Vérifier si la réponse est valide
                    if response.status_code not in [200, 206]:  # 200 OK ou 206 Partial Content
                        raise Exception(f"Erreur de téléchargement: {response.status_code} - {response.reason}")
                    
                    # Obtenir la taille totale du fichier
                    if 'content-length' in response.headers:
                        content_length = int(response.headers.get('content-length', 0))
                        
                        # Si c'est une reprise de téléchargement (206), la taille totale est dans content-range
                        if response.status_code == 206 and 'content-range' in response.headers:
                            # Format: bytes X-Y/Z où Z est la taille totale
                            content_range = response.headers.get('content-range')
                            total_size = int(content_range.split('/')[1])
                        else:
                            # Sinon, la taille totale est content-length + déjà téléchargé
                            total_size = content_length + (downloaded if response.status_code == 206 else 0)
                    
                    total_size_mb = total_size / (1024 * 1024)  # Convertir en MB
                    
                    # Mettre à jour l'interface utilisateur
                    self._update_ui_from_thread(
                        f"Téléchargement du client WoW ({total_size_mb:.2f} MB)...", 
                        int(100 * downloaded / total_size) if total_size > 0 and downloaded > 0 else 0,
                        "Vitesse: Calcul en cours...",
                        f"{downloaded / (1024 * 1024):.2f} MB / {total_size_mb:.2f} MB"
                    )
                    
                    # Mode d'ouverture du fichier (append binaire si reprise, write binaire sinon)
                    mode = 'ab' if downloaded > 0 and response.status_code == 206 else 'wb'
                    
                    # Télécharger le fichier par morceaux avec mise à jour de la progression
                    start_time = time.time()
                    last_update_time = start_time
                    last_downloaded = downloaded
                    block_size = 1024 * 1024  # 1 MB
                    
                    with open(zip_path, mode) as f:
                        for chunk in response.iter_content(chunk_size=block_size):
                            if self.should_cancel:
                                raise Exception("Téléchargement annulé par l'utilisateur")
                            
                            if chunk:  # Filtrer les keep-alive
                                f.write(chunk)
                                downloaded += len(chunk)
                                downloaded_mb = downloaded / (1024 * 1024)
                                
                                # Calculer la vitesse de téléchargement (mise à jour chaque seconde)
                                current_time = time.time()
                                if current_time - last_update_time >= 1:
                                    elapsed = current_time - last_update_time
                                    speed = (downloaded - last_downloaded) / elapsed / 1024  # KB/s
                                    
                                    if speed > 1024:
                                        speed_text = f"Vitesse: {speed/1024:.2f} MB/s"
                                    else:
                                        speed_text = f"Vitesse: {speed:.2f} KB/s"
                                    
                                    # Calculer le pourcentage de progression
                                    progress = int(100 * downloaded / total_size) if total_size > 0 else 0
                                    
                                    # Mettre à jour l'interface utilisateur
                                    self._update_ui_from_thread(
                                        f"Téléchargement en cours...", 
                                        progress,
                                        speed_text,
                                        f"{downloaded_mb:.2f} MB / {total_size_mb:.2f} MB"
                                    )
                                    
                                    # Mettre à jour les variables pour le prochain calcul
                                    last_update_time = current_time
                                    last_downloaded = downloaded
                    
                    # Si on arrive ici, le téléchargement est terminé avec succès
                    break
                    
                except (requests.exceptions.RequestException, requests.exceptions.ConnectionError, 
                        requests.exceptions.Timeout, requests.exceptions.ChunkedEncodingError) as e:
                    # Incrémenter le compteur de tentatives
                    retry_count += 1
                    
                    if retry_count >= max_retries:
                        # Si on a atteint le nombre maximal de tentatives, lever l'exception
                        raise Exception(f"Erreur de connexion après {max_retries} tentatives: {str(e)}")
                    
                    # Informer l'utilisateur de la tentative de reconnexion
                    self._update_ui_from_thread(
                        f"Erreur réseau: {str(e)}. Tentative de reconnexion ({retry_count}/{max_retries}) dans {retry_delay} secondes...", 
                        int(100 * downloaded / total_size) if total_size > 0 else 0
                    )
                    
                    # Attendre avant de réessayer
                    for i in range(retry_delay):
                        if self.should_cancel:
                            raise Exception("Téléchargement annulé par l'utilisateur")
                        time.sleep(1)
            
            # Vérifier si l'annulation a été demandée
            if self.should_cancel:
                raise Exception("Téléchargement annulé par l'utilisateur")
            
            # Vérifier que le fichier a bien été téléchargé complètement
            if total_size > 0 and downloaded < total_size:
                raise Exception(f"Téléchargement incomplet: {downloaded} octets sur {total_size}")
            
            # Mettre à jour l'interface utilisateur
            self._update_ui_from_thread(
                "Téléchargement terminé, vérification du fichier...", 
                100,
                "Vérification...",
                f"{total_size_mb:.2f} MB / {total_size_mb:.2f} MB"
            )
            
            # Vérifier que le fichier ZIP est valide avant de l'extraire
            try:
                with zipfile.ZipFile(zip_path, 'r') as zip_check:
                    # Vérifier l'intégrité du ZIP (cela lira l'en-tête)
                    if zip_check.testzip() is not None:
                        raise Exception("Le fichier ZIP téléchargé est corrompu")
                    
                    # Obtenir la liste des fichiers
                    file_list = zip_check.namelist()
                    total_files = len(file_list)
            except zipfile.BadZipFile:
                # Si le fichier ZIP est corrompu, le supprimer et lever une exception
                if os.path.exists(zip_path):
                    os.remove(zip_path)
                raise Exception("Le fichier ZIP téléchargé est corrompu et a été supprimé. Veuillez réessayer.")
            
            # Mettre à jour l'interface utilisateur
            self._update_ui_from_thread(
                "Extraction en cours...", 
                0,
                "Extraction...",
                f"0 / {total_files} fichiers"
            )
            
            # Extraire le fichier ZIP
            extracted = 0
            with zipfile.ZipFile(zip_path, 'r') as zip_ref:
                # Extraire chaque fichier avec progression
                for file in file_list:
                    if self.should_cancel:
                        raise Exception("Extraction annulée par l'utilisateur")
                    
                    try:
                        zip_ref.extract(file, destination)
                        extracted += 1
                    except Exception as e:
                        # Continuer l'extraction même si un fichier échoue
                        self._update_ui_from_thread(
                            f"Erreur lors de l'extraction de {file}: {str(e)}", 
                            int(100 * extracted / total_files)
                        )
                    
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
