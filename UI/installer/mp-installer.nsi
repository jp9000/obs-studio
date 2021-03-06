;
; NSIS Installer Script for OBS Studio, https://obsproject.com/
;
; This installer script is designed only for the release process
; of OBS Studio. It requires a lot of files to be in exactly the
; right places. If you're making a fork, it's strongly suggested
; that you make your own installer.
;
; If you choose to use this script anyway, be absolutely sure you
; have replaced every OBS specific check, whether process names,
; application names, files, registry entries, etc.
;
; This script also requires OBSInstallerUtils for additional
; functions. You can find it at
; https://github.com/notr1ch/OBSInstallerUtils

Unicode true
ManifestDPIAware true

; Define your application name
!define APPNAME "OBS Studio"

!ifndef APPVERSION
!define APPVERSION "25.0.8"
!define SHORTVERSION "25.0.8"
!endif

!define APPNAMEANDVERSION "OBS Studio ${SHORTVERSION}"

; Additional script dependencies
!include WinVer.nsh
!include x64.nsh

; Main Install settings
Name "${APPNAMEANDVERSION}"
!ifdef INSTALL64
InstallDir "$PROGRAMFILES64\obs-studio"
!else
InstallDir "$PROGRAMFILES32\obs-studio"
!endif
InstallDirRegKey HKLM "Software\${APPNAME}" ""

!ifdef INSTALL64
 OutFile "OBS-Studio-${SHORTVERSION}-Full-Installer-x64.exe"
!else
 OutFile "OBS-Studio-${SHORTVERSION}-Full-Installer-x86.exe"
!endif

; Use compression
SetCompressor /SOLID LZMA

; Need Admin
RequestExecutionLevel admin

; Modern interface settings
!include "MUI.nsh"

!define MUI_ICON "obs.ico"
!define MUI_HEADERIMAGE_BITMAP "OBSHeader.bmp"
!define MUI_WELCOMEFINISHPAGE_BITMAP "OBSBanner.bmp"

!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN
!define MUI_FINISHPAGE_RUN_TEXT "Launch OBS Studio ${SHORTVERSION}"
!define MUI_FINISHPAGE_RUN_FUNCTION "LaunchOBS"

!define MUI_WELCOMEPAGE_TEXT "This setup will guide you through installing OBS Studio.\n\nIt is recommended that you close all other applications before starting, including OBS Studio. This will make it possible to update relevant files without having to reboot your computer.\n\nClick Next to continue."

!define MUI_PAGE_CUSTOMFUNCTION_LEAVE PreReqCheck

!define MUI_HEADERIMAGE
!define MUI_PAGE_HEADER_TEXT "License Information"
!define MUI_PAGE_HEADER_SUBTEXT "Please review the license terms before installing OBS Studio."
!define MUI_LICENSEPAGE_TEXT_TOP "Press Page Down or scroll to see the rest of the license."
!define MUI_LICENSEPAGE_TEXT_BOTTOM " "
!define MUI_LICENSEPAGE_BUTTON "&Next >"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "new\core\data\obs-studio\license\gplv2.txt"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

;!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_COMPONENTS
!insertmacro MUI_UNPAGE_INSTFILES

; Set languages (first is default language)
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_RESERVEFILE_LANGDLL

Function PreReqCheck
!ifdef INSTALL64
	${if} ${RunningX64}
	${Else}
		MessageBox MB_OK|MB_ICONSTOP "This version of OBS Studio is not compatible with your system.  Please use the 32bit (x86) installer."
	${EndIf}
	; Abort on XP or lower
!endif

	${If} ${AtMostWinVista}
		MessageBox MB_OK|MB_ICONSTOP "Due to extensive use of DirectX 10 features, ${APPNAME} requires Windows 7 or higher and cannot be installed on this version of Windows."
		Quit
	${EndIf}

!ifdef INSTALL64
	; 64 bit Visual Studio 2019 runtime check
	ClearErrors
	SetOutPath "$PLUGINSDIR"
	File check_for_64bit_visual_studio_2019_runtimes.exe
	ExecWait "$PLUGINSDIR\check_for_64bit_visual_studio_2019_runtimes.exe" $R0
	Delete "$PLUGINSDIR\check_for_64bit_visual_studio_2019_runtimes.exe"
	IntCmp $R0 126 vs2019Missing_64 vs2019OK_64
	vs2019Missing_64:
		MessageBox MB_YESNO|MB_ICONEXCLAMATION "Your system is missing runtime components that ${APPNAME} requires. Would you like to download them?" IDYES vs2019true_64 IDNO vs2019false_64
		vs2019true_64:
			ExecShell "open" "https://obsproject.com/visual-studio-2019-runtimes-64-bit"
		vs2019false_64:
		Quit
	vs2019OK_64:
	ClearErrors
