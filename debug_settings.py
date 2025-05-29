import os
import sys
import logging
from datetime import datetime

# Configure logging
log_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "logs")
os.makedirs(log_dir, exist_ok=True)

log_file = os.path.join(log_dir, f"debug_settings_{datetime.now().strftime('%Y%m%d_%H%M%S')}.log")
logging.basicConfig(
    filename=log_file,
    level=logging.DEBUG,
    format='%(asctime)s - %(levelname)s - %(message)s'
)

def debug_settings_button():
    """
    Fonction de débogage pour le bouton Réglages
    Cette fonction est appelée depuis gui.py pour diagnostiquer les problèmes
    """
    try:
        logging.info("=== Débogage du bouton Réglages ===")
        logging.info(f"Répertoire courant: {os.getcwd()}")
        logging.info(f"Chemin Python: {sys.executable}")
        logging.info(f"Version Python: {sys.version}")
        logging.info(f"Chemin système: {sys.path}")
        
        # Vérifier si les modules nécessaires sont disponibles
        try:
            from PySide6.QtWidgets import QDialog
            logging.info("Module PySide6.QtWidgets importé avec succès")
        except ImportError as e:
            logging.error(f"Erreur d'importation PySide6.QtWidgets: {str(e)}")
        
        # Vérifier les fichiers de configuration
        config_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "config.json")
        logging.info(f"Chemin de configuration: {config_path}")
        logging.info(f"Le fichier de configuration existe: {os.path.exists(config_path)}")
        
        # Vérifier les fichiers d'assets
        assets_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "Asset")
        logging.info(f"Répertoire des assets: {assets_dir}")
        logging.info(f"Le répertoire des assets existe: {os.path.exists(assets_dir)}")
        
        return True
    except Exception as e:
        logging.error(f"Erreur lors du débogage: {str(e)}")
        return False

if __name__ == "__main__":
    # Test direct du script
    result = debug_settings_button()
    print(f"Résultat du débogage: {result}")
    print(f"Journal de débogage créé: {log_file}")
