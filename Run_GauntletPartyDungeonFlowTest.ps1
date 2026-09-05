param(
    [switch]$DryRun,
    [switch]$ValidateOnly,
    [switch]$PauseOnExit,
    [string]$RunUAT = "",
    [string]$ClientBuildRoot = "",
    [string]$CredentialsFile = "",
    [string]$LogRoot = ""
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectFile = Join-Path $ProjectRoot 'ProjectP.uproject'
$DefaultRunUAT = if (-not [string]::IsNullOrWhiteSpace($env:UE_RUNUAT)) {
    $env:UE_RUNUAT
}
else {
    'C:\Users\k2503200045\UnrealEngine\Engine\Build\BatchFiles\RunUAT.bat'
}
$DefaultClientBuildRoot = Join-Path $ProjectRoot 'Saved\Packaged\Client'
$DefaultCredentialsFile = Join-Path $ProjectRoot 'Saved\Gauntlet\gauntlet-login-lobby-credentials.json'
$DefaultServerRuntimeFile = Join-Path $ProjectRoot 'ServerRuntime.ini'
$DefaultLogRoot = Join-Path $ProjectRoot 'Saved\Logs\Gauntlet'
$TestName = 'ProjectPPartyDungeonFlowTest'
$RunId = Get-Date -Format 'yyyyMMdd_HHmmss'

function Write-Step {
    param([string]$Message)

    Write-Host ''
    Write-Host "==== $Message ===="
}

function Resolve-ConfiguredPath {
    param(
        [string]$Value,
        [string]$DefaultValue
    )

    if ([string]::IsNullOrWhiteSpace($Value)) {
        return $DefaultValue
    }

    return $Value
}

function Assert-RequiredPath {
    param(
        [string]$Path,
        [string]$Label
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        throw "$Label not found: $Path"
    }
}

function Find-ClientExecutable {
    param([string]$BuildRoot)

    $clientExe = Get-ChildItem -LiteralPath $BuildRoot -Recurse -Filter 'ProjectP.exe' -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -like '*\Windows\*' -or $_.FullName -like '*\Win64\*' } |
        Select-Object -First 1

    if (-not $clientExe) {
        throw "Client executable was not found under: $BuildRoot"
    }

    return $clientExe.FullName
}

function Build-RunUATArgs {
    param(
        [string]$ResolvedClientBuildRoot,
        [string]$ResolvedCredentialsFile,
        [string]$ResolvedServerRuntimeFile,
        [string]$ResolvedLogRoot
    )

    $gauntletTempRoot = Join-Path $ProjectRoot 'Saved\Gauntlet\Temp'
    $clientArgs = "-ProjectPServerConfig=`"$ResolvedServerRuntimeFile`" -windowed -ResX=640 -ResY=360 -unattended -log"

    return @(
        "-ScriptsForProject=$ProjectFile",
        'RunUnreal',
        "-project=$ProjectFile",
        "-build=$ResolvedClientBuildRoot",
        '-platform=Win64',
        '-configuration=Development',
        "-test=$TestName",
        '-numclients=3',
        '-packaged',
        '-log',
        '-unattended',
        "-LogDir=$ResolvedLogRoot",
        "-TempDir=$gauntletTempRoot",
        "-ClientArgs=$clientArgs",
        '-GauntletPartyDungeonFlowTest',
        "-GauntletCredentialsFile=$ResolvedCredentialsFile"
    )
}

try {
    $resolvedRunUAT = Resolve-ConfiguredPath -Value $RunUAT -DefaultValue $DefaultRunUAT
    $resolvedClientBuildRoot = Resolve-ConfiguredPath -Value $ClientBuildRoot -DefaultValue $DefaultClientBuildRoot
    $resolvedCredentialsFile = Resolve-ConfiguredPath -Value $CredentialsFile -DefaultValue $DefaultCredentialsFile
    $resolvedServerRuntimeFile = $DefaultServerRuntimeFile
    $resolvedLogRoot = Resolve-ConfiguredPath -Value $LogRoot -DefaultValue $DefaultLogRoot

    Write-Step 'Validate Paths'
    Assert-RequiredPath $ProjectFile 'Unreal project'
    Assert-RequiredPath $resolvedRunUAT 'RunUAT'
    Assert-RequiredPath $resolvedClientBuildRoot 'Client build root'
    Assert-RequiredPath $resolvedCredentialsFile 'Gauntlet credentials file'
    Assert-RequiredPath $resolvedServerRuntimeFile 'ServerRuntime.ini'
    $clientExe = Find-ClientExecutable -BuildRoot $resolvedClientBuildRoot

    New-Item -ItemType Directory -Force -Path $resolvedLogRoot | Out-Null

    Write-Host "Run id: $RunId"
    Write-Host "RunUAT: $resolvedRunUAT"
    Write-Host "Client build root: $resolvedClientBuildRoot"
    Write-Host "Client executable: $clientExe"
    Write-Host "Credentials file: $resolvedCredentialsFile"
    Write-Host "Server runtime file: $resolvedServerRuntimeFile"
    Write-Host "Log root: $resolvedLogRoot"

    $uatArgs = Build-RunUATArgs `
        -ResolvedClientBuildRoot $resolvedClientBuildRoot `
        -ResolvedCredentialsFile $resolvedCredentialsFile `
        -ResolvedServerRuntimeFile $resolvedServerRuntimeFile `
        -ResolvedLogRoot $resolvedLogRoot

    if ($ValidateOnly) {
        Write-Step 'Validate Only'
        Write-Host 'Validation completed without running Gauntlet.'
        exit 0
    }

    if ($DryRun) {
        Write-Step 'Dry Run'
        Write-Host "RunUAT: $resolvedRunUAT"
        Write-Host 'Arguments:'
        foreach ($arg in $uatArgs) {
            Write-Host "  $arg"
        }
        exit 0
    }

    $gauntletRunLog = Join-Path $resolvedLogRoot "Gauntlet_PartyDungeonFlow_$RunId.out.log"
    Write-Step 'Run Gauntlet Party Dungeon Flow Test'
    Write-Host "Output log: $gauntletRunLog"

    & $resolvedRunUAT @uatArgs *> $gauntletRunLog
    $exitCode = $LASTEXITCODE

    Write-Host "RunUAT exit code: $exitCode"
    if (Test-Path -LiteralPath $gauntletRunLog) {
        Get-Content -Tail 80 -Encoding UTF8 $gauntletRunLog
    }

    if ($exitCode -ne 0) {
        throw "Gauntlet Party Dungeon Flow test failed. See log: $gauntletRunLog"
    }

    Write-Step 'Completed'
    Write-Host 'Gauntlet Party Dungeon Flow test completed.'
    Write-Host "Gauntlet log: $gauntletRunLog"
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
