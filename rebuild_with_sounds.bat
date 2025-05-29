@echo off
echo Reconstruction de Sylvania Launcher avec tous les sons...

:: Fermer l'application si elle est en cours d'execution
echo Fermeture de l'application si elle est en cours d'execution...
taskkill /F /IM SylvaniaLauncher.exe 2>nul

:: Attendre un peu pour s'assurer que l'application est fermee
timeout /t 2 /nobreak >nul

:: Nettoyer les anciens builds
echo Nettoyage des anciens builds...
if exist "build" rmdir /s /q "build"
if exist "dist" rmdir /s /q "dist"

:: Creer le dossier Sounds dans Asset s'il n'existe pas
echo Creation du dossier Sounds dans Asset...
if not exist "Asset\Sounds" mkdir "Asset\Sounds"

:: Copier les fichiers audio dans Asset\Sounds
echo Copie des fichiers audio dans Asset\Sounds...
if exist "Sounds\*.mp3" copy /y "Sounds\*.mp3" "Asset\Sounds\"
if exist "Sounds\*.wav" copy /y "Sounds\*.wav" "Asset\Sounds\"

:: Compiler l'application avec PyInstaller
echo Compilation de l'application...
.\python\python.exe -m PyInstaller --noconfirm SylvaniaLauncher.spec

:: Copier les fichiers d'assets dans le dossier dist
echo Copie des assets...
xcopy /y /s /i "Asset" "dist\SylvaniaLauncher\Asset"

:: Copier les fichiers audio dans le dossier dist
echo Copie des fichiers audio...
if not exist "dist\SylvaniaLauncher\Sounds" mkdir "dist\SylvaniaLauncher\Sounds"
if exist "Sounds\*.mp3" xcopy /y "Sounds\*.mp3" "dist\SylvaniaLauncher\Sounds\"
if exist "Sounds\*.wav" xcopy /y "Sounds\*.wav" "dist\SylvaniaLauncher\Sounds\"

:: Copier l'icône à la racine pour les raccourcis
echo Copie de l'icône pour les raccourcis...
copy /y "Asset\sylvania_logo.ico" "dist\SylvaniaLauncher\sylvania_logo.ico"

echo Reconstruction terminée. L'application est prête à être distribuée.
echo Vous pouvez maintenant compiler l'installateur avec Inno Setup.
pause
