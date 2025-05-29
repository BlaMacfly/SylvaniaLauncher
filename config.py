#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""
Configuration module for Sylvania Launcher
Handles reading/writing configuration settings
"""

import os
import json
import logging
import sys
from pathlib import Path

# Configuration du système de journalisation
# Utiliser le dossier AppData pour les logs plutôt que le dossier d'installation
app_data_dir = os.path.join(os.path.expanduser("~"), "AppData", "Local", "SylvaniaLauncher")
log_dir = os.path.join(app_data_dir, "logs")
if not os.path.exists(log_dir):
    try:
        os.makedirs(log_dir)
    except Exception:
        # Fallback si impossible de créer le dossier (utiliser le dossier temporaire)
        log_dir = os.path.join(os.environ.get("TEMP", os.path.dirname(os.path.abspath(__file__))), "SylvaniaLauncher_logs")
        if not os.path.exists(log_dir):
            os.makedirs(log_dir)

log_file = os.path.join(log_dir, "sylvania_launcher.log")

# Configurer le logger pour écrire dans un fichier et afficher sur la console
logger = logging.getLogger("SylvaniaLauncher")
logger.setLevel(logging.DEBUG)

# Handler pour le fichier
file_handler = logging.FileHandler(log_file, encoding='utf-8')
file_handler.setLevel(logging.DEBUG)

# Handler pour la console
console_handler = logging.StreamHandler(sys.stdout)
console_handler.setLevel(logging.INFO)

# Format des messages de log
log_format = logging.Formatter('%(asctime)s - %(name)s - %(levelname)s - %(message)s')
file_handler.setFormatter(log_format)
console_handler.setFormatter(log_format)

# Ajouter les handlers au logger
logger.addHandler(file_handler)
logger.addHandler(console_handler)

# Default configuration
DEFAULT_CONFIG = {
    "wow_path": "",
    "realmlist": "set realmlist logon.sylvania.servegame.com",
    "realmlist_entries": [
        {"name": "Sylvania", "address": "set realmlist logon.sylvania.servegame.com", "active": True},
        {"name": "", "address": "", "active": False}
    ],
    "active_realmlist_index": 0,
    "torrent_file": os.path.join(os.path.dirname(os.path.abspath(__file__)), "WoW_3.3.5_Sylvania.torrent"),
    "client_zip_url": "https://dl.way-of-elendil.fr/WINDOWS_World_of_Warcraft_335a.zip",  # URL du client ZIP
    "download_method": "direct",  # Utiliser le téléchargement direct
    "max_upload_rate": 100,  # KB/s
    "max_download_rate": 0,  # 0 means unlimited
    "first_run": True,
    "auto_update": True,
    "language": "fr"
}

class Config:
    """Configuration manager for the launcher"""
    
    def __init__(self):
        """Initialize the configuration manager"""
        self.config_dir = os.path.join(os.path.expanduser("~"), "AppData", "Local", "SylvaniaLauncher")
        self.config_file = os.path.join(self.config_dir, "config.json")
        self.settings = self._load_config()
    
    def _load_config(self):
        """Load configuration from file or create default if not exists"""
        try:
            # Create config directory if it doesn't exist
            if not os.path.exists(self.config_dir):
                os.makedirs(self.config_dir)
            
            # Load existing config if it exists
            if os.path.exists(self.config_file):
                with open(self.config_file, 'r', encoding='utf-8') as f:
                    config = json.load(f)
                
                # Ensure all default keys exist
                for key, value in DEFAULT_CONFIG.items():
                    if key not in config:
                        config[key] = value
                
                # Migration: Si l'ancienne configuration n'a pas encore les entrées realmlist multiples
                if "realmlist_entries" not in config:
                    # Créer les entrées realmlist à partir de l'entrée unique existante
                    config["realmlist_entries"] = [
                        {"name": "Sylvania", "address": config.get("realmlist", "set realmlist logon.sylvania.servegame.com"), "active": True},
                        {"name": "", "address": "", "active": False}
                    ]
                    config["active_realmlist_index"] = 0
                
                return config
            
            # Create default config if file doesn't exist
            with open(self.config_file, 'w', encoding='utf-8') as f:
                json.dump(DEFAULT_CONFIG, f, indent=4)
            
            return DEFAULT_CONFIG.copy()
        
        except Exception as e:
            logging.error(f"Error loading configuration: {str(e)}")
            return DEFAULT_CONFIG.copy()
    
    def save(self):
        """Save current configuration to file"""
        try:
            with open(self.config_file, 'w', encoding='utf-8') as f:
                json.dump(self.settings, f, indent=4)
            return True
        except Exception as e:
            logging.error(f"Error saving configuration: {str(e)}")
            return False
    
    def get(self, key, default=None):
        """Get a configuration value"""
        return self.settings.get(key, default)
    
    def set(self, key, value):
        """Set a configuration value and save"""
        self.settings[key] = value
        return self.save()
    
    def update_realmlist(self, realmlist_text):
        """Update all realmlist.wtf files found in the WoW directory"""
        try:
            wow_path = self.get("wow_path")
            if not wow_path or not os.path.exists(wow_path):
                print(f"Erreur: Chemin WoW non trouvé ou invalide: {wow_path}")
                return False
            
            print(f"Mise à jour du realmlist vers: {realmlist_text}")
            print(f"Dossier WoW: {wow_path}")
            
            # Liste des emplacements standards pour realmlist.wtf
            standard_paths = [
                os.path.join(wow_path, "Data", "enUS", "realmlist.wtf"),
                os.path.join(wow_path, "data", "enUS", "realmlist.wtf"),
                os.path.join(wow_path, "Data", "enGB", "realmlist.wtf"),
                os.path.join(wow_path, "data", "enGB", "realmlist.wtf"),
                os.path.join(wow_path, "Data", "frFR", "realmlist.wtf"),
                os.path.join(wow_path, "data", "frFR", "realmlist.wtf"),
                os.path.join(wow_path, "realmlist.wtf"),
            ]
            
            # Rechercher tous les fichiers realmlist.wtf dans le dossier WoW
            found_files = []
            for root, dirs, files in os.walk(wow_path):
                for file in files:
                    if file.lower() == "realmlist.wtf":
                        found_path = os.path.join(root, file)
                        found_files.append(found_path)
                        print(f"Fichier realmlist.wtf trouvé: {found_path}")
            
            # Ajouter les chemins standards s'ils ne sont pas déjà dans la liste
            for path in standard_paths:
                if path not in found_files and os.path.exists(os.path.dirname(path)):
                    found_files.append(path)
                    print(f"Ajout du chemin standard: {path}")
            
            # Si aucun fichier n'a été trouvé, créer le fichier au chemin standard
            if not found_files:
                default_path = standard_paths[0]  # Premier chemin comme défaut
                os.makedirs(os.path.dirname(default_path), exist_ok=True)
                found_files.append(default_path)
                print(f"Aucun fichier realmlist.wtf trouvé, création à: {default_path}")
            
            # Mettre à jour tous les fichiers trouvés
            updated_files = []
            for file_path in found_files:
                try:
                    # Créer le répertoire parent si nécessaire
                    os.makedirs(os.path.dirname(file_path), exist_ok=True)
                    
                    # Lire le contenu actuel si le fichier existe
                    current_content = ""
                    if os.path.exists(file_path):
                        try:
                            with open(file_path, 'r', encoding='utf-8') as f:
                                current_content = f.read().strip()
                            print(f"Contenu actuel de {file_path}: {current_content}")
                        except Exception as e:
                            print(f"Impossible de lire {file_path}: {str(e)}")
                    
                    # Écrire le nouveau contenu
                    with open(file_path, 'w', encoding='utf-8') as f:
                        f.write(realmlist_text)
                    
                    # Vérifier que le fichier a bien été modifié
                    with open(file_path, 'r', encoding='utf-8') as f:
                        new_content = f.read().strip()
                    
                    if new_content == realmlist_text:
                        print(f"Fichier {file_path} mis à jour avec succès")
                        updated_files.append(file_path)
                    else:
                        print(f"Échec de la mise à jour de {file_path}: le contenu ne correspond pas")
                        
                except Exception as e:
                    print(f"Erreur lors de la mise à jour de {file_path}: {str(e)}")
            
            # Mettre à jour la configuration
            self.set("realmlist", realmlist_text)
            
            # Afficher un résumé
            if updated_files:
                print(f"Mise à jour réussie de {len(updated_files)} fichiers realmlist.wtf")
                return True
            else:
                print("Échec de la mise à jour de tous les fichiers realmlist.wtf")
                return False
        
        except Exception as e:
            print(f"Erreur lors de la mise à jour du realmlist: {str(e)}")
            return False
            
    def get_realmlist_entries(self):
        """Get all realmlist entries"""
        return self.settings.get("realmlist_entries", DEFAULT_CONFIG["realmlist_entries"])
        
    def set_realmlist_entry(self, index, name, address, active=False):
        """Set a realmlist entry"""
        if index < 0 or index >= 2:  # Limité à 2 entrées
            return False
            
        entries = self.get_realmlist_entries()
        
        # Mettre à jour l'entrée
        entries[index] = {
            "name": name,
            "address": address,
            "active": active
        }
        
        # Si cette entrée est active, désactiver les autres
        if active:
            for i, entry in enumerate(entries):
                if i != index:
                    entry["active"] = False
            self.set("active_realmlist_index", index)
            
            # Mettre à jour le fichier realmlist.wtf avec la nouvelle adresse
            self.update_realmlist(address)
        
        # Sauvegarder les modifications
        self.set("realmlist_entries", entries)
        return True
        
    def switch_realmlist(self, index):
        """Switch to a different realmlist"""
        entries = self.get_realmlist_entries()
        
        if index < 0 or index >= len(entries) or not entries[index]["address"]:
            return False
            
        # Activer l'entrée sélectionnée et désactiver les autres
        for i, entry in enumerate(entries):
            entry["active"] = (i == index)
        
        # Mettre à jour l'index actif
        self.set("active_realmlist_index", index)
        
        # Mettre à jour le fichier realmlist.wtf
        self.update_realmlist(entries[index]["address"])
        
        # Sauvegarder les modifications
        self.set("realmlist_entries", entries)
        return True
        
    def get_active_realmlist_index(self):
        """Get the index of the active realmlist"""
        return self.get("active_realmlist_index", 0)
