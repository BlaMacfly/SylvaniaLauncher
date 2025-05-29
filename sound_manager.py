#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""
Module de gestion des sons pour le Launcher Sylvania
"""

import os
import logging
from pathlib import Path
from PySide6.QtCore import QUrl
from PySide6.QtMultimedia import QMediaPlayer, QAudioOutput

class SoundManager:
    """Classe pour gérer les sons du launcher"""
    
    def __init__(self):
        """Initialiser le gestionnaire de sons"""
        # Trouver le répertoire des sons en mode développement ou compilé
        self.sounds_dir = self._get_sounds_dir()
        
        # Vérifier si le répertoire des sons existe
        if not os.path.exists(self.sounds_dir):
            logging.warning(f"Répertoire des sons non trouvé: {self.sounds_dir}")
        
        # Initialiser le lecteur audio
        self.player = QMediaPlayer()
        self.audio_output = QAudioOutput()
        self.player.setAudioOutput(self.audio_output)
        
        # Volume par défaut (50%)
        self.audio_output.setVolume(0.5)
        
        # Dictionnaire des sons
        self.sounds = {
            "play": os.path.join(self.sounds_dir, "launch.mp3"),  # Son pour le bouton Jouer
            "button": os.path.join(self.sounds_dir, "toggle.mp3")   # Son pour tous les autres boutons
        }
    
    def _get_sounds_dir(self):
        """Obtient le chemin des sons, fonctionne en développement et après compilation"""
        import sys
        
        # Liste des chemins possibles pour le dossier Sounds
        possible_paths = [
            # Chemin en mode développement
            os.path.join(os.path.dirname(os.path.abspath(__file__)), "Sounds"),
            
            # Chemin en mode compilé (PyInstaller)
            os.path.join(getattr(sys, '_MEIPASS', os.path.dirname(os.path.abspath(__file__))), "Sounds"),
            
            # Chemin alternatif (dans le dossier Asset)
            os.path.join(os.path.dirname(os.path.abspath(__file__)), "Asset", "Sounds"),
            
            # Chemin alternatif en mode compilé
            os.path.join(getattr(sys, '_MEIPASS', os.path.dirname(os.path.abspath(__file__))), "Asset", "Sounds"),
        ]
        
        # Vérifier chaque chemin possible
        for path in possible_paths:
            if os.path.exists(path):
                print(f"Dossier de sons trouvé: {path}")
                return path
        
        # Si aucun chemin n'est trouvé, retourner le chemin par défaut et logger l'erreur
        logging.warning(f"ATTENTION: Impossible de trouver le dossier Sounds. Chemins essayés: {possible_paths}")
        return os.path.join(os.path.dirname(os.path.abspath(__file__)), "Sounds")
    
    def play_sound(self, sound_name):
        """Jouer un son par son nom"""
        if sound_name not in self.sounds:
            logging.warning(f"Son '{sound_name}' non trouvé")
            return
        
        sound_path = self.sounds[sound_name]
        
        # Vérifier si le fichier son existe
        if not os.path.exists(sound_path):
            logging.warning(f"Fichier son '{sound_path}' non trouvé")
            return
        
        # Jouer le son
        try:
            self.player.setSource(QUrl.fromLocalFile(sound_path))
            self.player.play()
        except Exception as e:
            logging.error(f"Erreur lors de la lecture du son: {str(e)}")
    
    def set_volume(self, volume):
        """Définir le volume (0.0 à 1.0)"""
        self.audio_output.setVolume(max(0.0, min(1.0, volume)))
    
    def get_volume(self):
        """Obtenir le volume actuel"""
        return self.audio_output.volume()
