@echo off
echo Création du package portable Sylvania Launcher...

:: Définir le répertoire de sortie
set OUTPUT_DIR=C:\Users\Marty\Desktop\SylvaniaLauncher_Portable

:: Créer le répertoire de sortie s'il n'existe pas
if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"

:: Copier tous les fichiers du projet
echo Copie des fichiers du projet...
xcopy /E /I /Y "C:\Users\Marty\Desktop\Sylvania Launcher\*.*" "%OUTPUT_DIR%\"

:: Supprimer les fichiers inutiles pour la version portable
echo Nettoyage des fichiers inutiles...
if exist "%OUTPUT_DIR%\build" rd /S /Q "%OUTPUT_DIR%\build"
if exist "%OUTPUT_DIR%\dist" rd /S /Q "%OUTPUT_DIR%\dist"
if exist "%OUTPUT_DIR%\__pycache__" rd /S /Q "%OUTPUT_DIR%\__pycache__"
del /Q "%OUTPUT_DIR%\*.spec"
del /Q "%OUTPUT_DIR%\SylvaniaLauncher.iss"

:: Renommer le fichier batch portable
echo Préparation du lanceur portable...
copy "%OUTPUT_DIR%\SylvaniaLauncher_Portable.bat" "%OUTPUT_DIR%\SylvaniaLauncher.bat" /Y

:: Créer le fichier portable.txt
echo Cette installation est une version portable du Sylvania Launcher. > "%OUTPUT_DIR%\portable.txt"
echo Date de création: %date% %time% >> "%OUTPUT_DIR%\portable.txt"

:: Créer les dossiers nécessaires
echo Création des dossiers requis...
if not exist "%OUTPUT_DIR%\logs" mkdir "%OUTPUT_DIR%\logs"
if not exist "%OUTPUT_DIR%\data" mkdir "%OUTPUT_DIR%\data"

echo Package portable créé avec succès dans: %OUTPUT_DIR%
echo.
echo Vous pouvez maintenant copier ce dossier sur une clé USB ou l'archiver en ZIP.
pause