!else
	; 32 bit Visual Studio 2019 runtime check
	ClearErrors
	GetDLLVersion "vcruntime140.DLL" $R0 $R1
	GetDLLVersion "msvcp140.DLL" $R0 $R1
	GetDLLVersion "msvcp140_1.DLL" $R0 $R1
	IfErrors vs2019Missing_32 vs2019OK_32
	vs2019Missing_32:
		MessageBox MB_YESNO|MB_ICONEXCLAMATION "Your system is missing runtime components that ${APPNAME} requires. Would you like to download them?" IDYES vs2019true_32 IDNO vs2019false_32
		vs2019true_32:
			ExecShell "open" "https://obsproject.com/visual-studio-2019-runtimes-32-bit"
		vs2019false_32:
		Quit
	vs2019OK_32:
	ClearErrors
!endif

	; DirectX Version Check
	ClearErrors
	GetDLLVersion "D3DCompiler_33.dll" $R0 $R1
	IfErrors dxMissing33 dxOK
	dxMissing33:
	ClearErrors
	GetDLLVersion "D3DCompiler_34.dll" $R0 $R1
	IfErrors dxMissing34 dxOK
	dxMissing34:
	ClearErrors
	GetDLLVersion "D3DCompiler_35.dll" $R0 $R1
	IfErrors dxMissing35 dxOK
	dxMissing35:
	ClearErrors
	GetDLLVersion "D3DCompiler_36.dll" $R0 $R1
	IfErrors dxMissing36 dxOK
	dxMissing36:
	ClearErrors
	GetDLLVersion "D3DCompiler_37.dll" $R0 $R1
	IfErrors dxMissing37 dxOK
	dxMissing37:
	ClearErrors
	GetDLLVersion "D3DCompiler_38.dll" $R0 $R1
	IfErrors dxMissing38 dxOK
	dxMissing38:
	ClearErrors
	GetDLLVersion "D3DCompiler_39.dll" $R0 $R1
	IfErrors dxMissing39 dxOK
	dxMissing39:
	ClearErrors
	GetDLLVersion "D3DCompiler_40.dll" $R0 $R1
	IfErrors dxMissing40 dxOK
	dxMissing40:
	ClearErrors
	GetDLLVersion "D3DCompiler_41.dll" $R0 $R1
	IfErrors dxMissing41 dxOK
	dxMissing41:
	ClearErrors
	GetDLLVersion "D3DCompiler_42.dll" $R0 $R1
	IfErrors dxMissing42 dxOK
	dxMissing42:
	ClearErrors
	GetDLLVersion "D3DCompiler_43.dll" $R0 $R1
	IfErrors dxMissing43 dxOK
	dxMissing43:
	ClearErrors
	GetDLLVersion "D3DCompiler_47.dll" $R0 $R1
	IfErrors dxMissing47 dxOK
	dxMissing47:
	ClearErrors
	GetDLLVersion "D3DCompiler_49.dll" $R0 $R1
	IfErrors dxMissing49 dxOK
	dxMissing49:
	MessageBox MB_YESNO|MB_ICONEXCLAMATION "Your system is missing DirectX components that ${APPNAME} requires. Would you like to download them?" IDYES dxtrue IDNO dxfalse
	dxtrue:
		ExecShell "open" "https://obsproject.com/go/dxwebsetup"
	dxfalse:
	Quit
	dxOK:
	ClearErrors

	; Check previous instance
	check32BitRunning:
	OBSInstallerUtils::IsProcessRunning "obs32.exe"
	IntCmp $R0 1 0 notRunning1
		MessageBox MB_RETRYCANCEL|MB_ICONEXCLAMATION "${APPNAME} is already running. Please close it first before installing a new version." /SD IDCANCEL IDRETRY check32BitRunning
		Quit
	notRunning1:

	${if} ${RunningX64}
		check64BitRunning:
		OBSInstallerUtils::IsProcessRunning "obs64.exe"
		IntCmp $R0 1 0 notRunning2
			MessageBox MB_RETRYCANCEL|MB_ICONEXCLAMATION "${APPNAME} is already running. Please close it first before installing a new version." /SD IDCANCEL IDRETRY check64BitRunning
			Quit
		notRunning2:
	${endif}
