@echo off
@cd /d "%~dp0"
goto checkAdmin

:checkAdmin
	net session >nul 2>&1
	if %errorLevel% == 0 (
		echo.
	) else (
		echo Administrative rights are required, please re-run this script as Administrator.
		goto end
	)

:uninstallDLLs
	if exist "%~dp0\data\obs-plugins\win-dshow\obs-virtualcam-module32.dll" (
		regsvr32.exe /u /s "%~dp0\data\obs-plugins\win-dshow\obs-virtualcam-module32.dll"
	) else (
		regsvr32.exe /u /s obs-virtualcam-module32.dll
	)
	if exist "%~dp0\data\obs-plugins\win-dshow\obs-virtualcam-module64.dll" (
		regsvr32.exe /u /s "%~dp0\data\obs-plugins\win-dshow\obs-virtualcam-module64.dll"
	) else (
		regsvr32.exe /u /s obs-virtualcam-module64.dll
	)

:endSuccess
	echo Virtual Cam uninstalled!
	echo.

:end
	pause
	exit
