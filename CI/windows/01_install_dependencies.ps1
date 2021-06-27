Param(
    [Switch]$Help = $(if (Test-Path variable:Help) { $Help }),
    [Switch]$Quiet = $(if (Test-Path variable:Quiet) { $Quiet }),
    [Switch]$Verbose = $(if (Test-Path variable:Verbose) { $Verbose }),
    [Switch]$NoChoco = $(if (Test-Path variable:NoChoco) { $NoChoco }),
    [String]$BuildArch = $(if (Test-Path variable:BuildArch) { "${BuildArch}" } else { (Get-CimInstance CIM_OperatingSystem).OSArchitecture })
)

##############################################################################
# Windows dependency management function
##############################################################################
#
# This script file can be included in build scripts for Windows or run
# directly
#
##############################################################################

$ErrorActionPreference = "Stop"

Function Install-obs-deps {
    Param(
        [Parameter(Mandatory=$true)]
        [String]$Version
    )

    Write-Status "Setup for pre-built Windows OBS dependencies v${Version}"
    Ensure-Directory $DepsBuildDir

    if (!(Test-Path "$DepsBuildDir/dependencies${Version}")) {

        Write-Step "Download..."
        $ProgressPreference = $(if ($Quiet.isPresent) { "SilentlyContinue" } else { "Continue" })
        Invoke-WebRequest -Uri "https://cdn-fastly.obsproject.com/downloads/dependencies${Version}.zip" -UseBasicParsing -OutFile "dependencies${Version}.zip"
        $ProgressPreference = "Continue"

        Write-Step "Unpack..."

        Expand-Archive -Path "dependencies${Version}.zip"
    } else {
        Write-Step "Found existing pre-built dependencies..."
    }
}

function Install-qt-deps {
    Param(
        [Parameter(Mandatory=$true)]
        [String]$Version
    )

    Write-Status "Setup for pre-built dependency Qt v${Version}"
    Ensure-Directory $DepsBuildDir

    if (!(Test-Path "$DepsBuildDir/Qt_${Version}")) {

        Write-Step "Download..."
        $ProgressPreference = $(if ($Quiet.isPresent) { 'SilentlyContinue' } else { 'Continue' })
        Invoke-WebRequest -Uri "https://cdn-fastly.obsproject.com/downloads/Qt_${Version}.7z" -UseBasicParsing -OutFile "Qt_${Version}.7z"
        $ProgressPreference = "Continue"
        
        Write-Step "Unpack..."

        # TODO: Replace with zip and properly package Qt to share directory with other deps
        & 7z x "Qt_${Version}.7z"
        & mv ${Version} "Qt_${Version}"
    } else {
        Write-Step "Found existing pre-built Qt..."
    }
}

function Install-vlc {
    Param(
        [Parameter(Mandatory=$true)]
        [String]$Version
    )

    Write-Status "Setup for dependency VLC v${Version}"
    Ensure-Directory $DepsBuildDir

    if (!((Test-Path "$DepsBuildDir/vlc-${Version}") -and (Test-Path "$DepsBuildDir/vlc-${Version}/include/vlc/vlc.h"))) {
        Write-Step "Download..."
        $ProgressPreference = $(if ($Quiet.isPresent) { 'SilentlyContinue' } else { 'Continue' })
        Invoke-WebRequest -Uri https://cdn-fastly.obsproject.com/downloads/vlc.zip -UseBasicParsing -OutFile "vlc_${Version}.zip"
        $ProgressPreference = "Continue"

        Write-Step "Unpack..."
        # Expand-Archive -Path "vlc_${Version}.zip"
        & 7z x "vlc_${Version}.zip" -ovlc
        & mv vlc "vlc-${Version}"
    } else {
        Write-Step "Found existing VLC..."
    }
}

function Install-cef {
    Param(
        [Parameter(Mandatory=$true)]
        [String]$Version
    )
    Write-Status "Setup for dependency CEF v${Version} - ${BuildArch}"

    Ensure-Directory $DepsBuildDir
    $ArchSuffix = "$(if ($BuildArch -eq "64-bit") { "64" } else { "32" })"

    if (!((Test-Path "${DepsBuildDir}/cef_binary_${Version}_windows${ArchSuffix}_minimal") -and (Test-Path "${DepsBuildDir}/cef_binary_${Version}_windows${ArchSuffix}_minimal/build/libcef_dll_wrapper/Release/libcef_dll_wrapper.lib"))) {
        Write-Step "Download..."
        $ProgressPreference = $(if ($Quiet.isPresent) { 'SilentlyContinue' } else { 'Continue' })
        Invoke-WebRequest -Uri https://cdn-fastly.obsproject.com/downloads/cef_binary_${Version}_windows${ArchSuffix}_minimal.zip -UseBasicParsing -OutFile "cef_binary_${Version}_windows${ArchSuffix}_minimal.zip"
        $ProgressPreference = "Continue"

        Write-Step "Unpack..."
        Expand-Archive -Path "cef_binary_${Version}_windows${ArchSuffix}_minimal.zip" -DestinationPath .
    } else {
        Write-Step "Found existing CEF framework and loader library..."
    }
}

function Install-Dependencies {
    Param(
        [String]$BuildArch = $(if (Test-Path variable:BuildArch) { "${BuildArch}" })
    )

    if(!($NoChoco.isPresent)) {
        Install-Windows-Dependencies
    }

    $BuildDependencies = @(
        @('obs-deps', $WindowsDepsVersion),
        @('qt-deps', $WindowsQtVersion),
        @('vlc', $WindowsVlcVersion),
        @('cef', $WindowsCefVersion)
    )

    Foreach($Dependency in ${BuildDependencies}) {
        $DependencyName = $Dependency[0]
        $DependencyVersion = $Dependency[1]

        $FunctionName = "Install-${DependencyName}"
        & ${FunctionName} -Version ${DependencyVersion}
    }

    Ensure-Directory ${CheckoutDir}
}

function Install-Dependencies-Standalone {
    $ProductName = "OBS-Studio"
    $CheckoutDir = git rev-parse --show-toplevel

    $DepsBuildDir = "${CheckoutDir}/../obs-build-dependencies"
    $ObsBuildDir = "${CheckoutDir}/../obs-studio"

    . ${CheckoutDir}/CI/include/build_support_windows.ps1

    Write-Status "Setup of OBS build dependencies"
    Install-Dependencies
}

function Print-Usage {
    $Lines = @(
        "Usage: ${MyInvocation.MyCommand.Name}",
        "-Help                    : Print this help",
        "-Quiet                   : Suppress most build process output",
        "-Verbose                 : Enable more verbose build process output",
        "-NoChoco                 : Skip automatic dependency installation via Chocolatey - Default: on"
        "-BuildArch               : Build architecture to use (32-bit or 64-bit) - Default: local architecture"
    )

    $Lines | Write-Host
}

if(!(Test-Path variable:_RunObsBuildScript)) {
    if($Help.isPresent) {
        Print-Usage
        exit 0
    }

    Install-Dependencies-Standalone
}