FunctionEnd

Var dllFilesInUse

Function checkDLLs
	OBSInstallerUtils::ResetInUseFileChecks
!ifdef INSTALL64
	OBSInstallerUtils::AddInUseFileCheck "$INSTDIR\bin\64bit\avutil-56.dll"
	OBSInstallerUtils::AddInUseFileCheck "$INSTDIR\bin\64bit\swscale-5.dll"
!else
	OBSInstallerUtils::AddInUseFileCheck "$INSTDIR\bin\32bit\avutil-56.dll"
	OBSInstallerUtils::AddInUseFileCheck "$INSTDIR\bin\32bit\swscale-5.dll"
!endif
	OBSInstallerUtils::AddInUseFileCheck "$INSTDIR\data\obs-plugins\win-capture\graphics-hook32.dll"
	OBSInstallerUtils::AddInUseFileCheck "$INSTDIR\data\obs-plugins\win-capture\graphics-hook64.dll"
	OBSInstallerUtils::AddInUseFileCheck "$INSTDIR\data\obs-plugins\win-dshow\obs-virtualcam-module32.dll"
	OBSInstallerUtils::AddInUseFileCheck "$INSTDIR\data\obs-plugins\win-dshow\obs-virtualcam-module64.dll"
	OBSInstallerUtils::AddInUseFileCheck "$APPDATA\obs-studio-hook\graphics-hook32.dll"
	OBSInstallerUtils::AddInUseFileCheck "$APPDATA\obs-studio-hook\graphics-hook64.dll"
	OBSInstallerUtils::GetAppNameForInUseFiles
	StrCpy $dllFilesInUse "$R0"
FunctionEnd

Function checkFilesInUse
	retryFileChecks:
	Call checkDLLs
	StrCmp $dllFilesInUse "" dllsNotInUse
	MessageBox MB_RETRYCANCEL|MB_ICONEXCLAMATION "OBS files are being used by the following applications:$\r$\n$\r$\n$dllFilesInUse$\r$\nPlease close these applications to continue setup." /SD IDCANCEL IDRETRY retryFileChecks
	Quit

	dllsNotInUse:
FunctionEnd

Function LaunchOBS
!ifdef INSTALL64
	Exec '"$WINDIR\explorer.exe" "$SMPROGRAMS\OBS Studio\OBS Studio (64bit).lnk"'
!else
	Exec '"$WINDIR\explorer.exe" "$SMPROGRAMS\OBS Studio\OBS Studio (32bit).lnk"'
!endif
FunctionEnd

Section "OBS Studio" SecCore
	SetShellVarContext all

	Call checkFilesInUse

	; Set Section properties
	SectionIn RO
	SetOverwrite on
	AllowSkipFiles off

	; Set Section Files and Shortcuts
	SetOutPath "$INSTDIR"
	OBSInstallerUtils::KillProcess "obs-plugins\32bit\cef-bootstrap.exe"
	OBSInstallerUtils::KillProcess "obs-plugins\64bit\cef-bootstrap.exe"

	File /r "new\core\data"

!ifdef INSTALL64
	SetOutPath "$INSTDIR\bin"
	File /r "new\core\bin\64bit"
	SetOutPath "$INSTDIR\obs-plugins"
	File /r "new\core\obs-plugins\64bit"
!else
	SetOutPath "$INSTDIR\bin"
	File /r "new\core\bin\32bit"
	SetOutPath "$INSTDIR\obs-plugins"
	File /r "new\core\obs-plugins\32bit"
!endif

	# ----------------------------

	SetShellVarContext all

	SetOutPath "$INSTDIR"
	File /r "new\obs-browser\data"
	SetOutPath "$INSTDIR\obs-plugins"
	OBSInstallerUtils::KillProcess "32bit\cef-bootstrap.exe"
	OBSInstallerUtils::KillProcess "32bit\obs-browser-page.exe"
	${if} ${RunningX64}
		OBSInstallerUtils::KillProcess "64bit\cef-bootstrap.exe"
		OBSInstallerUtils::KillProcess "64bit\obs-browser-page.exe"
	${endif}
