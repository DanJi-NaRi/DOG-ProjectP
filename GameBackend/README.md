# Game Backend

This folder contains a minimal local game backend for ProjectP.

## Why Node.js was chosen

- `Node.js` is already installed on this machine, so it can run immediately.
- `Express` is a small HTTP layer, which matches the backend's request/response flow.
- `mysql2` gives direct MySQL access without adding an ORM that would hide the SQL learning process.

Alternative options:

- `ASP.NET Core`: strong structure for production, but `dotnet` is not installed here.
- `C++ HTTP server`: closer to game server language, but much heavier for an initial backend API.

## Setup

1. Copy `GameBackend/config.example.json` to `GameBackend/config.json`.
2. Fill in your local MySQL account and database name. This project currently uses the `yuno_auth` schema.
3. Run `npm.cmd run game-backend` from the project root.

`npm.cmd run login-server` is kept as a compatibility alias.
`npm.cmd run dungeon-manager` starts the dungeon manager.
`npm.cmd run server-monitor` starts the server monitor at `http://127.0.0.1:9000`.

For a shared build machine, set `server.host` to `0.0.0.0` so other development PCs can reach the backend. Keep `database.host` as `127.0.0.1` when MySQL runs on the same build machine.
Set `monitor.host` to `0.0.0.0` when another PC needs to open the monitor page.

## Endpoints

- `GET /api/health`
- `GET /api/health/db`
- `POST /api/login`
- `POST /api/logout`
- `POST /api/session/ping`
- `POST /api/session/verify`
- `POST /api/dungeon/session/verify`
- `POST /api/dungeon/member-state`
- `POST /api/dungeon/member-state/query`
- `POST /api/dungeon/session-ended`
- `GET /api/telemetry/status`
- `GET /api/telemetry/lobby/users`
- `POST /api/telemetry/lobby`
- `POST /api/register`
- `GET /api/users`
- `GET /api/users/:ID`

## Server Monitor

- `GET /api/monitor/status`
- `GET /api/monitor/dungeons`
- `GET /api/monitor/ports`
- `GET /api/monitor/events`
- `GET /api/monitor/lobby/users`
