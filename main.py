#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""
Sylvania Launcher - World of Warcraft 3.3.5 Launcher
Main entry point for the application
"""

import sys
import os

# S'assurer que le répertoire courant est dans le chemin de recherche
current_dir = os.path.dirname(os.path.abspath(__file__))
if current_dir not in sys.path:
    sys.path.insert(0, current_dir)

from PySide6.QtWidgets import QApplication
from gui import MainWindow

def main():
    """Main function to start the application"""
    # Create the Qt Application
    app = QApplication(sys.argv)
    
    # Set application name and organization
    app.setApplicationName("Sylvania Launcher")
    app.setOrganizationName("Sylvania")
    
    # Create and show the main window
    window = MainWindow()
    window.show()
    
    # Execute the application
    return app.exec()

if __name__ == "__main__":
    main()
