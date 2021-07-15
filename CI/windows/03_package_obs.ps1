Param(
    [Switch]$Help = $(if (Test-Path variable:Help) { $Help }),
    [Switch]$Quiet = $(if (Test-Path variable:Quiet) { $Quiet }),
    [Switch]$Verbose = $(if (Test-Path variable:Verbose) { $Verbose }),
    [Switch]$BuildInstaller = $(if ($BuildInstaller.isPresent) { $BuildInstaller }),
    [Switch]$CombinedArchs = $(if ($CombinedArchs.isPresent) { $CombinedArchs }),
    [String]$BuildDirectory = $(if (Test-Path variable:BuildDirectory) { "${BuildDirectory}" } else { "build" }),
    [String]$BuildArch = $(if (Test-Path variable:BuildArch) { "${BuildArch}" } else { (Get-CimInstance CIM_OperatingSystem).OSArchitecture }),
    [String]$BuildConfiguration = $(if (Test-Path variable:BuildConfiguration) { "${BuildConfiguration}" } else { "RelWithDebInfo" })
)

##############################################################################
# Windows OBS package function
##############################################################################
#
# This script file can be included in build scripts for Windows or run
# directly
#
##############################################################################

$ErrorActionPreference = "Stop"

function Package-OBS {
    Param(
        [String]$BuildDirectory = $(if (Test-Path variable:BuildDirectory) { "${BuildDirectory}" }),
        [String]$BuildArch = $(if (Test-Path variable:BuildArch) { "${BuildArch}" }),
        [String]$BuildConfiguration = $(if (Test-Path variable:BuildConfiguration) { "${BuildConfiguration}" })
    )

    Write-Status "Package plugin ${ProductName}"
    Ensure-Directory ${CheckoutDir}

    if ($CombinedArchs.isPresent) {
        if (!(Test-Path env:obsInstallerTempDir)) {
            $Env:obsInstallerTempDir = "${CheckoutDir}/install_temp"
        }

        if (!(Test-Path ${CheckoutDir}/install_temp/bin/64bit)) {
            Write-Step "Build 64-bit OBS..."
            cmake -S . -B "${BuildDirectory}64" -DCOPIED_DEPENDENCIES=OFF -DCOPY_DEPENDENCIES=ON

            cmake --build "${BuildDirectory}64" --config "${BuildConfiguration}"
        }

        if (!(Test-Path ${CheckoutDir}/install_temp/bin/32bit)) {
            Write-Step "Build 32-bit OBS..."
            cmake -S . -B "${BuildDirectory}32" -DCOPIED_DEPENDENCIES=OFF -DCOPY_DEPENDENCIES=ON
            cmake --build "${BuildDirectory}32" --config "${BuildConfiguration}"
        }

        Write-Step "Prepare Installer run..."
        cmake -S . -B build -DINSTALLER_RUN=ON -DCMAKE_INSTALL_PREFIX="${CheckoutDir}/build/install"
        Write-Step "Execute Installer run..."
        cmake --build build --config "${BuildConfiguration}" -t install

        $CompressVars = @{
            Path = "${CheckoutDir}/build/install/*"
            CompressionLevel = "Optimal"
            DestinationPath = "${FileName}-Windows.zip"
        }

        Write-Step "Creating zip archive..."

        $ProgressPreference = $(if ($Quiet.isPresent) { 'SilentlyContinue' } else { 'Continue' })
        Compress-Archive -Force @CompressVars
        $ProgressPreference = 'Continue'

    } elseif ($BuildArch -eq "64-bit") {
        Write-Step "Install 64-bit OBS..."
        cmake --build ${BuildDirectory}64 --config ${BuildConfiguration} -t install

        $CompressVars = @{
            Path = "${CheckoutDir}/build64/install/bin", "${CheckoutDir}/build64/install/data", "${CheckoutDir}/build64/install/obs-plugins"
            CompressionLevel = "Optimal"
            DestinationPath = "${FileName}-Win64.zip"
        }

        Write-Step "Creating zip archive..."

        $ProgressPreference = $(if ($Quiet.isPresent) { 'SilentlyContinue' } else { 'Continue' })
        Compress-Archive -Force @CompressVars
        $ProgressPreference = 'Continue'

    } elseif ($BuildArch -eq "32-bit") {
        Write-Step "Install 32-bit OBS..."
        cmake --build ${BuildDirectory}32 --config ${BuildConfiguration} -t install

        $CompressVars = @{
            Path = "${CheckoutDir}/build32/install/bin", "${CheckoutDir}/build32/install/data", "${CheckoutDir}/build32/install/obs-plugins"
            CompressionLevel = "Optimal"
            DestinationPath = "${FileName}-Win32.zip"
        }

        Write-Step "Creating zip archive..."

        $ProgressPreference = $(if ($Quiet.isPresent) { 'SilentlyContinue' } else { 'Continue' })
        Compress-Archive -Force @CompressVars
        $ProgressPreference = 'Continue'

    }
}

function Package-OBS-Standalone {
    $ProductName = "OBS-Studio"
    $CheckoutDir = git rev-parse --show-toplevel

    . ${CheckoutDir}/CI/include/build_support_windows.ps1

    Write-Step "Fetch OBS tags..."
    $null = git fetch origin --tags

    Ensure-Directory ${CheckoutDir}
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

    Package-OBS
}

function Print-Usage {
    $Lines = @(
        "Usage: ${MyInvocation.MyCommand.Name}",
        "-Help                    : Print this help",
        "-Quiet                   : Suppress most build process output",
        "-Verbose                 : Enable more verbose build process output",
        "-CombinedArchs           : Create combined architecture package",
        "-BuildDirectory          : Directory to use for builds - Default: build64 on 64-bit systems, build32 on 32-bit systems",
        "-BuildArch               : Build architecture to use (32-bit or 64-bit) - Default: local architecture",
        "-BuildConfiguration      : Build configuration to use - Default: RelWithDebInfo"
    )

    $Lines | Write-Host
}


if(!(Test-Path variable:_RunObsBuildScript)) {
    if($Help.isPresent) {
        Print-Usage
        exit 0
    }

    Package-OBS-Standalone
}
