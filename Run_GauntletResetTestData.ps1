param(
    [switch]$PrepareDuplicateLogin,
    [switch]$WriteCredentialsFile,
    [switch]$DeleteCredentialsFile,
    [switch]$ValidateOnly,
    [switch]$PauseOnExit,
    [string]$CredentialsFile = "",
    [string]$ApiBaseUrl = ""
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$DefaultCredentialsFile = Join-Path $ProjectRoot 'Saved\Gauntlet\gauntlet-login-lobby-credentials.json'

function Write-Step {
    param([string]$Message)

    Write-Host ''
    Write-Host "==== $Message ===="
}

function Get-RequiredEnv {
    param([string]$Name)

    $value = [Environment]::GetEnvironmentVariable($Name, 'Process')
    if ([string]::IsNullOrWhiteSpace($value)) {
        throw "$Name is required."
    }

    return $value
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

function Get-CredentialsFilePath {
    if ([string]::IsNullOrWhiteSpace($CredentialsFile)) {
        return $DefaultCredentialsFile
    }

    return $CredentialsFile
}

function Write-GauntletCredentialsFile {
    param(
        [string]$OutputPath,
        [string]$TestAuth
    )

    $credentials = [ordered]@{
        testAuth = $TestAuth
        accounts = @(
            [ordered]@{
                clientIndex = 1
                id = 'Gauntlet-test-1'
                password = Get-RequiredEnv 'GAUNTLET_TEST_1_PASSWORD'
                username = 'TestBot1'
            }
            [ordered]@{
                clientIndex = 2
                id = 'Gauntlet-test-2'
                password = Get-RequiredEnv 'GAUNTLET_TEST_2_PASSWORD'
                username = 'TestBot2'
            }
            [ordered]@{
                clientIndex = 3
                id = 'Gauntlet-test-3'
                password = Get-RequiredEnv 'GAUNTLET_TEST_3_PASSWORD'
                username = 'TestBot3'
            }
        )
    }

    $parentPath = Split-Path -Parent $OutputPath
    if (-not [string]::IsNullOrWhiteSpace($parentPath) -and -not (Test-Path -LiteralPath $parentPath)) {
        New-Item -ItemType Directory -Force -Path $parentPath | Out-Null
    }

    $json = $credentials | ConvertTo-Json -Depth 5
    [System.IO.File]::WriteAllText($OutputPath, "$json`r`n", [System.Text.UTF8Encoding]::new($false))
    Write-Host "Gauntlet credentials file written: $OutputPath"
}

function Get-GameBackendBaseUrl {
    param([string]$OverrideBaseUrl)

    if (-not [string]::IsNullOrWhiteSpace($OverrideBaseUrl)) {
        return $OverrideBaseUrl.TrimEnd('/')
    }

    $configPath = Join-Path $ProjectRoot 'GameBackend\config.json'
    Assert-RequiredPath $configPath 'GameBackend config'

    $config = Get-Content -LiteralPath $configPath -Raw -Encoding UTF8 | ConvertFrom-Json
    $hostValue = if ($config.server.host) { [string]$config.server.host } else { '127.0.0.1' }
    if ($hostValue -eq '0.0.0.0' -or $hostValue -eq '::') {
        $hostValue = '127.0.0.1'
    }

    $portValue = if ($config.server.port) { [int]$config.server.port } else { 8080 }
    return "http://${hostValue}:$portValue"
}

function Invoke-GauntletResetApi {
    param(
        [string]$BaseUrl,
        [string]$TestAuth
    )

    $resetUrl = "$($BaseUrl.TrimEnd('/'))/api/test/reset-gauntlet"
    $body = @{
        prepareDuplicateLogin = [bool]$PrepareDuplicateLogin.IsPresent
    } | ConvertTo-Json

    Write-Step 'Call Reset API'
    Write-Host "URL: $resetUrl"
    Write-Host "PrepareDuplicateLogin: $($PrepareDuplicateLogin.IsPresent)"

    $response = Invoke-RestMethod `
        -Uri $resetUrl `
        -Method Post `
        -Headers @{ 'X-Gauntlet-Test-Auth' = $TestAuth } `
        -ContentType 'application/json' `
        -Body $body `
        -TimeoutSec 15

    if (-not $response.ok) {
        throw "Gauntlet reset API returned ok=false."
    }

    Write-Host 'Gauntlet reset API succeeded.'
    if ($response.reset) {
        $response.reset | ConvertTo-Json -Depth 5
    }
}

try {
    Write-Step 'Validate Secrets'
    $testAuth = Get-RequiredEnv 'GAUNTLET_TEST_AUTH'
    $resolvedCredentialsFile = Get-CredentialsFilePath
    Write-Host "Credentials file: $resolvedCredentialsFile"

    if ($WriteCredentialsFile) {
        Write-Step 'Write Credentials File'
        Write-GauntletCredentialsFile -OutputPath $resolvedCredentialsFile -TestAuth $testAuth
    }

    if ($ValidateOnly) {
        Write-Step 'Validate Only'
        Write-Host 'Validation completed without calling Reset API.'
        exit 0
    }

    $baseUrl = Get-GameBackendBaseUrl -OverrideBaseUrl $ApiBaseUrl
    Invoke-GauntletResetApi -BaseUrl $baseUrl -TestAuth $testAuth

    if ($DeleteCredentialsFile -and (Test-Path -LiteralPath $resolvedCredentialsFile)) {
        Remove-Item -LiteralPath $resolvedCredentialsFile -Force
        Write-Host "Gauntlet credentials file deleted: $resolvedCredentialsFile"
    }

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
