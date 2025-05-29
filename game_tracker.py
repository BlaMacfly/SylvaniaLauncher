#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""
Module de suivi du temps de jeu pour le Launcher Sylvania
"""

import os
import json
import time
import logging
from pathlib import Path

class GameTimeTracker:
    """Classe pour suivre le temps de jeu et le nombre de lancements"""
    
    def __init__(self):
        """Initialiser le tracker de temps de jeu"""
        # Vérifier si nous sommes en mode portable
        portable_file = os.path.join(os.path.dirname(os.path.abspath(__file__)), "portable.txt")
        is_portable = os.path.exists(portable_file)
        
        if is_portable:
            # Mode portable - stocker les statistiques dans le répertoire de l'application
            self.config_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "data")
            logging.info(f"Mode portable détecté. Stockage des statistiques dans: {self.config_dir}")
        else:
            # Mode normal - stocker les statistiques dans AppData
            self.config_dir = os.path.join(os.path.expanduser("~"), "AppData", "Local", "SylvaniaLauncher")
            logging.info(f"Mode normal. Stockage des statistiques dans: {self.config_dir}")
            
        self.stats_file = os.path.join(self.config_dir, "game_stats.json")
        self.stats = self._load_stats()
        
    def _load_stats(self):
        """Charger les statistiques depuis le fichier JSON ou créer des statistiques par défaut"""
        try:
            # Créer le répertoire de configuration s'il n'existe pas
            if not os.path.exists(self.config_dir):
                os.makedirs(self.config_dir)
            
            # Charger les statistiques existantes si le fichier existe
            if os.path.exists(self.stats_file):
                with open(self.stats_file, 'r', encoding='utf-8') as f:
                    return json.load(f)
            
            # Créer des statistiques par défaut si le fichier n'existe pas
            default_stats = {
                "time": 0,        # Temps total de jeu en secondes
                "launches": 0     # Nombre total de lancements
            }
            
            # Sauvegarder les statistiques par défaut
            self._save_stats(default_stats)
            return default_stats
            
        except Exception as e:
            logging.error(f"Erreur lors du chargement des statistiques: {str(e)}")
            return {"time": 0, "launches": 0}
    
    def _save_stats(self, stats=None):
        """Sauvegarder les statistiques dans le fichier JSON"""
        try:
            if stats is None:
                stats = self.stats
            
            with open(self.stats_file, 'w', encoding='utf-8') as f:
                json.dump(stats, f, indent=2)
                
        except Exception as e:
            logging.error(f"Erreur lors de la sauvegarde des statistiques: {str(e)}")
    
    def get_time(self):
        """Obtenir le temps total de jeu en secondes"""
        return self.stats.get("time", 0)
    
    def get_formatted_time(self):
        """Obtenir le temps total de jeu formaté (ex: 1h 25min)"""
        total_seconds = self.get_time()
        
        # Calculer les heures, minutes et secondes
        hours = total_seconds // 3600
        minutes = (total_seconds % 3600) // 60
        seconds = total_seconds % 60
        
        # Formater le temps
        if hours > 0:
            return f"{int(hours)}h {int(minutes)}min"
        elif minutes > 0:
            return f"{int(minutes)}min {int(seconds)}s"
        else:
            return f"{int(seconds)}s"
    
    def get_launches(self):
        """Obtenir le nombre total de lancements"""
        return self.stats.get("launches", 0)
    
    def increment_launches(self):
        """Incrémenter le compteur de lancements"""
        self.stats["launches"] = self.stats.get("launches", 0) + 1
        self._save_stats()
        return self.stats["launches"]
    
    def add_time(self, seconds):
        """Ajouter du temps de jeu (en secondes)"""
        self.stats["time"] = self.stats.get("time", 0) + seconds
        self._save_stats()
        return self.stats["time"]
    
    def get_last_played(self):
        """Obtenir la date de la dernière session de jeu"""
        return self.stats.get("last_played", "Jamais")
    
    def update_last_played(self):
        """Mettre à jour la date de la dernière session de jeu"""
        # Obtenir la date et l'heure actuelles au format français
        current_time = time.strftime("%d/%m/%Y à %H:%M", time.localtime())
        self.stats["last_played"] = current_time
        self._save_stats()
        return current_time
