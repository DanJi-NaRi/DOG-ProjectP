@echo off
setlocal

cd /d "%~dp0"

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$ErrorActionPreference = 'Continue';" ^
  "$configPath = Join-Path (Resolve-Path '.').Path 'GameBackend\config.json';" ^
  "$config = Get-Content $configPath -Raw | ConvertFrom-Json;" ^
  "function Test-Http($name, $url) {" ^
  "  try { $response = Invoke-RestMethod -Uri $url -TimeoutSec 3; Write-Host ('[OK]   ' + $name + ' - ' + $url); return $true; }" ^
  "  catch { Write-Host ('[FAIL] ' + $name + ' - ' + $url + ' - ' + $_.Exception.Message); return $false; }" ^
  "}" ^
  "function Test-UdpEndpoint($name, $port) {" ^
  "  try { $endpoints = @(Get-NetUDPEndpoint -LocalPort $port -ErrorAction Stop); if ($endpoints.Count -gt 0) { $processNames = @(); foreach ($endpoint in $endpoints) { try { $processNames += (Get-Process -Id $endpoint.OwningProcess -ErrorAction Stop).ProcessName } catch {} } Write-Host ('[OK]   ' + $name + ' - UDP 127.0.0.1:' + $port + ' - ' + (($processNames | Select-Object -Unique) -join ', ')); return $true; } Write-Host ('[FAIL] ' + $name + ' - UDP 127.0.0.1:' + $port + ' - not listening'); return $false; }" ^
  "  catch { Write-Host ('[FAIL] ' + $name + ' - UDP 127.0.0.1:' + $port + ' - ' + $_.Exception.Message); return $false; }" ^
  "}" ^
  "$backendPort = if ($config.server.port) { [int]$config.server.port } else { 8080 };" ^
  "$managerPort = if ($config.dungeonManager.port) { [int]$config.dungeonManager.port } else { 8090 };" ^
  "$monitorPort = if ($config.monitor -and $config.monitor.port) { [int]$config.monitor.port } else { 9000 };" ^
  "$lobbyPort = if ($config.monitor -and $config.monitor.lobbyPort) { [int]$config.monitor.lobbyPort } else { 7777 };" ^
  "$dungeonStart = if ($config.dungeonManager.portStart) { [int]$config.dungeonManager.portStart } else { 7780 };" ^
  "$dungeonEnd = if ($config.dungeonManager.portEnd) { [int]$config.dungeonManager.portEnd } else { 7799 };" ^
  "$ok = $true;" ^
  "$result = Test-Http 'GameBackend' ('http://127.0.0.1:' + $backendPort + '/api/health'); $ok = $result -and $ok;" ^
  "$result = Test-Http 'GameBackend DB' ('http://127.0.0.1:' + $backendPort + '/api/health/db'); $ok = $result -and $ok;" ^
  "$result = Test-Http 'DungeonManager' ('http://127.0.0.1:' + $managerPort + '/api/health'); $ok = $result -and $ok;" ^
  "$result = Test-Http 'ServerMonitor' ('http://127.0.0.1:' + $monitorPort + '/api/monitor/status'); $ok = $result -and $ok;" ^
  "$result = Test-UdpEndpoint 'LobbyServer' $lobbyPort; $ok = $result -and $ok;" ^
  "$openDungeonPorts = @(Get-NetUDPEndpoint -ErrorAction SilentlyContinue | Where-Object { $_.LocalPort -ge $dungeonStart -and $_.LocalPort -le $dungeonEnd } | Select-Object -ExpandProperty LocalPort -Unique | Sort-Object);" ^
  "if ($openDungeonPorts.Count -gt 0) { Write-Host ('[INFO] Open dungeon ports: ' + ($openDungeonPorts -join ', ')); } else { Write-Host '[INFO] Open dungeon ports: none'; }" ^
  "if ($ok) { Write-Host 'Health check passed.'; exit 0; } else { Write-Host 'Health check failed.'; exit 1; }"

set "RESULT=%ERRORLEVEL%"
endlocal & exit /b %RESULT%
