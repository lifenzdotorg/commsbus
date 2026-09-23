; Commsbus Windows installer (Inno Setup 6).
;
; Commsbus ships the 64-bit standalone application only -- the VST2/VST3/AAX
; plugin components and the 32-bit build upstream SonoBus installed are gone.
;
;   iscc /DSBVERSION=0.1.0 wininstaller.iss
;
; Signing is optional. To sign, define SIGN and pass a sign tool named
; "signtool", as distwin.sh does:
;
;   iscc /DSIGN "/Ssigntool=signtool.exe sign /t http://timestamp.digicert.com /f cert.p12 /p pass $f" ...

#ifdef SIGN
  #define SIGNFLAG "signonce"
#else
  #define SIGNFLAG ""
#endif

[Setup]
AppName=Commsbus
AppVersion={#SBVERSION}
AppPublisher=LifeNZ
MinVersion=6.1
WizardStyle=modern
DefaultDirName={autopf}\Commsbus
DefaultGroupName=Commsbus
UninstallDisplayIcon={app}\Commsbus.exe
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
OutputBaseFilename=Commsbus-{#SBVERSION}-Installer
LicenseFile=gpl-3.0.txt
SetupLogging=yes
#ifdef SIGN
SignTool=signtool $f
SignedUninstaller=yes
#endif
DisableReadyPage=true
DisableWelcomePage=yes
DisableDirPage=no
; Commsbus may be running unattended; let the installer close it and restart it
CloseApplications=yes
RestartApplications=yes


[Files]
Source: "Commsbus\Commsbus.exe"; DestDir: "{app}"; Flags: ignoreversion {#SIGNFLAG}
Source: "Commsbus\README.txt"; DestDir: "{app}"; DestName: "README.txt"; Flags: isreadme


[Icons]
Name: "{group}\Commsbus"; Filename: "{app}\Commsbus.exe"
Name: "{group}\README"; Filename: "{app}\README.txt"
Name: "{group}\Uninstall Commsbus"; Filename: "{app}\unins000.exe"

[Registry]
Root: HKCR; Subkey: "commsbus"; ValueType: "string"; ValueData: "URL:commsbus Protocol"; Flags: uninsdeletekey
Root: HKCR; Subkey: "commsbus"; ValueType: "string"; ValueName: "URL Protocol"; ValueData: ""
Root: HKCR; Subkey: "commsbus\DefaultIcon"; ValueType: "string"; ValueData: "{app}\Commsbus.exe,0"
Root: HKCR; Subkey: "commsbus\shell\open\command"; ValueType: "string"; ValueData: """{app}\Commsbus.exe"" ""%1"""


[Code]
var
  OkToCopyLog : Boolean;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssDone then
    OkToCopyLog := True;
end;

procedure DeinitializeSetup();
begin
  if OkToCopyLog then
    FileCopy (ExpandConstant ('{log}'), ExpandConstant ('{app}\InstallationLogFile.log'), FALSE);
  RestartReplace (ExpandConstant ('{log}'), '');
end;


[UninstallDelete]
Type: files; Name: "{app}\InstallationLogFile.log"
