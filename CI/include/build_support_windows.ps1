$CIWorkflow = "${CheckoutDir}/.github/workflows/main.yml"

$CIDepsVersion = Get-Content ${CIWorkflow} | Select-String "[ ]+DEPS_VERSION_WIN: '([0-9\-]+)'" | ForEach-Object{$_.Matches.Groups[1].Value}
$CIQtVersion = Get-Content ${CIWorkflow} | Select-String "[ ]+QT_VERSION_WIN: '([0-9\.]+)'" | ForEach-Object{$_.Matches.Groups[1].Value}
$CIVlcVersion = Get-Content ${CIWorkflow} | Select-String "[ ]+VLC_VERSION_WIN: '(.+)'" | ForEach-Object{$_.Matches.Groups[1].Value}
$CICefVersion = Get-Content ${CIWorkflow} | Select-String "[ ]+CEF_BUILD_VERSION_WIN: '([0-9\.]+)'" | ForEach-Object{$_.Matches.Groups[1].Value}

function Write-Status {
    Param(
        [Parameter(Mandatory=$true)]
        [String] $output
    )

    if (!($Quiet.isPresent)) {
        if (Test-Path Env:CI) {
            Write-Host "[${ProductName}] ${output}"
        } else {
            Write-Host -ForegroundColor blue "[${ProductName}] ${output}"
        }
    }
}

function Write-Info {
    Param(
        [Parameter(Mandatory=$true)]
        [String] $output
    )

    if (!($Quiet.isPresent)) {
        if (Test-Path Env:CI) {
            Write-Host " + ${output}"
        } else {
            Write-Host -ForegroundColor DarkYellow " + ${output}"
        }
    }
}

function Write-Step {
    Param(
        [Parameter(Mandatory=$true)]
        [String] $output
    )

    if (!($Quiet.isPresent)) {
        if (Test-Path Env:CI) {
            Write-Host " + ${output}"
        } else {
            Write-Host -ForegroundColor green " + ${output}"
        }
    }
}

function Write-Error {
    Param(
        [Parameter(Mandatory=$true)]
        [String] $output
    )

    if (Test-Path Env:CI) {
        Write-Host " + ${output}"
    } else {
        Write-Host -ForegroundColor red " + ${output}"
    }
}

function Test-CommandExists {
    Param(
        [Parameter(Mandatory=$true)]
        [String] $Command
    )

    $CommandExists = $false
    $OldActionPref = $ErrorActionPreference
    $ErrorActionPreference = "stop"

    try {
        if (Get-Command $Command) {
            $CommandExists = $true
        }
    } Catch {
        $CommandExists = $false
    } Finally {
        $ErrorActionPreference = $OldActionPref
    }

    return $CommandExists
}

function Ensure-Directory {
    Param(
        [Parameter(Mandatory=$true)]
        [String] $Directory
    )

    if (!(Test-Path $Directory)) {
        $null = New-Item -ItemType Directory -Force -Path $Directory
    }

    Set-Location -Path $Directory
}

$BuildDirectory = "$(if (Test-Path Env:BuildDirectory) { $env:BuildDirectory } else { $BuildDirectory })"
$BuildConfiguration = "$(if (Test-Path Env:BuildConfiguration) { $env:BuildConfiguration } else { $BuildConfiguration })"
$BuildArch = "$(if (Test-Path Env:BuildArch) { $env:BuildArch } else { $BuildArch })"
$WindowsDepsVersion = "$(if (Test-Path Env:WindowsDepsVersion ) { $env:WindowsDepsVersion } else { $CIDepsVersion })"
$WindowsQtVersion = "$(if (Test-Path Env:WindowsQtVersion ) { $env:WindowsQtVersion } else { $CIQtVersion })"
$WindowsVlcVersion = "$(if (Test-Path Env:WindowsVlcVersion ) { $env:WindowsVlcVersion } else { $CIVlcVersion })"
$WindowsCefVersion = "$(if (Test-Path Env:WindowsCefVersion ) { $env:WindowsCefVersion } else { $CICefVersion })"
$CmakeSystemVersion = "$(if (Test-Path Env:CMAKE_SYSTEM_VERSION) { $Env:CMAKE_SYSTEM_VERSION } else { "10.0.18363.657" })"

function Install-Windows-Dependencies {
    Write-Status "Check Windows build dependencies"

    $ObsBuildDependencies = @(
        @("7z", "7zip"),
        @("cmake", "cmake --install-arguments 'ADD_CMAKE_TO_PATH=System'")
    )

    if(!(Test-CommandExists "choco")) {
        Write-Step "Install Chocolatey..."
        Set-ExecutionPolicy Bypass -Scope Process -Force; [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072; iex ((New-Object System.Net.WebClient).DownloadString('https://chocolatey.org/install.ps1'))
    }

    Foreach($Dependency in $ObsBuildDependencies) {
        if($Dependency -is [system.array]) {
            $Command = $Dependency[0]
            $ChocoName = $Dependency[1]
        } else {
            $Command = $Dependency
            $ChocoName = $Dependency
        }

        if(!(Test-CommandExists "${Command}")) {
            Write-Step "Install dependency ${ChocoName}..."
            Invoke-Expression "choco install -y ${ChocoName}"
        }
    }

    $Env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path", "User")
}
