"""
Script de test pour le bouton Réglages du Sylvania Launcher
Ce script teste spécifiquement la fonctionnalité du bouton Réglages
et fournit des informations de diagnostic détaillées.
"""

import os
import sys
import logging
import traceback
from datetime import datetime

# Configuration du journal
log_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "logs")
os.makedirs(log_dir, exist_ok=True)
log_file = os.path.join(log_dir, f"test_settings_{datetime.now().strftime('%Y%m%d_%H%M%S')}.log")

logging.basicConfig(
    filename=log_file,
    level=logging.DEBUG,
    format='%(asctime)s - %(levelname)s - %(message)s'
)

console = logging.StreamHandler()
console.setLevel(logging.INFO)
logging.getLogger('').addHandler(console)

def main():
    """Fonction principale de test"""
    try:
        logging.info("=== Test du bouton Réglages ===")
        logging.info(f"Répertoire courant: {os.getcwd()}")
        logging.info(f"Chemin Python: {sys.executable}")
        logging.info(f"Version Python: {sys.version}")
        
        # Vérifier les fichiers essentiels
        essential_files = ["gui.py", "config.py", "main.py", "debug_settings.py"]
        for file in essential_files:
            file_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), file)
            exists = os.path.exists(file_path)
            logging.info(f"Fichier {file}: {'Trouvé' if exists else 'MANQUANT'}")
            
            if exists and file == "gui.py":
                # Vérifier si la méthode open_settings existe dans gui.py
                with open(file_path, "r", encoding="utf-8") as f:
                    content = f.read()
                    if "def open_settings" in content:
                        logging.info("La méthode open_settings est définie dans gui.py")
                    else:
                        logging.warning("La méthode open_settings n'a pas été trouvée dans gui.py")
        
        # Tester l'importation des modules essentiels
        logging.info("Test d'importation des modules essentiels...")
        
        try:
            import PySide6
            logging.info(f"PySide6 importé avec succès (version: {PySide6.__version__})")
        except ImportError as e:
            logging.error(f"Erreur lors de l'importation de PySide6: {str(e)}")
        
        try:
            from PySide6.QtWidgets import QDialog, QVBoxLayout, QLabel
            logging.info("Widgets PySide6 importés avec succès")
        except ImportError as e:
            logging.error(f"Erreur lors de l'importation des widgets PySide6: {str(e)}")
        
        # Tester la création d'une boîte de dialogue simple
        try:
            logging.info("Test de création d'une boîte de dialogue...")
            from PySide6.QtWidgets import QApplication, QDialog, QVBoxLayout, QLabel, QPushButton
            
            app = QApplication.instance() or QApplication(sys.argv)
            
            dialog = QDialog()
            dialog.setWindowTitle("Test de boîte de dialogue")
            
            layout = QVBoxLayout(dialog)
            layout.addWidget(QLabel("Si vous voyez cette boîte de dialogue, le test est réussi."))
            
            button = QPushButton("Fermer")
            button.clicked.connect(dialog.accept)
            layout.addWidget(button)
            
            logging.info("Boîte de dialogue créée avec succès, affichage...")
            result = dialog.exec_()
            logging.info(f"Boîte de dialogue fermée avec le code: {result}")
            
        except Exception as e:
            logging.error(f"Erreur lors de la création de la boîte de dialogue: {str(e)}")
            traceback.print_exc(file=open(log_file, "a"))
        
        logging.info("Test terminé. Consultez le fichier de log pour plus de détails.")
        print(f"Test terminé. Journal enregistré dans: {log_file}")
        
    except Exception as e:
        logging.error(f"Erreur générale: {str(e)}")
        traceback.print_exc(file=open(log_file, "a"))
        print(f"Une erreur s'est produite. Consultez le journal: {log_file}")

if __name__ == "__main__":
    main()
