@echo off
setlocal

set "PROJECTP_NO_PAUSE="
if /I "%~1"=="--no-pause" set "PROJECTP_NO_PAUSE=1"

cd /d "%~dp0"

title ProjectP ServerHealthCheck

set "PROJECT_DIR=%~dp0"
set "LOBBY_DIR=%PROJECT_DIR%Saved\Packaged\LobbyServer\WindowsServer"
set "LOBBY_EXE=%LOBBY_DIR%\ProjectPServer.exe"

where npm.cmd >nul 2>nul
if errorlevel 1 (
    echo npm.cmd not found. Install Node.js or check PATH.
    if not defined PROJECTP_NO_PAUSE pause
    exit /b 1
)

if not exist "%LOBBY_EXE%" (
    echo Lobby server executable not found:
    echo "%LOBBY_EXE%"
    echo Build and package the server first.
    if not defined PROJECTP_NO_PAUSE pause
    exit /b 1
)

echo Starting GameBackend HTTP server...
start "ProjectP GameBackend 8080" cmd /k "title npm run game-backend && mode con: cols=90 lines=12 && cd /d ""%PROJECT_DIR%"" && npm.cmd run game-backend"

echo Starting Server Monitor...
start "ProjectP ServerMonitor 9000" cmd /k "title npm run server-monitor && mode con: cols=90 lines=12 && cd /d ""%PROJECT_DIR%"" && npm.cmd run server-monitor"

echo Starting Dungeon Manager HTTP server...
start "ProjectP DungeonManager 8090" cmd /k "title npm run dungeon-manager && mode con: cols=90 lines=12 && cd /d ""%PROJECT_DIR%"" && npm.cmd run dungeon-manager"

echo Starting Lobby Server...
start "ProjectP LobbyServer 7777" /D "%LOBBY_DIR%" "%LOBBY_EXE%" /Game/LeDuat/Maps/Map_Lobby -log -port=7777 -unattended

echo Positioning server console windows...
powershell -NoProfile -ExecutionPolicy Bypass -File "%PROJECT_DIR%Move-ServerWindows.ps1"
title ProjectP ServerHealthCheck

echo.
echo Waiting for servers to initialize...
timeout /t 10 /nobreak >nul

call "%PROJECT_DIR%HealthCheck_Servers.bat"
set "RESULT=%ERRORLEVEL%"

if not defined PROJECTP_NO_PAUSE pause

endlocal & exit /b %RESULT%
