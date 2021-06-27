Param(
    [Switch]$Help,
    [Switch]$Quiet,
    [Switch]$Verbose,
    [Switch]$NoChoco,
    [Switch]$Package,
    [Switch]$SkipDependencyChecks,
    [Switch]$BuildInstaller,
    [Switch]$CombinedArchs,
    [String]$BuildDirectory = "build",
    [String]$BuildArch = (Get-CimInstance CIM_OperatingSystem).OSArchitecture,
    [String]$BuildConfiguration = "RelWithDebInfo"
)

##############################################################################
# Windows OBS build script
##############################################################################
#
# This script contains all steps necessary to:
#
#   * Build OBS with all required dependencies
#   * Create 64-bit and 32-bit variants
#
# Parameters:
#   -Help                   : Print usage help
#   -Quiet                  : Suppress most build process output
#   -Verbose                : Enable more verbose build process output
#   -SkipDependencyChecks   : Skip dependency checks
#   -NoChoco                : Skip automatic dependency installation
#                             via Chocolatey
#   -BuildDirectory         : Directory to use for builds
#                             Default: Win64 on 64-bit systems
#                                      Win32 on 32-bit systems
#   -BuildArch              : Build architecture to use (32bit or 64bit)
#   -BuildConfiguration     : Build configuration to use
#                             Default: RelWithDebInfo
#   -CombinedArchs          : Create combined packages and installer
#                             (64-bit and 32-bit) - Default: off"
#   -Package                : Prepare folder structure for installer creation
#
# Environment Variables (optional):
#  WindowsDepsVersion       : Pre-compiled Windows dependencies version
#  WindowsQtVersion         : Pre-compiled Qt version
#
##############################################################################

$ErrorActionPreference = "Stop"

$_RunObsBuildScript = $true
$ProductName = "OBS-Studio"

$CheckoutDir = git rev-parse --show-toplevel

$DepsBuildDir = "${CheckoutDir}/../obs-build-dependencies"
$ObsBuildDir = "${CheckoutDir}/../obs-studio"

. ${CheckoutDir}/CI/include/build_support_windows.ps1

## DEPENDENCY INSTALLATION ##
. ${CheckoutDir}/CI/windows/01_install_dependencies.ps1

## OBS LIBRARY BUILD ##
. ${CheckoutDir}/CI/windows/02_build_obs.ps1

## PLUGIN PACKAGE AND NOTARIZE ##
. ${CheckoutDir}/CI/windows/03_package_obs.ps1

function Build-OBS-Main {
    Ensure-Directory ${CheckoutDir}
    Write-Step "Fetching version tags..."
    & git fetch origin --tags
    $GitBranch = git rev-parse --abbrev-ref HEAD
    $GitHash = git rev-parse --short HEAD
    $ErrorActionPreference = "SilentlyContiue"
    $GitTag = git describe --tags --abbrev=0
    $ErrorActionPreference = "Stop"

    if(Test-Path variable:BUILD_FOR_DISTRIBUTION) {
        $VersionString = "${GitTag}"
    } else {
        $VersionString = "${GitTag}-${GitHash}"
    }

    $FileName = "${ProductName}-${VersionString}"

    if($CombinedArchs.isPresent) {
        if (!(Test-Path env:obsInstallerTempDir)) {
            $Env:obsInstallerTempDir = "${CheckoutDir}/install_temp"
        }

        if(!($SkipDependencyChecks.isPresent)) {
            Install-Dependencies -NoChoco:${NoChoco} -BuildArch 64-bit
        }

        Build-OBS -BuildArch 64-bit

        if(!($SkipDependencyChecks.isPresent)) {
            Install-Dependencies -NoChoco:${NoChoco} -BuildArch 32-bit
        }

        Build-OBS -BuildArch 32-bit
    } else {
        if(!($SkipDependencyChecks.isPresent)) {
            Install-Dependencies -NoChoco:$NoChoco
        }

        Build-OBS
    }

    if($Package.isPresent) {
        Package-OBS -CombinedArchs:$CombinedArchs
    }

    Write-Info "All done!"
}

## MAIN SCRIPT FUNCTIONS ##
function Print-Usage {
    Write-Host "build-windows.ps1 - Build script for ${ProductName}"
    $Lines = @(
        "Usage: ${MyInvocation.MyCommand.Name}",
        "-Help                    : Print this help",
        "-Quiet                   : Suppress most build process output"
        "-Verbose                 : Enable more verbose build process output"
        "-SkipDependencyChecks    : Skip dependency checks - Default: off",
        "-NoChoco                 : Skip automatic dependency installation via Chocolatey - Default: on",
        "-BuildDirectory          : Directory to use for builds - Default: build64 on 64-bit systems, build32 on 32-bit systems",
        "-BuildArch               : Build architecture to use (32bit or 64bit) - Default: local architecture",
        "-BuildConfiguration      : Build configuration to use - Default: RelWithDebInfo",
        "-CombinedArchs           : Create combined packages and installer (64-bit and 32-bit) - Default: off"
        "-Package                 : Prepare folder structure for installer creation"
    )
    $Lines | Write-Host
}

if($Help.isPresent) {
    Print-Usage
    exit 0
}

Build-OBS-Main
