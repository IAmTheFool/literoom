#define MyAppName "Literoom"
#define MyAppExe  "literoom.exe"

; The release workflow passes the real version: ISCC /DMyAppVersion=1.2.3
#ifndef MyAppVersion
  #define MyAppVersion "0.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\..\build\release"
#endif

[Setup]
; Never change this GUID. It's how Windows recognises upgrades of the same app.
AppId={{B7C3E5A2-4D81-4F6A-9C0E-2A5D8F13C7B9}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher=Roshan
AppPublisherURL=https://github.com/IAmTheFool/literoom
AppSupportURL=https://github.com/IAmTheFool/literoom/issues
AppUpdatesURL=https://github.com/IAmTheFool/literoom/releases
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
UninstallDisplayIcon={app}\{#MyAppExe}
OutputDir=..\..\build\installer
OutputBaseFilename=literoom-setup-x64
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequiredOverridesAllowed=dialog
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
SetupIconFile=..\icons\literoom.ico
LicenseFile=..\..\LICENSE

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; Flags: unchecked

[Files]
Source: "{#SourceDir}\{#MyAppExe}"; DestDir: "{app}"; Flags: ignoreversion
; ONNX builds only: onnxruntime*.dll, the Visual C++ runtime DLLs it needs, and
; the MobileSAM models (the app looks in <exe folder>\models). Both lines are
; skipped when the folder has no such files, so non-ONNX builds still work.
Source: "{#SourceDir}\*.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#SourceDir}\models\*"; DestDir: "{app}\models"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExe}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExe}"; Description: "Launch {#MyAppName}"; Flags: nowait postinstall skipifsilent