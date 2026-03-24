#define MyAppName "CyberCompanionCpp"
#ifndef MyAppVersion
  #define MyAppVersion "1.0.0"
#endif
#define MyAppPublisher "Aemeath"
#define MyAppExeName "CyberCompanionCpp.exe"
#ifndef BuildOutputDir
  #define BuildOutputDir "..\\out\\package\\windows-ninja-release"
#endif

[Setup]
AppId={{3F3DFD32-6E39-4A63-9BC6-34A97A580D52}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=..\out\installer
OutputBaseFilename=CyberCompanionCppSetup
Compression=lzma
SolidCompression=yes
WizardStyle=modern

[Files]
Source: "{#BuildOutputDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Launch {#MyAppName}"; Flags: nowait postinstall skipifsilent
