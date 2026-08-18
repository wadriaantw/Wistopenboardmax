; ===========================================================================
;  WistOpenboard — Inno Setup script (Josh / coworker build)
;  Same as WistOpenboard.iss. Historically sourced from C:\WistOpenboard-Coworker\
;  (which carried the touch-pan deadzone fix before it was merged into main);
;  the fix is now in the main source, so both scripts build from the same
;  deployed product folder.
; ===========================================================================

#define MyAppName        "WistOpenboard"
#define MyAppVersion     "2026.3"
#define MyAppPublisher   "Adriaan Willemse"
#define MyAppExeName     "OpenBoard.exe"
#define MyAppId          "{{C9F5C5BD-2026-4E1A-9F88-D7E4A8C14BDE}"
#define ProductDir       "C:\openboard-fork\build\build\win32\release\product"
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

; VC++ runtime is ALWAYS embedded in the installer (compile fails if the file
; is missing next to this .iss — that's intentional, we want it bundled).
Source: "{#VCRedistFile}"; DestDir: "{tmp}"; Flags: deleteafterinstall

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
{ App is built with MSVC 2022 and needs VCRUNTIME140_1.dll, which ships with
  the 2019+ runtime (14.20 or newer). Older 2015/2017 runtimes (14.0-14.1x)
  register Major=14 too, so we must also check Minor. }
function VCRedistAlreadyInstalled: Boolean;
var
  installed: Cardinal;
  major:     Cardinal;
  minor:     Cardinal;
begin
  Result := False;
  if RegQueryDWordValue(HKLM64,
       'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64',
       'Installed', installed) and (installed = 1) then
  begin
    if RegQueryDWordValue(HKLM64,
         'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64',
         'Major', major) and
       RegQueryDWordValue(HKLM64,
         'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64',
         'Minor', minor) then
    begin
      Result := (major > 14) or ((major = 14) and (minor >= 20));
    end;
  end;
end;

function NeedsVCRedistInstall: Boolean;
begin
  Result := not VCRedistAlreadyInstalled;
end;
