@echo off
echo Correction des icones pour Sylvania Launcher...

:: Chemin vers l'icone
set ICON_PATH=%~dp0Asset\sylvania_logo.ico

:: Copier l'icone vers un emplacement temporaire pour la compilation
copy /Y "%ICON_PATH%" "%TEMP%\sylvania_icon.ico"

:: Recompiler l'application avec PyInstaller en specifiant explicitement l'icone
echo Recompilation de l'application avec la bonne icone...
.\python\python.exe -m PyInstaller --noconfirm --clean --icon="%TEMP%\sylvania_icon.ico" --name=SylvaniaLauncher main.py

:: Copier l'icone dans le dossier dist
copy /Y "%ICON_PATH%" "dist\SylvaniaLauncher\sylvania_logo.ico"

:: Creer un fichier .inf pour forcer l'association de l'icone
echo [Version] > "dist\SylvaniaLauncher\icon.inf"
echo Signature="$Windows NT$" >> "dist\SylvaniaLauncher\icon.inf"
echo [DefaultInstall] >> "dist\SylvaniaLauncher\icon.inf"
echo UpdateIcons=UpdateIcons >> "dist\SylvaniaLauncher\icon.inf"
echo [UpdateIcons] >> "dist\SylvaniaLauncher\icon.inf"
echo %%11%%\SylvaniaLauncher.exe,1,sylvania_logo.ico >> "dist\SylvaniaLauncher\icon.inf"

echo Compilation terminee. Les icones ont ete corrigees.
echo Vous pouvez maintenant recompiler l'installateur avec Inno Setup.
pause
