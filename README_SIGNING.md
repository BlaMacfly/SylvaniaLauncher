# Guide de signature numérique pour Sylvania Launcher

Ce document explique comment utiliser le certificat auto-signé pour réduire les faux positifs des antivirus.

## Pourquoi signer l'application ?

La signature numérique d'une application permet de :
- Prouver l'authenticité du logiciel
- Réduire les faux positifs des antivirus
- Améliorer la confiance des utilisateurs

## Instructions d'utilisation

1. **Compilation de l'application**
   - Compilez d'abord l'application avec PyInstaller en utilisant le fichier .spec modifié
   - Exécutez : `.\python\python.exe -m PyInstaller --noconfirm SylvaniaLauncher.spec`

2. **Création et application du certificat**
   - Exécutez le script `create_certificate.bat`
   - Ce script va :
     - Créer un certificat auto-signé dans le dossier "Certificate"
     - Signer l'exécutable avec ce certificat

3. **Compilation de l'installateur**
   - Une fois l'exécutable signé, compilez l'installateur avec Inno Setup
   - Ouvrez `SylvaniaLauncher.iss` et cliquez sur "Compiler"

## Limitations

- Ce certificat est auto-signé et n'est pas reconnu par les autorités de certification
- Les utilisateurs peuvent toujours voir des avertissements de sécurité
- Pour une solution plus robuste, envisagez d'acheter un certificat de signature de code auprès d'une autorité de certification reconnue

## Mot de passe du certificat

Le mot de passe du certificat est : `SylvaniaLauncher2025`

**Note importante** : Dans un environnement de production réel, ne partagez jamais le mot de passe du certificat dans un fichier README. Cette configuration est uniquement destinée à un usage de développement.
