param(
    [switch]$PauseOnExit
)

$ErrorActionPreference = 'Stop'

$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectFile = Join-Path $ProjectRoot 'ProjectP.uproject'
$ServerRuntimeConfig = Join-Path $ProjectRoot 'ServerRuntime.ini'
$RunbookFile = Join-Path $ProjectRoot 'AI_Docs\ServerBuildPackagingRunbook.md'
$RunUAT = 'C:\Users\k2503200045\UnrealEngine\Engine\Build\BatchFiles\RunUAT.bat'
$ServerTarget = 'ProjectPServer'
$RunId = Get-Date -Format 'yyyyMMdd_HHmmss'
$LogRoot = Join-Path $ProjectRoot 'Saved\Logs'
$LobbyLog = Join-Path $ProjectRoot "Saved\Logs\Package_LobbyServer_$RunId.out.log"
$DungeonLog = Join-Path $ProjectRoot "Saved\Logs\Package_DungeonServer_$RunId.out.log"

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

function Invoke-ServerPackage {
    param(
        [string]$Name,
        [string]$Map,
        [string]$Archive,
        [string]$LogPath
    )

    Write-Step "Package $Name"
    Write-Host "Map: $Map"
    Write-Host "Archive: $Archive"
    Write-Host "Log: $LogPath"

    $uatArgs = @(
        'BuildCookRun',
        "-project=$ProjectFile",
        '-noP4',
        '-server',
        '-noclient',
        '-serverplatform=Win64',
        '-serverconfig=Development',
        "-target=$ServerTarget",
        '-build',
        '-cook',
        '-stage',
        '-pak',
        '-archive',
        "-archivedirectory=$Archive",
        "-map=$Map",
        '-utf8output',
        '-unattended',
        '-ubtargs=-NoUBA -MaxParallelActions=4'
    )

    & $RunUAT @uatArgs *> $LogPath
    $exitCode = $LASTEXITCODE

    Write-Host "RunUAT exit code: $exitCode"
    if (Test-Path $LogPath) {
        Get-Content -Tail 40 -Encoding UTF8 $LogPath
    }

    if ($exitCode -ne 0) {
        throw "$Name packaging failed. See log: $LogPath"
    }

    $logText = Get-Content -Raw -Encoding UTF8 $LogPath
    if ($logText -notmatch 'BUILD SUCCESSFUL' -or $logText -notmatch 'ExitCode=0 \(Success\)') {
        throw "$Name packaging finished but success markers were not found. See log: $LogPath"
    }

    Copy-ServerRuntimeConfig -DestinationRoot (Join-Path $Archive 'WindowsServer')
}

function Invoke-Main {
    Write-Step 'Validate Paths'
    Assert-RequiredPath $RunbookFile 'Runbook'
    Assert-RequiredPath $ProjectFile 'Unreal project'
    Assert-RequiredPath $RunUAT 'RunUAT'
    Assert-RequiredPath (Join-Path $ProjectRoot 'Source\ProjectPServer.Target.cs') 'Server target'
    New-Item -ItemType Directory -Force -Path $LogRoot | Out-Null
    Write-Host "Run id: $RunId"
    Write-Host "Runbook: $RunbookFile"

    $buildProcesses = @(Get-BuildProcessInfo)
    if ($buildProcesses.Count -gt 0) {
        $buildProcesses | Format-Table -AutoSize
        throw 'Existing build processes are still running.'
    }

    Invoke-ServerPackage `
        -Name 'LobbyServer' `
        -Map '/Game/LeDuat/Maps/Map_Lobby' `
        -Archive (Join-Path $ProjectRoot 'Saved\Packaged\LobbyServer') `
        -LogPath $LobbyLog

    Invoke-ServerPackage `
        -Name 'DungeonServer' `
        -Map '/Game/LeDuat/Maps/BuildMap/Map_Stage1' `
        -Archive (Join-Path $ProjectRoot 'Saved\Packaged\DungeonServer') `
        -LogPath $DungeonLog

    Write-Step 'Completed'
    Write-Host 'Lobby packaging: BUILD SUCCESSFUL'
    Write-Host 'Dungeon packaging: BUILD SUCCESSFUL'
    Write-Host "Lobby log: $LobbyLog"
    Write-Host "Dungeon log: $DungeonLog"
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
