# Guide de dépannage pour Sylvania Launcher

Ce document fournit des instructions pour résoudre les problèmes courants avec Sylvania Launcher, en particulier les problèmes liés au bouton "Réglages".

## Problèmes connus

### Le bouton "Réglages" ne fonctionne pas

Si le bouton "Réglages" ne répond pas ou ne fonctionne pas correctement après l'installation, voici quelques étapes à suivre pour diagnostiquer et résoudre le problème :

1. **Utiliser l'outil de test des réglages** :
   - Allez dans le menu Démarrer > Sylvania Launcher > Outils > Test des Réglages
   - Cet outil va effectuer une série de tests et afficher une boîte de dialogue si tout fonctionne correctement
   - Les résultats des tests sont enregistrés dans le dossier `logs` de l'application

2. **Vérifier les journaux** :
   - Consultez les fichiers de journalisation dans le dossier `logs` de l'application
   - Les fichiers commençant par `test_settings_` contiennent les informations de diagnostic
   - Les fichiers commençant par `launcher_` contiennent les journaux de démarrage de l'application

3. **Réinstaller l'application** :
   - Si les problèmes persistent, essayez de désinstaller puis réinstaller l'application
   - Assurez-vous que l'installation est effectuée avec des privilèges administrateur

## Problèmes liés à Python

Si vous rencontrez des erreurs liées à Python ou aux modules Python :

1. **Vérifier l'installation de Python portable** :
   - Assurez-vous que le dossier `python` existe dans le répertoire d'installation
   - Si ce dossier est absent ou incomplet, réinstallez l'application

2. **Vérifier les dépendances** :
   - Ouvrez une invite de commande dans le répertoire d'installation
   - Exécutez la commande suivante pour vérifier les dépendances :
     ```
     python\python.exe -m pip list
     ```
   - Vérifiez que PySide6 est bien installé

## Problèmes d'autorisation

Si vous rencontrez des problèmes d'autorisation :

1. **Vérifier les permissions du dossier d'installation** :
   - Assurez-vous que l'utilisateur actuel a les droits d'écriture dans le dossier d'installation
   - Le dossier `logs` doit être accessible en écriture

2. **Exécuter en tant qu'administrateur** :
   - Essayez d'exécuter l'application en tant qu'administrateur
   - Cliquez avec le bouton droit sur le raccourci et sélectionnez "Exécuter en tant qu'administrateur"

## Contact et support

Si vous continuez à rencontrer des problèmes après avoir suivi ces étapes, veuillez contacter le support technique à l'adresse suivante : support@sylvania.fr

N'oubliez pas de joindre les fichiers de journalisation pertinents à votre demande de support.
