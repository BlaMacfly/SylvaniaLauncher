@echo off
echo Mise a jour des icones pour Sylvania Launcher...

:: Chemin vers l'icone
set ICON_PATH=%~dp0Asset\sylvania_logo.ico

:: Mettre a jour l'icone du raccourci bureau si elle existe
if exist "%USERPROFILE%\Desktop\Sylvania Launcher.lnk" (
    echo Mise a jour de l'icone du raccourci bureau...
    powershell -Command "$WshShell = New-Object -ComObject WScript.Shell; $Shortcut = $WshShell.CreateShortcut('%USERPROFILE%\Desktop\Sylvania Launcher.lnk'); $Shortcut.IconLocation = '%ICON_PATH%'; $Shortcut.Save()"
    echo Icone du raccourci bureau mise a jour.
)

:: Mettre a jour l'icone du raccourci menu demarrer si elle existe
if exist "%APPDATA%\Microsoft\Windows\Start Menu\Programs\Sylvania Launcher.lnk" (
    echo Mise a jour de l'icone du raccourci menu demarrer...
    powershell -Command "$WshShell = New-Object -ComObject WScript.Shell; $Shortcut = $WshShell.CreateShortcut('%APPDATA%\Microsoft\Windows\Start Menu\Programs\Sylvania Launcher.lnk'); $Shortcut.IconLocation = '%ICON_PATH%'; $Shortcut.Save()"
    echo Icone du raccourci menu demarrer mise a jour.
)

echo Mise a jour des icones terminee.
pause
