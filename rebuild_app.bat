@echo off
echo Reconstruction de Sylvania Launcher avec toutes les corrections...

:: Nettoyer les anciens builds
echo Nettoyage des anciens builds...
if exist "build" rmdir /s /q "build"
if exist "dist" rmdir /s /q "dist"

:: Compiler l'application avec PyInstaller
echo Compilation de l'application...
.\python\python.exe -m PyInstaller --noconfirm SylvaniaLauncher.spec

:: Copier les fichiers d'assets dans le dossier dist
echo Copie des assets...
xcopy /y /s /i "Asset" "dist\SylvaniaLauncher\Asset"

:: Copier l'icône à la racine pour les raccourcis
echo Copie de l'icône pour les raccourcis...
copy /y "Asset\sylvania_logo.ico" "dist\SylvaniaLauncher\sylvania_logo.ico"

:: Créer un certificat et signer l'application
echo Création du certificat et signature de l'application...
call create_certificate.bat

echo Reconstruction terminée. L'application est prête à être distribuée.
echo Vous pouvez maintenant compiler l'installateur avec Inno Setup.
pause
