param(
    [switch]$PauseOnExit
)

$ErrorActionPreference = 'Stop'

$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectFile = Join-Path $ProjectRoot 'ProjectP.uproject'
$ServerRuntimeConfig = Join-Path $ProjectRoot 'ServerRuntime.ini'
$RunUAT = 'C:\Users\k2503200045\UnrealEngine\Engine\Build\BatchFiles\RunUAT.bat'
$ClientTarget = 'ProjectP'
$RunId = Get-Date -Format 'yyyyMMdd_HHmmss'
$ClientArchive = Join-Path $ProjectRoot 'Saved\Packaged\Client'
$LogRoot = Join-Path $ProjectRoot 'Saved\Logs'
$ClientLog = Join-Path $ProjectRoot "Saved\Logs\Package_Client_$RunId.out.log"
$ClientMaps = @(
    '/Game/LeDuat/Maps/Map_Login',
    '/Game/LeDuat/Maps/Map_Register',
    '/Game/LeDuat/Maps/Map_Lobby',
    '/Game/LeDuat/Maps/BuildMap/Map_Stage1'
)

function Write-Step {
    param([string]$Message)
    Write-Host ''
    Write-Host "==== $Message ===="
}

function Get-BuildProcessInfo {
    Get-CimInstance Win32_Process | Where-Object {
        $_.ProcessId -ne $PID -and (
            $_.CommandLine -like '*BuildCookRun*' -or
            $_.Name -eq 'UnrealEditor-Cmd.exe'
        )
    } | Select-Object ProcessId, Name, CommandLine
}

function Assert-RequiredPath {
    param(
        [string]$Path,
        [string]$Label
    )

    if (-not (Test-Path $Path)) {
        throw "$Label not found: $Path"
    }
}

function Copy-ServerRuntimeConfig {
    param([string]$DestinationRoot)

    if (-not (Test-Path $ServerRuntimeConfig)) {
        Write-Host "ServerRuntime.ini not found. Skipping runtime config copy: $ServerRuntimeConfig"
        return
    }

    if (-not (Test-Path $DestinationRoot)) {
        New-Item -ItemType Directory -Force -Path $DestinationRoot | Out-Null
    }

    $destinationPath = Join-Path $DestinationRoot 'ServerRuntime.ini'
    Copy-Item -LiteralPath $ServerRuntimeConfig -Destination $destinationPath -Force
    Write-Host "ServerRuntime.ini copied: $destinationPath"
}

function Invoke-ClientPackage {
    Write-Step 'Package Client'
    Write-Host "Archive: $ClientArchive"
    Write-Host "Log: $ClientLog"
    Write-Host "Maps: $($ClientMaps -join ', ')"

    $uatArgs = @(
        'BuildCookRun',
        "-project=$ProjectFile",
        '-noP4',
        '-platform=Win64',
        '-clientconfig=Development',
        "-target=$ClientTarget",
        '-build',
        '-cook',
        '-stage',
        '-pak',
        '-archive',
        "-archivedirectory=$ClientArchive",
        "-map=$($ClientMaps -join '+')",
        '-utf8output',
        '-unattended',
        '-ubtargs=-NoUBA -MaxParallelActions=4'
    )

    & $RunUAT @uatArgs *> $ClientLog
    $exitCode = $LASTEXITCODE

    Write-Host "RunUAT exit code: $exitCode"
    if (Test-Path $ClientLog) {
        Get-Content -Tail 40 -Encoding UTF8 $ClientLog
    }

    if ($exitCode -ne 0) {
        throw "Client packaging failed. See log: $ClientLog"
    }

    $logText = Get-Content -Raw -Encoding UTF8 $ClientLog
    if ($logText -notmatch 'BUILD SUCCESSFUL' -or $logText -notmatch 'ExitCode=0 \(Success\)') {
        throw "Client packaging finished but success markers were not found. See log: $ClientLog"
    }

    Copy-ServerRuntimeConfig -DestinationRoot (Join-Path $ClientArchive 'Windows')
}

function Invoke-Main {
    Write-Step 'Validate Paths'
    Assert-RequiredPath $ProjectFile 'Unreal project'
    Assert-RequiredPath $RunUAT 'RunUAT'
    Assert-RequiredPath (Join-Path $ProjectRoot 'Source\ProjectP.Target.cs') 'Client target'
    New-Item -ItemType Directory -Force -Path $LogRoot | Out-Null
    Write-Host "Run id: $RunId"

    $buildProcesses = @(Get-BuildProcessInfo)
    if ($buildProcesses.Count -gt 0) {
        $buildProcesses | Format-Table -AutoSize
        throw 'Existing build processes are still running.'
    }

    Invoke-ClientPackage

    Write-Step 'Validate Output'
    $clientExe = Get-ChildItem -LiteralPath $ClientArchive -Recurse -Filter 'ProjectP.exe' -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $clientExe) {
        throw "Client executable was not found under: $ClientArchive"
    }

    Write-Step 'Completed'
    Write-Host 'Client packaging: BUILD SUCCESSFUL'
    Write-Host "Client executable: $($clientExe.FullName)"
    Write-Host "Client log: $ClientLog"
}

try {
    Invoke-Main
    exit 0
}
catch {
    Write-Host ''
    Write-Host 'FAILED'
    Write-Host $_.Exception.Message
    exit 1
}
finally {
    if ($PauseOnExit) {
        Write-Host ''
        Read-Host 'Press Enter to close'
    }
}
