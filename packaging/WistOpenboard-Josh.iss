; ===========================================================================
;  WistOpenboard — Inno Setup script (Josh / coworker build)
;  Same as WistOpenboard.iss but sources from C:\WistOpenboard-Coworker\
;  which includes the 15 px touch-pan deadzone fix.
; ===========================================================================

#define MyAppName        "WistOpenboard"
#define MyAppVersion     "2026.2"
#define MyAppPublisher   "Adriaan Willemse"
#define MyAppExeName     "OpenBoard.exe"
#define MyAppId          "{{C9F5C5BD-2026-4E1A-9F88-D7E4A8C14BDE}"
#define ProductDir       "C:\WistOpenboard-Coworker"
#define VCRedistFile     "vc_redist.x64.exe"

[Setup]
AppId={#MyAppId}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
VersionInfoVersion={#MyAppVersion}.0
AppPublisher={#MyAppPublisher}
AppVerName={#MyAppName} {#MyAppVersion}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
DisableDirPage=auto
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible
PrivilegesRequired=admin
OutputDir=Output
OutputBaseFilename=WistOpenboard-Josh-Setup-{#MyAppVersion}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\{#MyAppExeName}
UninstallDisplayName={#MyAppName} {#MyAppVersion}
SetupIconFile=

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon";    Description: "{cm:CreateDesktopIcon}";  GroupDescription: "{cm:AdditionalIcons}";  Flags: checkedonce
Name: "quicklaunchicon"; Description: "{cm:CreateQuickLaunchIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked; OnlyBelowVersion: 6.1

[Files]
Source: "{#ProductDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

Source: "{#VCRedistFile}"; DestDir: "{tmp}"; Flags: deleteafterinstall skipifsourcedoesntexist; Check: VCRedistAvailable

[Icons]
Name: "{group}\{#MyAppName}";       Filename: "{app}\{#MyAppExeName}"
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{tmp}\{#VCRedistFile}"; Parameters: "/install /quiet /norestart"; \
    StatusMsg: "Installing Visual C++ 2022 runtime..."; \
    Flags: waituntilterminated; \
    Check: NeedsVCRedistInstall

Filename: "{app}\{#MyAppExeName}"; Description: "Launch {#MyAppName}"; \
    Flags: nowait postinstall skipifsilent

[Code]
function VCRedistAvailable: Boolean;
begin
  Result := FileExists(ExpandConstant('{src}\{#VCRedistFile}'));
end;

function VCRedistAlreadyInstalled: Boolean;
var
  installed: Cardinal;
  major:     Cardinal;
begin
  Result := False;
  if RegQueryDWordValue(HKLM64,
       'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64',
       'Installed', installed) and (installed = 1) then
  begin
    if RegQueryDWordValue(HKLM64,
         'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64',
         'Major', major) then
    begin
      Result := (major >= 14);
    end
    else
      Result := True;
  end;
end;

function NeedsVCRedistInstall: Boolean;
begin
  Result := VCRedistAvailable and (not VCRedistAlreadyInstalled);
end;
