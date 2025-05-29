@echo off
echo Téléchargement de Python portable...

:: Définir le dossier d'installation
set INSTALL_DIR=%CD%

:: Créer le dossier python s'il n'existe pas
if not exist "%INSTALL_DIR%\python" mkdir "%INSTALL_DIR%\python"

:: Télécharger Python portable
echo Téléchargement de Python...
powershell -Command "& {[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; Invoke-WebRequest -Uri 'https://www.python.org/ftp/python/3.11.4/python-3.11.4-embed-amd64.zip' -OutFile '%INSTALL_DIR%\python.zip'}"

:: Extraire l'archive
echo Extraction de Python...
powershell -Command "& {Expand-Archive -Path '%INSTALL_DIR%\python.zip' -DestinationPath '%INSTALL_DIR%\python' -Force}"

:: Télécharger get-pip.py
echo Configuration de pip...
powershell -Command "& {[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; Invoke-WebRequest -Uri 'https://bootstrap.pypa.io/get-pip.py' -OutFile '%INSTALL_DIR%\python\get-pip.py'}"

:: Installer pip
"%INSTALL_DIR%\python\python.exe" "%INSTALL_DIR%\python\get-pip.py"

:: Configurer Python pour qu'il puisse utiliser pip
echo import site >> "%INSTALL_DIR%\python\python311._pth"

:: Nettoyer
del "%INSTALL_DIR%\python.zip"
del "%INSTALL_DIR%\python\get-pip.py"

echo Python portable installé avec succès!