!ifdef INSTALL64
	File /r "new\obs-browser\obs-plugins\64bit"
	SetOutPath "$INSTDIR\bin\64bit"
!else
	File /r "new\obs-browser\obs-plugins\32bit"
	SetOutPath "$INSTDIR\bin\32bit"
!endif

	# ----------------------------
	# Copy game capture files to ProgramData
	SetOutPath "$APPDATA\obs-studio-hook"
	File "new\core\data\obs-plugins\win-capture\graphics-hook32.dll"
	File "new\core\data\obs-plugins\win-capture\graphics-hook64.dll"
	File "new\core\data\obs-plugins\win-capture\obs-vulkan32.json"
	File "new\core\data\obs-plugins\win-capture\obs-vulkan64.json"
	OBSInstallerUtils::AddAllApplicationPackages "$APPDATA\obs-studio-hook"

	WriteUninstaller "$INSTDIR\uninstall.exe"

!ifdef INSTALL64
	SetOutPath "$INSTDIR\bin\64bit"
	CreateShortCut "$DESKTOP\OBS Studio.lnk" "$INSTDIR\bin\64bit\obs64.exe"
!else
	SetOutPath "$INSTDIR\bin\32bit"
	CreateShortCut "$DESKTOP\OBS Studio.lnk" "$INSTDIR\bin\32bit\obs32.exe"
!endif

	CreateDirectory "$SMPROGRAMS\OBS Studio"

!ifdef INSTALL64
	SetOutPath "$INSTDIR\bin\64bit"
	CreateShortCut "$SMPROGRAMS\OBS Studio\OBS Studio (64bit).lnk" "$INSTDIR\bin\64bit\obs64.exe"
!else
	SetOutPath "$INSTDIR\bin\32bit"
	CreateDirectory "$SMPROGRAMS\OBS Studio"
	CreateShortCut "$SMPROGRAMS\OBS Studio\OBS Studio (32bit).lnk" "$INSTDIR\bin\32bit\obs32.exe"
!endif

	CreateShortCut "$SMPROGRAMS\OBS Studio\Uninstall.lnk" "$INSTDIR\uninstall.exe"
SectionEnd

Section -FinishSection

	SetShellVarContext all

	# ---------------------------------------
	# 64bit vulkan hook registry stuff

	${if} ${RunningX64}
		SetRegView 64
		WriteRegStr HKLM "Software\${APPNAME}" "" "$INSTDIR"

		ClearErrors
		DeleteRegValue HKCU "Software\Khronos\Vulkan\ImplicitLayers" "$APPDATA\obs-studio-hook\obs-vulkan64.json"
		ClearErrors
		WriteRegDWORD HKLM "Software\Khronos\Vulkan\ImplicitLayers" "$APPDATA\obs-studio-hook\obs-vulkan64.json" 0
	${endif}

	# ---------------------------------------
	# 32bit vulkan hook registry stuff

	SetRegView 32
	WriteRegStr HKLM "Software\${APPNAME}" "" "$INSTDIR"

	ClearErrors
	DeleteRegValue HKCU "Software\Khronos\Vulkan\ImplicitLayers" "$APPDATA\obs-studio-hook\obs-vulkan32.json"
	ClearErrors
	WriteRegDWORD HKLM "Software\Khronos\Vulkan\ImplicitLayers" "$APPDATA\obs-studio-hook\obs-vulkan32.json" 0

	# ---------------------------------------
	# Register virtual camera dlls

	Exec '"$SYSDIR\regsvr32.exe" /s "$INSTDIR\data\obs-plugins\win-dshow\obs-virtualcam-module32.dll"'
	${if} ${RunningX64}
		Exec '"$SYSDIR\regsvr32.exe" /s "$INSTDIR\data\obs-plugins\win-dshow\obs-virtualcam-module64.dll"'
	${endif}

	# ---------------------------------------

	ClearErrors
	SetRegView default

	WriteRegStr HKLM "Software\${APPNAME}" "" "$INSTDIR"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "DisplayName" "${APPNAME}"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "UninstallString" "$INSTDIR\uninstall.exe"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "ProductID" "d16d2409-3151-4331-a9b1-dfd8cf3f0d9c"
!ifdef INSTALL64
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "DisplayIcon" "$INSTDIR\bin\64bit\obs64.exe"
!else
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "DisplayIcon" "$INSTDIR\bin\32bit\obs32.exe"
!endif
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "Publisher" "OBS Project"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "HelpLink" "https://obsproject.com"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "DisplayVersion" "${APPVERSION}"

