#ifndef AppVersion
  #error AppVersion is required
#endif
#ifndef FileVersion
  #error FileVersion is required
#endif
#ifndef ApplicationDirectory
  #error ApplicationDirectory is required
#endif
#ifndef ProjectRoot
  #error ProjectRoot is required
#endif

[Setup]
AppId={{24C18A48-5E2D-4A30-9C92-9D2AB248446B}
AppName=ASTROCHRON
AppVersion={#AppVersion}
AppPublisher=ASTROCHRON
AppPublisherURL=https://github.com/KaffuAlcaid/ASTROCHRON
AppSupportURL=https://github.com/KaffuAlcaid/ASTROCHRON/issues
DefaultDirName={localappdata}\Programs\ASTROCHRON
DefaultGroupName=ASTROCHRON
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.22000
DisableDirPage=no
DisableProgramGroupPage=yes
UninstallDisplayIcon={app}\ASTROCHRON.exe
SetupIconFile={#ProjectRoot}\assets\app-icon\astrochron.ico
LicenseFile={#ProjectRoot}\LICENSE
OutputBaseFilename=ASTROCHRON-v{#AppVersion}-windows-x64-setup
VersionInfoVersion={#FileVersion}
VersionInfoProductVersion={#FileVersion}
VersionInfoProductTextVersion={#AppVersion}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "chinesesimplified"; MessagesFile: "compiler:Languages\ChineseSimplified.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#ApplicationDirectory}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\ASTROCHRON"; Filename: "{app}\ASTROCHRON.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\ASTROCHRON"; Filename: "{app}\ASTROCHRON.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\ASTROCHRON.exe"; Description: "{cm:LaunchProgram,ASTROCHRON}"; Flags: nowait postinstall skipifsilent
