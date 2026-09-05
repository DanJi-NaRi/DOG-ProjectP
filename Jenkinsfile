pipeline {
    agent { label 'gauntlet-windows-login' }

    options {
        disableConcurrentBuilds()
        timestamps()
    }

    stages {
        stage('Checkout SVN') {
            steps {
                checkout([
                    $class: 'SubversionSCM',
                    locations: [[
                        remote: 'https://svna.gameinjae.kr/svn/GA7thFinal_RageOfPharaoh/Project',
                        credentialsId: 'kkkgg549',
                        local: '.'
                    ]],
                    workspaceUpdater: [$class: 'UpdateUpdater']
                ])
            }
        }

        stage('Validate Environment') {
            steps {
                powershell '''
                $ErrorActionPreference = "Stop"

                $requiredPaths = @(
                    "ProjectP.uproject",
                    "Source\\ProjectP.Target.cs",
                    "Source\\ProjectPServer.Target.cs",
                    "Run_ServerBuildPackaging.ps1",
                    "Run_ClientBuildPackaging.ps1",
                    "Run_GauntletResetTestData.ps1",
                    "Run_GauntletLoginLobbyTest.ps1",
                    "Run_GauntletPartyDungeonFlowTest.ps1",
                    "Stop_AllServers.bat",
                    "Start_AllServers.bat",
                    "HealthCheck_Servers.bat",
                    "GameBackend\\config.example.json",
                    "package.json",
                    "C:\\Users\\k2503200045\\UnrealEngine\\Engine\\Build\\BatchFiles\\RunUAT.bat"
                )

                foreach ($path in $requiredPaths) {
                    if (-not (Test-Path $path)) {
                        throw "Required path not found: $path"
                    }

                    Write-Host "OK: $path"
                }

                svn --version --quiet
                '''
            }
        }

        stage('Prepare Runtime Config') {
            steps {
                powershell '''
                $ErrorActionPreference = "Stop"

                $backendConfigPath = "GameBackend\\config.json"
                $serverRuntimePath = "ServerRuntime.ini"
                $secretBackendConfigPath = "C:\\JenkinsSecrets\\ProjectP\\GameBackend.config.json"
                $secretServerRuntimePath = "C:\\JenkinsSecrets\\ProjectP\\ServerRuntime.ini"

                if (-not (Test-Path $secretBackendConfigPath)) {
                    throw "Jenkins backend config secret was not found."
                }

                if (-not (Test-Path $secretServerRuntimePath)) {
                    throw "Jenkins server runtime config secret was not found."
                }

                Copy-Item -LiteralPath $secretBackendConfigPath -Destination $backendConfigPath -Force
                $backendConfig = Get-Content -Raw -Encoding UTF8 $backendConfigPath | ConvertFrom-Json
                if ($null -eq $backendConfig.dungeonManager) {
                    $backendConfig | Add-Member -MemberType NoteProperty -Name dungeonManager -Value ([pscustomobject]@{}) -Force
                }

                $backendConfig.dungeonManager | Add-Member -MemberType NoteProperty -Name mapName -Value "/Game/LeDuat/Maps/BuildMap/Map_Stage1" -Force
                $backendConfig.dungeonManager | Add-Member -MemberType NoteProperty -Name serverExecutable -Value "Saved/Packaged/DungeonServer/WindowsServer/ProjectPServer.exe" -Force

                $startupWaitMs = 0
                if ($null -ne $backendConfig.dungeonManager.startupWaitMs) {
                    $startupWaitMs = [int]$backendConfig.dungeonManager.startupWaitMs
                }

                if ($startupWaitMs -lt 90000) {
                    $backendConfig.dungeonManager | Add-Member -MemberType NoteProperty -Name startupWaitMs -Value 90000 -Force
                }

                $utf8NoBom = New-Object System.Text.UTF8Encoding($false)
                [System.IO.File]::WriteAllText((Resolve-Path $backendConfigPath).Path, ($backendConfig | ConvertTo-Json -Depth 10), $utf8NoBom)
                Copy-Item -LiteralPath $secretServerRuntimePath -Destination $serverRuntimePath -Force
                Write-Host "Backend config prepared: $backendConfigPath"
                Write-Host "Server runtime config prepared: $serverRuntimePath"
                '''
            }
        }

        stage('Stop Servers') {
            steps {
                bat 'call Stop_AllServers.bat'
            }
        }

        stage('Build Server') {
            steps {
                powershell './Run_ServerBuildPackaging.ps1'
            }
        }

        stage('Build Client') {
            steps {
                powershell './Run_ClientBuildPackaging.ps1'
            }
        }

        stage('Refresh Runtime Host') {
            steps {
                powershell '''
                $ErrorActionPreference = "Stop"

                function Get-ProjectPServerHost {
                    if ($env:PROJECTP_SERVER_HOST -and $env:PROJECTP_SERVER_HOST.Trim()) {
                        return $env:PROJECTP_SERVER_HOST.Trim()
                    }

                    $configs = @(Get-NetIPConfiguration | Where-Object {
                        $_.IPv4Address -and $_.NetAdapter.Status -eq "Up"
                    })

                    $withGateway = @($configs | Where-Object {
                        $_.IPv4DefaultGateway -and $_.IPv4DefaultGateway.NextHop
                    })

                    $candidates = if ($withGateway.Count -gt 0) { $withGateway } else { $configs }
                    foreach ($config in $candidates) {
                        foreach ($address in @($config.IPv4Address)) {
                            $ip = [string]$address.IPAddress
                            if ($ip -and $ip -notlike "127.*" -and $ip -notlike "169.254.*") {
                                return $ip
                            }
                        }
                    }

                    throw "Could not detect server IPv4 address."
                }

                function Set-ServerRuntimeHost {
                    param(
                        [string]$Path,
                        [string]$ServerHost
                    )

                    if (-not (Test-Path $Path)) {
                        throw "ServerRuntime.ini not found: $Path"
                    }

                    $resolvedPath = (Resolve-Path $Path).Path
                    $text = Get-Content -Raw -Encoding UTF8 $resolvedPath

                    if ($text -match "(?m)^ServerHost\\s*=") {
                        $text = [regex]::Replace($text, "(?m)^ServerHost\\s*=.*$", "ServerHost=$ServerHost")
                    }
                    elseif ($text -match "(?m)^\\[ProjectP\\.Server\\]") {
                        $text = [regex]::Replace($text, "(?m)^\\[ProjectP\\.Server\\]\\s*$", "[ProjectP.Server]`r`nServerHost=$ServerHost")
                    }
                    else {
                        $text = $text.TrimEnd() + "`r`n`r`n[ProjectP.Server]`r`nServerHost=$ServerHost`r`n"
                    }

                    $utf8NoBom = New-Object System.Text.UTF8Encoding($false)
                    [System.IO.File]::WriteAllText($resolvedPath, $text, $utf8NoBom)
                    Write-Host "ServerHost refreshed: $Path -> $ServerHost"
                }

                $serverHost = Get-ProjectPServerHost
                Write-Host "Detected server host: $serverHost"

                $runtimePaths = @(
                    "ServerRuntime.ini",
                    "Saved\\Packaged\\LobbyServer\\WindowsServer\\ServerRuntime.ini",
                    "Saved\\Packaged\\DungeonServer\\WindowsServer\\ServerRuntime.ini",
                    "Saved\\Packaged\\Client\\Windows\\ServerRuntime.ini"
                )

                foreach ($runtimePath in $runtimePaths) {
                    Set-ServerRuntimeHost -Path $runtimePath -ServerHost $serverHost
                }
                '''
            }
        }

        stage('Start Servers') {
            steps {
                withCredentials([
                    string(credentialsId: 'projectp-gauntlet-test-auth', variable: 'GAUNTLET_TEST_AUTH')
                ]) {
                    bat '''
                    set JENKINS_NODE_COOKIE=dontKillMe
                    set BUILD_ID=dontKillMe
                    call Start_AllServers.bat --no-pause
                    '''
                }
            }
        }

        stage('Health Check') {
            steps {
                bat 'call HealthCheck_Servers.bat'
            }
        }

        stage('Reset Gauntlet Test Data') {
            steps {
                withCredentials([
                    string(credentialsId: 'projectp-gauntlet-test-auth', variable: 'GAUNTLET_TEST_AUTH'),
                    string(credentialsId: 'projectp-gauntlet-test-1-password', variable: 'GAUNTLET_TEST_1_PASSWORD'),
                    string(credentialsId: 'projectp-gauntlet-test-2-password', variable: 'GAUNTLET_TEST_2_PASSWORD'),
                    string(credentialsId: 'projectp-gauntlet-test-3-password', variable: 'GAUNTLET_TEST_3_PASSWORD')
                ]) {
                    powershell '''
                    $ErrorActionPreference = "Stop"
                    .\\Run_GauntletResetTestData.ps1 -WriteCredentialsFile -PrepareDuplicateLogin
                    '''
                }
            }
        }

        stage('Run Gauntlet Login Lobby Test') {
            steps {
                catchError(buildResult: 'FAILURE', stageResult: 'FAILURE') {
                    powershell '''
                    $ErrorActionPreference = "Stop"
                    .\\Run_GauntletLoginLobbyTest.ps1
                    '''
                }
            }
        }

        stage('Reset Gauntlet Test Data Before Party Dungeon Flow') {
            steps {
                withCredentials([
                    string(credentialsId: 'projectp-gauntlet-test-auth', variable: 'GAUNTLET_TEST_AUTH'),
                    string(credentialsId: 'projectp-gauntlet-test-1-password', variable: 'GAUNTLET_TEST_1_PASSWORD'),
                    string(credentialsId: 'projectp-gauntlet-test-2-password', variable: 'GAUNTLET_TEST_2_PASSWORD'),
                    string(credentialsId: 'projectp-gauntlet-test-3-password', variable: 'GAUNTLET_TEST_3_PASSWORD')
                ]) {
                    powershell '''
                    $ErrorActionPreference = "Stop"
                    .\\Run_GauntletResetTestData.ps1 -WriteCredentialsFile
                    '''
                }
            }
        }

        stage('Run Gauntlet Party Dungeon Flow Test') {
            steps {
                catchError(buildResult: 'FAILURE', stageResult: 'FAILURE') {
                    powershell '''
                    $ErrorActionPreference = "Stop"
                    .\\Run_GauntletPartyDungeonFlowTest.ps1
                    '''
                }
            }
        }

        stage('Post Reset Gauntlet Test Data') {
            steps {
                withCredentials([
                    string(credentialsId: 'projectp-gauntlet-test-auth', variable: 'GAUNTLET_TEST_AUTH')
                ]) {
                    powershell '''
                    $ErrorActionPreference = "Stop"
                    .\\Run_GauntletResetTestData.ps1 -DeleteCredentialsFile
                    '''
                }
            }
        }

        stage('Publish Distribution') {
            when {
                expression { currentBuild.currentResult == 'SUCCESS' }
            }

            steps {
                powershell '''
                $ErrorActionPreference = "Stop"

                $baseRoot = "C:\\Project\\파라오의 분노 (배포)"
                $serverRoot = Join-Path $baseRoot "서버 배포"
                $clientRoot = Join-Path $baseRoot "클라 배포"
                $expectedBaseRoot = "C:\\Project\\파라오의 분노 (배포)"
                $resolvedBaseRoot = [System.IO.Path]::GetFullPath($baseRoot)
                $utf8NoBom = New-Object System.Text.UTF8Encoding($false)

                if ($resolvedBaseRoot -ne $expectedBaseRoot) {
                    throw "Unexpected distribution root: $resolvedBaseRoot"
                }

                function Clear-DirectoryContents {
                    param([string]$Path)

                    New-Item -ItemType Directory -Force -Path $Path | Out-Null
                    $resolvedRoot = [System.IO.Path]::GetFullPath($Path)
                    Get-ChildItem -LiteralPath $Path -Force | ForEach-Object {
                        $childPath = [System.IO.Path]::GetFullPath($_.FullName)
                        if (-not $childPath.StartsWith($resolvedRoot + [System.IO.Path]::DirectorySeparatorChar)) {
                            throw "Refusing to remove outside distribution root: $childPath"
                        }

                        Remove-Item -LiteralPath $_.FullName -Recurse -Force
                    }
                }

                function Invoke-RobocopyChecked {
                    param(
                        [string]$Source,
                        [string]$Destination,
                        [string]$Label
                    )

                    if (-not (Test-Path $Source)) {
                        throw "Source not found for ${Label}: $Source"
                    }

                    Write-Host "Copying $Label..."
                    robocopy $Source $Destination /E /XD .svn /NFL /NDL /NJH /NJS /NP
                    $exitCode = $LASTEXITCODE
                    if ($exitCode -gt 7) {
                        throw "robocopy failed for ${Label}. ExitCode=$exitCode"
                    }

                    $global:LASTEXITCODE = 0
                }

                function Write-Utf8NoBomFile {
                    param(
                        [string]$Path,
                        [string]$Text
                    )

                    $normalizedText = [regex]::Replace($Text, "\\r\\n|\\r|\\n", "`r`n")
                    [System.IO.File]::WriteAllText($Path, $normalizedText, $utf8NoBom)
                }

                function Write-StartClientScript {
                    param([string]$Path)

                    $scriptText = @'
@echo off
setlocal

cd /d "%~dp0"

set "CLIENT_EXE=%~dp0Saved\\Packaged\\Client\\Windows\\ProjectP.exe"

if not exist "%CLIENT_EXE%" (
    echo Client executable not found:
    echo "%CLIENT_EXE%"
    echo Build and package the client first.
    pause
    exit /b 1
)

start "ProjectP Client" "%CLIENT_EXE%" -log

endlocal
'@

                    Write-Utf8NoBomFile -Path $Path -Text ($scriptText.TrimEnd() + "`r`n")
                }

                function Get-ClientRuntimeConfigText {
                    param([string]$SourcePath)

                    if (-not (Test-Path $SourcePath)) {
                        throw "ServerRuntime.ini not found: $SourcePath"
                    }

                    $text = Get-Content -Raw -Encoding UTF8 $SourcePath
                    $text = [regex]::Replace($text, "(?m)^DungeonStateServerAuthKey\\s*=.*\\r?\\n?", "")
                    return $text.TrimEnd() + "`r`n"
                }

                Clear-DirectoryContents -Path $baseRoot
                New-Item -ItemType Directory -Force -Path $serverRoot | Out-Null
                New-Item -ItemType Directory -Force -Path $clientRoot | Out-Null

                Invoke-RobocopyChecked -Source "Saved\\Packaged\\LobbyServer" -Destination (Join-Path $serverRoot "Saved\\Packaged\\LobbyServer") -Label "LobbyServer package"
                Invoke-RobocopyChecked -Source "Saved\\Packaged\\DungeonServer" -Destination (Join-Path $serverRoot "Saved\\Packaged\\DungeonServer") -Label "DungeonServer package"
                Invoke-RobocopyChecked -Source "GameBackend" -Destination (Join-Path $serverRoot "GameBackend") -Label "GameBackend"
                Invoke-RobocopyChecked -Source "node_modules" -Destination (Join-Path $serverRoot "node_modules") -Label "node_modules"

                foreach ($file in @("package.json", "package-lock.json", "Start_AllServers.bat", "Stop_AllServers.bat", "HealthCheck_Servers.bat", "Move-ServerWindows.ps1", "ServerRuntime.ini")) {
                    if (-not (Test-Path $file)) {
                        throw "Distribution source file not found: $file"
                    }

                    Copy-Item -LiteralPath $file -Destination (Join-Path $serverRoot $file) -Force
                }

                $serverConfigPath = Join-Path $serverRoot "GameBackend\\config.json"
                $serverConfig = Get-Content -Raw -Encoding UTF8 $serverConfigPath | ConvertFrom-Json
                $serverConfig.dungeonManager.serverExecutable = "Saved/Packaged/DungeonServer/WindowsServer/ProjectPServer.exe"
                [System.IO.File]::WriteAllText($serverConfigPath, (($serverConfig | ConvertTo-Json -Depth 20) + "`r`n"), $utf8NoBom)

                Invoke-RobocopyChecked -Source "Saved\\Packaged\\Client" -Destination (Join-Path $clientRoot "Saved\\Packaged\\Client") -Label "Client package"
                Write-StartClientScript -Path (Join-Path $clientRoot "Start_Client.bat")

                $clientRuntimeText = Get-ClientRuntimeConfigText -SourcePath "ServerRuntime.ini"
                Write-Utf8NoBomFile -Path (Join-Path $clientRoot "ServerRuntime.ini") -Text $clientRuntimeText
                Write-Utf8NoBomFile -Path (Join-Path $clientRoot "Saved\\Packaged\\Client\\Windows\\ServerRuntime.ini") -Text $clientRuntimeText

                Write-Host "Distribution published: $baseRoot"
                Write-Host "Server distribution: $serverRoot"
                Write-Host "Client distribution: $clientRoot"
                exit 0
                '''
            }
        }
    }

    post {
        always {
            archiveArtifacts artifacts: 'Saved/Logs/**/*.log', allowEmptyArchive: true
        }
    }
}
