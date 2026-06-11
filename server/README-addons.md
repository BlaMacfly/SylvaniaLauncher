# Manifeste des addons Legion — hébergement & maintenance

Ce dossier contient le manifeste des addons recommandés **Legion 7.3.5** affichés
dans le launcher Sylvania (onglet AddOns en mode Legion).

## 1. Déploiement (à faire une fois)

Dépose `addons.json` à cette URL exacte :

```
https://legendesylvania.com/addons.json
```

C'est tout. Au prochain lancement, le launcher récupère cette liste à distance.

- Tant que ce fichier est **absent ou invalide**, le launcher utilise sa copie
  embarquée (identique à celle-ci au moment de la release) — il ne casse jamais.
- Une fois le fichier en ligne, **ajouter / modifier / retirer un addon ne
  demande plus de recompiler le launcher** : il suffit d'éditer ce JSON.

> ⚠️ L'URL est figée dans le launcher (`kLegionAddonManifestUrl`). Si tu changes
> d'emplacement, il faut une nouvelle version du launcher. Garde donc bien
> `https://legendesylvania.com/addons.json`.

## 2. Structure d'une entrée

```json
{
  "id": "elvui",                      // identifiant unique, minuscules, stable
  "name": "ElvUI",                    // nom affiché
  "version": "7.3.5",                 // version affichée
  "description": "…",                 // une phrase
  "url": "https://legendesylvania.com/assets/downloads/addons/elvui.zip",
  "folders": ["ElvUI", "ElvUI_Config"]
}
```

### Règles importantes

- **`url`** doit pointer vers l'hôte **`legendesylvania.com`** en `https` —
  le launcher refuse tout autre domaine (sécurité anti-détournement).
- **`folders`** = les **vrais dossiers d'addon** contenus dans le zip, c'est-à-dire
  ceux qui contiennent un fichier `.toc`. **Ce n'est ni le nom affiché, ni le
  dossier d'emballage** (`ElvUI-Legion-main`, `DeadlyBossMods-7.3.31`…).
  N'y mets **pas** les bibliothèques partagées (`LibStub`, `Ace*`, `oUF`…).
  Ce champ sert à détecter « déjà installé » et à désinstaller proprement ;
  s'il est faux, l'addon s'installe mais le launcher ne le « voit » pas.

## 3. Ajouter un nouvel addon

1. Dépose l'archive sur le serveur, p. ex.
   `https://legendesylvania.com/assets/downloads/addons/monaddon.zip`
2. Récupère ses vrais dossiers d'addon (PowerShell) :

   ```powershell
   Add-Type -AssemblyName System.IO.Compression.FileSystem
   ([System.IO.Compression.ZipFile]::OpenRead("C:\chemin\monaddon.zip")).Entries |
     Where-Object Name -match '\.toc$' |
     ForEach-Object { ($_.FullName -replace '\\','/' -split '/')[-2] } |
     Sort-Object -Unique
   ```
   (retire de la sortie les libs partagées : `LibStub`, `Lib*`, `Ace*`, `oUF*`…)

3. Ajoute l'entrée dans `addons.json` et ré-uploade le fichier. Fini.

## 4. Vérifier que le JSON est valide avant upload

```powershell
Get-Content addons.json -Raw | ConvertFrom-Json | Out-Null; "OK"
```

Si la commande affiche `OK`, le fichier est syntaxiquement correct.
