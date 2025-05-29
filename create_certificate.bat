@echo off
echo Creation d'un certificat auto-signe pour Sylvania Launcher...

:: Creer un dossier pour le certificat s'il n'existe pas
if not exist "Certificate" mkdir Certificate

:: Generer un certificat auto-signe avec OpenSSL
echo Etape 1: Generation du certificat...
powershell -Command "& {
    $cert = New-SelfSignedCertificate -Subject 'CN=Sylvania Launcher, O=BlaMacfly, C=FR' -Type CodeSigning -CertStoreLocation Cert:\CurrentUser\My
    $CertPassword = ConvertTo-SecureString -String 'SylvaniaLauncher2025' -Force -AsPlainText
    Export-PfxCertificate -Cert $cert -FilePath 'Certificate\SylvaniaLauncherCert.pfx' -Password $CertPassword
    Export-Certificate -Cert $cert -FilePath 'Certificate\SylvaniaLauncherCert.cer'
    Write-Host 'Certificat cree avec succes!'
}"

:: Signer l'executable avec le certificat
echo Etape 2: Signature de l'executable...
powershell -Command "& {
    $CertPassword = ConvertTo-SecureString -String 'SylvaniaLauncher2025' -Force -AsPlainText
    $Cert = Get-PfxCertificate -FilePath 'Certificate\SylvaniaLauncherCert.pfx' -Password $CertPassword
    Set-AuthenticodeSignature -FilePath 'dist\SylvaniaLauncher\SylvaniaLauncher.exe' -Certificate $Cert
    Write-Host 'Executable signe avec succes!'
}"

echo Processus termine. L'executable a ete signe avec un certificat auto-signe.
echo Note: Ce certificat n'est pas reconnu par les autorites de certification,
echo mais peut aider a reduire les faux positifs des antivirus.
pause