SectionEnd

; Modern install component descriptions
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
	!insertmacro MUI_DESCRIPTION_TEXT ${SecCore} "Core OBS Studio files"
!insertmacro MUI_FUNCTION_DESCRIPTION_END

;Uninstall section
Section "un.obs-studio Program Files" UninstallSection1

	SectionIn RO

	; Remove hook files and vulkan registry
	SetShellVarContext all

	RMDir /r "$APPDATA\obs-studio-hook"

	SetRegView 32
	DeleteRegValue HKCU "Software\Khronos\Vulkan\ImplicitLayers" "$APPDATA\obs-studio-hook\obs-vulkan32.json"
	DeleteRegValue HKLM "Software\Khronos\Vulkan\ImplicitLayers" "$APPDATA\obs-studio-hook\obs-vulkan32.json"
	${if} ${RunningX64}
		SetRegView 64
		DeleteRegValue HKCU "Software\Khronos\Vulkan\ImplicitLayers" "$APPDATA\obs-studio-hook\obs-vulkan64.json"
		DeleteRegValue HKLM "Software\Khronos\Vulkan\ImplicitLayers" "$APPDATA\obs-studio-hook\obs-vulkan64.json"
	${endif}
	SetRegView default
	SetShellVarContext current
	ClearErrors

	; Unregister virtual camera dlls
	Exec '"$SYSDIR\regsvr32.exe" /u /s "$INSTDIR\data\obs-plugins\win-dshow\obs-virtualcam-module32.dll"'
	${if} ${RunningX64}
		Exec '"$SYSDIR\regsvr32.exe" /u /s "$INSTDIR\data\obs-plugins\win-dshow\obs-virtualcam-module64.dll"'
	${endif}

	; Remove from registry...
	DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}"
	DeleteRegKey HKLM "SOFTWARE\${APPNAME}"

	; Delete self
	Delete "$INSTDIR\uninstall.exe"

	; Delete Shortcuts
	Delete "$DESKTOP\OBS Studio.lnk"
	Delete "$SMPROGRAMS\OBS Studio\OBS Studio (32bit).lnk"
	Delete "$SMPROGRAMS\OBS Studio\Uninstall.lnk"
	${if} ${RunningX64}
		Delete "$SMPROGRAMS\OBS Studio\OBS Studio (64bit).lnk"
	${endif}

	IfFileExists "$INSTDIR\data\obs-plugins\win-ivcam\seg_service.exe" UnregisterSegService SkipUnreg
	UnregisterSegService:
	ExecWait '"$INSTDIR\data\obs-plugins\win-ivcam\seg_service.exe" /UnregServer'
	SkipUnreg:

	; Clean up OBS Studio
	RMDir /r "$INSTDIR\bin"
	RMDir /r "$INSTDIR\data"
	RMDir /r "$INSTDIR\obs-plugins"
	RMDir "$INSTDIR"

	; Remove remaining directories
	RMDir "$SMPROGRAMS\OBS Studio"
	RMDir "$INSTDIR\OBS Studio"
SectionEnd

Section /o "un.User Settings" UninstallSection2
	RMDir /r "$APPDATA\obs-studio"
SectionEnd

!insertmacro MUI_UNFUNCTION_DESCRIPTION_BEGIN
	!insertmacro MUI_DESCRIPTION_TEXT ${UninstallSection1} "Remove the OBS program files."
	!insertmacro MUI_DESCRIPTION_TEXT ${UninstallSection2} "Removes all settings, scenes and sources, profiles, log files and other application data."
!insertmacro MUI_UNFUNCTION_DESCRIPTION_END

; Version information
VIProductVersion "${APPVERSION}.0"
VIAddVersionKey /LANG=${LANG_ENGLISH} "ProductName" "OBS Studio"
VIAddVersionKey /LANG=${LANG_ENGLISH} "CompanyName" "obsproject.com"
VIAddVersionKey /LANG=${LANG_ENGLISH} "LegalCopyright" "(c) 2012-2020"
; FileDescription is what shows in the UAC elevation prompt when signed
VIAddVersionKey /LANG=${LANG_ENGLISH} "FileDescription" "OBS Studio"
VIAddVersionKey /LANG=${LANG_ENGLISH} "FileVersion" "1.0"

; eof
