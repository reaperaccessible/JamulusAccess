; Inno Setup script for JamulusAccess (built by jamulusaccess\build.ps1)

#define MyAppName "JamulusAccess"
#define MyAppPublisher "ReaperAccessible"
#define MyAppURL "https://github.com/reaperaccessible/JamulusAccess"
#define MyAppExeName "JamulusAccess.exe"

; passed by build.ps1: /DMyAppVersion=1.00 /DAppDir=... /DRedistDir=... /DSrcDir=... /DOutputDir=...
#ifndef MyAppVersion
  #error MyAppVersion is required
#endif

[Setup]
AppId={{3E3E877B-3478-4D1C-990A-2F4B50E20E9A}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}/issues
AppUpdatesURL={#MyAppURL}/releases
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputBaseFilename=JamulusAccessInstaller_{#MyAppVersion}
OutputDir={#OutputDir}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
SetupIconFile={#SrcDir}\src\res\win-mainicon.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
UninstallDisplayName={#MyAppName}
VersionInfoCompany={#MyAppPublisher}
VersionInfoProductName={#MyAppName}
VersionInfoVersion={#MyAppVersion}
VersionInfoDescription={#MyAppName} Setup
; an update while JamulusAccess runs: close it first (Restart Manager), never leave the old exe
CloseApplications=yes
UsedUserAreasWarning=no
RestartApplications=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "french"; MessagesFile: "compiler:Languages\French.isl"

[CustomMessages]
english.ServerShortcut=JamulusAccess Server
english.ManualShortcut=JamulusAccess Manual
english.LaunchApp=Start JamulusAccess
french.ServerShortcut=Serveur JamulusAccess
french.ManualShortcut=Manuel de JamulusAccess
french.LaunchApp=Démarrer JamulusAccess
; the standard CreateDesktopIcon message has an "&" that screen readers read in the task list
english.DesktopIcon=Create a desktop icon
french.DesktopIcon=Créer une icône sur le Bureau
english.ManualFile=manual_en.html
french.ManualFile=manual_fr.html

[Tasks]
Name: "desktopicon"; Description: "{cm:DesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; the deployed program: JamulusAccess.exe, Qt, Qt's translations, docs\ (manual and changelog)
Source: "{#AppDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
; Microsoft C++ runtime, next to the exe (no separate runtime installer)
Source: "{#RedistDir}\msvcp140*.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#RedistDir}\vcruntime140*.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#RedistDir}\concrt140.dll"; DestDir: "{app}"; Flags: ignoreversion
; licence (GNU AGPL 3.0 or later)
Source: "{#SrcDir}\COPYING"; DestDir: "{app}"; DestName: "COPYING.txt"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{#MyAppName}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autoprograms}\{#MyAppName}\{cm:ServerShortcut}"; Filename: "{app}\{#MyAppExeName}"; Parameters: "-s"; IconFilename: "{app}\{#MyAppExeName}"
Name: "{autoprograms}\{#MyAppName}\{cm:ManualShortcut}"; Filename: "{app}\docs\{cm:ManualFile}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Registry]
; Win+R
Root: HKA; Subkey: "SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\{#MyAppExeName}"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName}"; Flags: uninsdeletekey

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchApp}"; Flags: nowait postinstall skipifsilent runasoriginaluser
; automatic update from the program (silent install): start JamulusAccess again, as the user
Filename: "{app}\{#MyAppExeName}"; Flags: nowait runasoriginaluser; Check: WizardSilent

; The settings (in AppData) are never removed: the user keeps them.
