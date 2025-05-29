@echo off
setlocal enabledelayedexpansion

:: Créer le dossier de logs s'il n'existe pas
if not exist "%~dp0logs" mkdir "%~dp0logs"

:: Définir le fichier de log avec horodatage
set LOGFILE=%~dp0logs\launcher_%date:~6,4%%date:~3,2%%date:~0,2%_%time:~0,2%%time:~3,2%%time:~6,2%.log
set LOGFILE=!LOGFILE: =0!

echo ===== Démarrage de Sylvania Launcher (Portable) ===== > "%LOGFILE%"
echo Date et heure: %date% %time% >> "%LOGFILE%"
echo Répertoire: %~dp0 >> "%LOGFILE%"

:: Créer le fichier portable.txt s'il n'existe pas
if not exist "%~dp0portable.txt" (
    echo Cette installation est une version portable du Sylvania Launcher. > "%~dp0portable.txt"
    echo Date de création: %date% %time% >> "%~dp0portable.txt"
    echo Fichier portable.txt créé >> "%LOGFILE%"
)

:: Créer le dossier data s'il n'existe pas
if not exist "%~dp0data" (
    mkdir "%~dp0data"
    echo Dossier data créé >> "%LOGFILE%"
)

:: Vérifier si Python portable existe
if not exist "%~dp0python\pythonw.exe" (
    echo ERREUR: Python portable non trouvé >> "%LOGFILE%"
    echo Python portable non trouvé. Installation en cours...
    
    :: Exécuter le script get_python.bat
    echo Exécution de get_python.bat... >> "%LOGFILE%"
    call "%~dp0get_python.bat"
    
    if not exist "%~dp0python\pythonw.exe" (
        echo ERREUR: L'installation de Python a échoué >> "%LOGFILE%"
        echo L'installation de Python a échoué. Veuillez télécharger manuellement Python.
        pause
        exit /b 1
    )
    
    :: Installer les dépendances
    echo Installation des dépendances... >> "%LOGFILE%"
    "%~dp0python\python.exe" -m pip install -r "%~dp0requirements.txt"
    
    echo Python et dépendances installés avec succès >> "%LOGFILE%"
)

:: Vérifier si le fichier principal existe
if not exist "%~dp0main.py" (
    echo ERREUR: main.py non trouvé >> "%LOGFILE%"
    echo Fichier principal (main.py) non trouvé. Veuillez réinstaller l'application.
    pause
    exit /b 1
)

:: Vérifier les fichiers essentiels
echo Vérification des fichiers essentiels... >> "%LOGFILE%"
for %%F in (gui.py config.py download_client.py game_tracker.py) do (
    if not exist "%~dp0%%F" (
        echo AVERTISSEMENT: Fichier %%F non trouvé >> "%LOGFILE%"
    ) else (
        echo OK: Fichier %%F trouvé >> "%LOGFILE%"
    )
)

:: Lancer l'application
echo Lancement de l'application... >> "%LOGFILE%"
cd /d "%~dp0"
start "Sylvania Launcher Portable" "%~dp0python\pythonw.exe" "%~dp0main.py" 2>> "%LOGFILE%"

echo Application lancée avec succès >> "%LOGFILE%"
exit /b 0
