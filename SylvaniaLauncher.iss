; Script d'installation pour Sylvania Launcher
; Créé avec Inno Setup

#define MyAppName "Sylvania Launcher"
#define MyAppVersion "1.1"
#define MyAppPublisher "Sylvania"
#define MyAppURL "https://sylvania-servergame.com/"
#define MyAppExeName "SylvaniaLauncher.exe"

[Setup]
; Identifiant unique pour cette application
AppId={{F8A69425-3E9A-4D47-8E1C-AC3B7D5A6F12}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
DisableProgramGroupPage=yes
LicenseFile=LICENSE
InfoBeforeFile=info_before.txt
InfoAfterFile=info_after.txt
OutputDir=C:\Users\Marty\Desktop
OutputBaseFilename=SylvaniaLauncher_Setup_v1.1
Compression=lzma
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\sylvania_logo.ico
UninstallDisplayName={#MyAppName}
CreateUninstallRegKey=yes
SetupIconFile=C:\Users\Marty\Desktop\Sylvania Launcher\Asset\sylvania_logo.ico

; Note: Les images personnalisées pour l'installateur doivent être au format BMP
; WizardImageFile et WizardSmallImageFile ont été désactivés car ils nécessitent des fichiers BMP

[Languages]
Name: "french"; MessagesFile: "compiler:Languages\French.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "removeuserdata"; Description: "Supprimer les données utilisateur lors de la désinstallation"; GroupDescription: "Options de désinstallation:"; Flags: unchecked

[Files]
; Fichiers principaux du projet - version PyInstaller
Source: "C:\Users\Marty\Desktop\Sylvania Launcher\dist\Sylvania Launcher Portable\*.*"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\Users\Marty\Desktop\Sylvania Launcher\dist\Sylvania Launcher Portable\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
; S'assurer que les assets sont correctement inclus
Source: "C:\Users\Marty\Desktop\Sylvania Launcher\Asset\sylvania_logo.jpeg"; DestDir: "{app}\Asset"; Flags: ignoreversion
Source: "C:\Users\Marty\Desktop\Sylvania Launcher\Asset\sylvania_logo.ico"; DestDir: "{app}\Asset"; Flags: ignoreversion
Source: "C:\Users\Marty\Desktop\Sylvania Launcher\Asset\Background.jpg"; DestDir: "{app}\Asset"; Flags: ignoreversion
; S'assurer que le fichier de version est inclus
Source: "C:\Users\Marty\Desktop\Sylvania Launcher\version.py"; DestDir: "{app}"; Flags: ignoreversion
; Documentation
Source: "C:\Users\Marty\Desktop\Sylvania Launcher\README_TROUBLESHOOTING.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"; IconFilename: "{app}\Asset\sylvania_logo.ico"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"; IconFilename: "{app}\Asset\sylvania_logo.ico"; Tasks: desktopicon

[Run]
; Option pour lancer l'application après l'installation
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[Messages]
WelcomeLabel2=Cet assistant va installer [name/ver] sur votre ordinateur.%n%nCette version inclut :%n- Le nouveau logo SYLVANIA WOW%n- Une meilleure gestion des téléchargements avec reprise en cas d'instabilité réseau%n- Mise à jour de l'URL de téléchargement du client vers sylvania-servergame.com%n%nIl est recommandé de fermer toutes les autres applications avant de continuer.

[Code]
// Variable pour stocker l'état de la case à cocher
var
  RemoveUserDataSelected: Boolean;

// Fonction appelée lors de l'initialisation de l'assistant
procedure InitializeWizard();
begin
  // Initialiser la variable
  RemoveUserDataSelected := False;
end;

// Fonction appelée lorsque l'utilisateur clique sur une case à cocher
procedure CurPageChanged(CurPageID: Integer);
begin
  // Si nous sommes sur la page des tâches, enregistrer l'état de la case à cocher
  if CurPageID = wpSelectTasks then
  begin
    RemoveUserDataSelected := WizardIsTaskSelected('removeuserdata');
  end;
end;

// Fonction pour gérer la désinstallation et supprimer les données utilisateur si demandé
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  DataDir: String;
begin
  if CurUninstallStep = usPostUninstall then
  begin
    // Demander à l'utilisateur s'il souhaite supprimer les données utilisateur
    DataDir := ExpandConstant('{localappdata}\SylvaniaLauncher');
    if DirExists(DataDir) then
    begin
      if MsgBox('Voulez-vous supprimer toutes les données utilisateur de Sylvania Launcher ?', mbConfirmation, MB_YESNO) = IDYES then
      begin
        DelTree(DataDir, True, True, True);
      end;
    end;
  end;
end;
