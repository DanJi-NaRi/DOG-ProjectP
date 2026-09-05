@echo off
setlocal

cd /d "%~dp0"

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$root = (Resolve-Path '.').Path;" ^
  "$targets = Get-CimInstance Win32_Process | Where-Object {" ^
  "  $cmd = $_.CommandLine;" ^
  "  $path = $_.ExecutablePath;" ^
  "  ($_.Name -eq 'node.exe' -and $cmd -and ($cmd -like '*GameBackend/server.js*' -or $cmd -like '*GameBackend\server.js*' -or $cmd -like '*GameBackend/dungeon-manager.js*' -or $cmd -like '*GameBackend\dungeon-manager.js*' -or $cmd -like '*GameBackend/server-monitor.js*' -or $cmd -like '*GameBackend\server-monitor.js*')) -or" ^
  "  ($_.Name -eq 'cmd.exe' -and $cmd -and ($cmd -like '*npm.cmd run game-backend*' -or $cmd -like '*npm.cmd run dungeon-manager*' -or $cmd -like '*npm.cmd run server-monitor*')) -or" ^
  "  ($_.Name -eq 'ProjectPServer.exe' -and $path -and $path -like ($root + '\Saved\Packaged\*Server\WindowsServer*'))" ^
  "};" ^
  "$count = @($targets).Count;" ^
  "foreach ($target in $targets) { try { Stop-Process -Id $target.ProcessId -Force -ErrorAction Stop; Write-Host ('Stopped PID ' + $target.ProcessId + ' ' + $target.Name); } catch { Write-Host ('Failed PID ' + $target.ProcessId + ': ' + $_.Exception.Message); } }" ^
  "Write-Host ('Total stopped: ' + $count);" ^
  "exit 0"

endlocal
