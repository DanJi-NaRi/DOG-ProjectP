const swaggerJSDoc = require("swagger-jsdoc");

const swaggerOptions = {
  definition: {
    openapi: "3.0.0",
    info: {
      title: "ProjectP Game Backend API",
      version: "1.0.0",
      description: "Node.js Express API documentation for ProjectP.",
    },
    servers: [
      {
        url: "/",
        description: "Current game backend host",
      },
      {
        url: "http://127.0.0.1:8080",
        description: "Local game backend default",
      },
    ],
    tags: [
      {
        name: "Health",
        description: "Backend and database health checks",
      },
      {
        name: "Auth",
        description: "Login, logout, session, and register APIs",
      },
      {
        name: "Dungeon",
        description: "Dungeon session allocation and member state APIs",
      },
      {
        name: "Telemetry",
        description: "Server telemetry APIs",
      },
      {
        name: "Users",
        description: "User lookup APIs",
      },
    ],
    components: {
      securitySchemes: {
        serverAuth: {
          type: "apiKey",
          in: "header",
          name: "X-Server-Auth",
        },
      },
      schemas: {
        BasicOkResponse: {
          type: "object",
          properties: {
            ok: {
              type: "boolean",
              example: true,
            },
            message: {
              type: "string",
              example: "Request succeeded.",
            },
          },
          required: ["ok"],
        },
        ErrorResponse: {
          type: "object",
          properties: {
            ok: {
              type: "boolean",
              example: false,
            },
            message: {
              type: "string",
              example: "Request failed.",
            },
            error: {
              type: "string",
              example: "Error detail.",
            },
          },
          required: ["ok", "message"],
        },
        User: {
          type: "object",
          properties: {
            user_Index: {
              type: "integer",
              example: 1,
            },
            ID: {
              type: "string",
              example: "testuser",
            },
            username: {
              type: "string",
              example: "TestUser",
            },
            status: {
              type: "integer",
              example: 1,
            },
            created_at: {
              type: "string",
              format: "date-time",
            },
            last_login_at: {
              type: "string",
              format: "date-time",
            },
          },
        },
        LoginRequest: {
          type: "object",
          properties: {
            ID: {
              type: "string",
              example: "testuser",
            },
            password: {
              type: "string",
              example: "password",
            },
          },
          required: ["ID", "password"],
        },
        RegisterRequest: {
          type: "object",
          properties: {
            ID: {
              type: "string",
              example: "testuser",
            },
            username: {
              type: "string",
              example: "TestUser",
            },
            password: {
              type: "string",
              example: "password",
            },
          },
          required: ["ID", "username", "password"],
        },
        TokenRequest: {
          type: "object",
          properties: {
            token: {
              type: "string",
              example: "login-token",
            },
          },
          required: ["token"],
        },
        DungeonMember: {
          type: "object",
          properties: {
            userIndex: {
              type: "integer",
              example: 1,
            },
            characterId: {
              type: "integer",
              enum: [100, 200, 300],
              example: 100,
            },
          },
          required: ["userIndex", "characterId"],
        },
        DungeonSession: {
          type: "object",
          properties: {
            dungeonSessionId: {
              type: "string",
              example: "dungeon-001",
            },
            address: {
              type: "string",
              example: "127.0.0.1:7780",
            },
            host: {
              type: "string",
              example: "127.0.0.1",
            },
            port: {
              type: "integer",
              example: 7780,
            },
            pid: {
              type: "integer",
              example: 12345,
            },
          },
        },
        DungeonAllocateRequest: {
          type: "object",
          properties: {
            partyId: {
              type: "integer",
              example: 1,
            },
            memberUserIndexes: {
              type: "array",
              items: {
                type: "integer",
              },
              example: [1, 2, 3],
            },
            members: {
              type: "array",
              items: {
                $ref: "#/components/schemas/DungeonMember",
              },
            },
          },
        },
        DungeonSessionIdRequest: {
          type: "object",
          properties: {
            dungeonSessionId: {
              type: "string",
              example: "dungeon-001",
            },
          },
          required: ["dungeonSessionId"],
        },
        DungeonSessionVerifyRequest: {
          type: "object",
          properties: {
            token: {
              type: "string",
              example: "login-token",
            },
            dungeonSessionId: {
              type: "string",
              example: "dungeon-001",
            },
          },
          required: ["token", "dungeonSessionId"],
        },
        DungeonMemberState: {
          type: "object",
          properties: {
            user_Index: {
              type: "integer",
              example: 1,
            },
            username: {
              type: "string",
              example: "TestUser",
            },
            connectionState: {
              type: "string",
              enum: ["Online", "InGame", "OutGame", "Offline"],
              example: "Online",
            },
            dungeonSessionId: {
              type: "string",
              example: "",
            },
            isJoinable: {
              type: "boolean",
              example: false,
            },
            updatedAt: {
              type: "string",
              format: "date-time",
            },
          },
        },
        DungeonMemberStateRequest: {
          type: "object",
          properties: {
            userIndex: {
              type: "integer",
              example: 1,
            },
            connectionState: {
              type: "string",
              enum: ["Online", "InGame", "OutGame", "Offline"],
              example: "Online",
            },
            dungeonSessionId: {
              type: "string",
              example: "",
            },
          },
          required: ["userIndex", "connectionState"],
        },
        LobbyConnectedUser: {
          type: "object",
          properties: {
            userIndex: {
              type: "integer",
              example: 1,
            },
            playerId: {
              type: "integer",
              example: 0,
            },
            username: {
              type: "string",
              example: "TestUser",
            },
            loginAt: {
              type: "string",
              format: "date-time",
            },
            connectedSeconds: {
              type: "integer",
              example: 60,
            },
            authVerified: {
              type: "boolean",
              example: true,
            },
          },
        },
        LobbyTelemetryRequest: {
          type: "object",
          properties: {
            currentClientCount: {
              type: "integer",
              example: 1,
            },
            totalConnected: {
              type: "integer",
              example: 10,
            },
            totalDisconnected: {
              type: "integer",
              example: 9,
            },
            reportedAt: {
              type: "string",
              format: "date-time",
            },
            connectedUsers: {
              type: "array",
              items: {
                $ref: "#/components/schemas/LobbyConnectedUser",
              },
            },
          },
          required: ["currentClientCount"],
        },
      },
    },
    paths: {
      "/api/health": {
        get: {
          tags: ["Health"],
          summary: "Check game backend health.",
          responses: {
            200: {
              description: "Game backend is running.",
            },
          },
        },
      },
      "/api/health/db": {
        get: {
          tags: ["Health"],
          summary: "Check database connection.",
          responses: {
            200: {
              description: "Database connection succeeded.",
            },
            500: {
              description: "Database connection failed.",
            },
          },
        },
      },
      "/api/login": {
        post: {
          tags: ["Auth"],
          summary: "Log in and create a login token.",
          requestBody: {
            required: true,
            content: {
              "application/json": {
                schema: {
                  $ref: "#/components/schemas/LoginRequest",
                },
              },
            },
          },
          responses: {
            200: {
              description: "Login succeeded.",
            },
            400: {
              description: "ID or password is missing.",
            },
            401: {
              description: "Invalid ID or password.",
            },
            409: {
              description: "User is already logged in.",
            },
            500: {
              description: "Login failed.",
            },
          },
        },
      },
      "/api/logout": {
        post: {
          tags: ["Auth"],
          summary: "Revoke a login token.",
          requestBody: {
            required: true,
            content: {
              "application/json": {
                schema: {
                  $ref: "#/components/schemas/TokenRequest",
                },
              },
            },
          },
          responses: {
            200: {
              description: "Logout request completed.",
            },
            400: {
              description: "Token is missing.",
            },
            500: {
              description: "Logout failed.",
            },
          },
        },
      },
      "/api/session/ping": {
        post: {
          tags: ["Auth"],
          summary: "Keep a login token active.",
          requestBody: {
            required: true,
            content: {
              "application/json": {
                schema: {
                  $ref: "#/components/schemas/TokenRequest",
                },
              },
            },
          },
          responses: {
            200: {
              description: "Session ping succeeded.",
            },
            400: {
              description: "Token is missing.",
            },
            401: {
              description: "Login token is inactive.",
            },
            500: {
              description: "Session ping failed.",
            },
          },
        },
      },
      "/api/session/verify": {
        post: {
          tags: ["Auth"],
          summary: "Verify a login token.",
          requestBody: {
            required: true,
            content: {
              "application/json": {
                schema: {
                  $ref: "#/components/schemas/TokenRequest",
                },
              },
            },
          },
          responses: {
            200: {
              description: "Session verify succeeded.",
            },
            400: {
              description: "Token is missing.",
            },
            401: {
              description: "Login token is inactive.",
            },
            500: {
              description: "Session verify failed.",
            },
          },
        },
      },
      "/api/register": {
        post: {
          tags: ["Auth"],
          summary: "Create a new user account.",
          requestBody: {
            required: true,
            content: {
              "application/json": {
                schema: {
                  $ref: "#/components/schemas/RegisterRequest",
                },
              },
            },
          },
          responses: {
            201: {
              description: "Register succeeded.",
            },
            400: {
              description: "Required field is missing.",
            },
            409: {
              description: "ID already exists.",
            },
            500: {
              description: "Register failed.",
            },
          },
        },
      },
      "/api/dungeon/session/verify": {
        post: {
          tags: ["Dungeon"],
          summary: "Verify that a user can enter a dungeon session.",
          security: [
            {
              serverAuth: [],
            },
          ],
          requestBody: {
            required: true,
            content: {
              "application/json": {
                schema: {
                  $ref: "#/components/schemas/DungeonSessionVerifyRequest",
                },
              },
            },
          },
          responses: {
            200: {
              description: "Dungeon session verify succeeded.",
            },
            400: {
              description: "Required field is missing.",
            },
            401: {
              description: "Auth failed.",
            },
            403: {
              description: "User is not allocated to the dungeon session.",
            },
            500: {
              description: "Dungeon session verify failed.",
            },
          },
        },
      },
      "/api/dungeon/allocate": {
        post: {
          tags: ["Dungeon"],
          summary: "Allocate a dungeon server through DungeonManager.",
          security: [
            {
              serverAuth: [],
            },
          ],
          requestBody: {
            required: true,
            content: {
              "application/json": {
                schema: {
                  $ref: "#/components/schemas/DungeonAllocateRequest",
                },
              },
            },
          },
          responses: {
            200: {
              description: "Dungeon server allocated.",
            },
            401: {
              description: "Server auth is invalid.",
            },
            502: {
              description: "DungeonManager allocation failed.",
            },
          },
        },
      },
      "/api/dungeon/shutdown": {
        post: {
          tags: ["Dungeon"],
          summary: "Request dungeon server shutdown.",
          security: [
            {
              serverAuth: [],
            },
          ],
          requestBody: {
            required: true,
            content: {
              "application/json": {
                schema: {
                  $ref: "#/components/schemas/DungeonSessionIdRequest",
                },
              },
            },
          },
          responses: {
            200: {
              description: "Dungeon server shutdown requested.",
            },
            400: {
              description: "Dungeon session ID is missing.",
            },
            401: {
              description: "Server auth is invalid.",
            },
            502: {
              description: "DungeonManager shutdown failed.",
            },
          },
        },
      },
      "/api/dungeon/session-ended": {
        post: {
          tags: ["Dungeon"],
          summary: "Notify that a dungeon session ended.",
          security: [
            {
              serverAuth: [],
            },
          ],
          requestBody: {
            required: true,
            content: {
              "application/json": {
                schema: {
                  $ref: "#/components/schemas/DungeonSessionIdRequest",
                },
              },
            },
          },
          responses: {
            200: {
              description: "Dungeon session ended.",
            },
            400: {
              description: "Dungeon session ID is missing.",
            },
            401: {
              description: "Server auth is invalid.",
            },
          },
        },
      },
      "/api/dungeon/member-state": {
        post: {
          tags: ["Dungeon"],
          summary: "Update a dungeon member connection state.",
          security: [
            {
              serverAuth: [],
            },
          ],
          requestBody: {
            required: true,
            content: {
              "application/json": {
                schema: {
                  $ref: "#/components/schemas/DungeonMemberStateRequest",
                },
              },
            },
          },
          responses: {
            200: {
              description: "Dungeon member state updated.",
            },
            400: {
              description: "Invalid member state request.",
            },
            401: {
              description: "Server auth is invalid.",
            },
            404: {
              description: "User not found.",
            },
            500: {
              description: "Failed to update dungeon member state.",
            },
          },
        },
      },
      "/api/dungeon/member-state/query": {
        post: {
          tags: ["Dungeon"],
          summary: "Query a user's effective dungeon member state.",
          requestBody: {
            required: true,
            content: {
              "application/json": {
                schema: {
                  $ref: "#/components/schemas/TokenRequest",
                },
              },
            },
          },
          responses: {
            200: {
              description: "Dungeon member state returned.",
            },
            400: {
              description: "Token is missing.",
            },
            401: {
              description: "Login token is inactive.",
            },
            500: {
              description: "Failed to query dungeon member state.",
            },
          },
        },
      },
      "/api/telemetry/status": {
        get: {
          tags: ["Telemetry"],
          summary: "Get backend telemetry snapshot.",
          security: [
            {
              serverAuth: [],
            },
          ],
          responses: {
            200: {
              description: "Telemetry snapshot returned.",
            },
            401: {
              description: "Server auth is invalid.",
            },
          },
        },
      },
      "/api/telemetry/events": {
        get: {
          tags: ["Telemetry"],
          summary: "Get recent backend API events.",
          security: [
            {
              serverAuth: [],
            },
          ],
          responses: {
            200: {
              description: "Telemetry events returned.",
            },
            401: {
              description: "Server auth is invalid.",
            },
          },
        },
      },
      "/api/telemetry/lobby/users": {
        get: {
          tags: ["Telemetry"],
          summary: "Get currently connected lobby users.",
          security: [
            {
              serverAuth: [],
            },
          ],
          responses: {
            200: {
              description: "Lobby users returned.",
            },
            401: {
              description: "Server auth is invalid.",
            },
          },
        },
      },
      "/api/telemetry/lobby": {
        post: {
          tags: ["Telemetry"],
          summary: "Update lobby telemetry.",
          security: [
            {
              serverAuth: [],
            },
          ],
          requestBody: {
            required: true,
            content: {
              "application/json": {
                schema: {
                  $ref: "#/components/schemas/LobbyTelemetryRequest",
                },
              },
            },
          },
          responses: {
            200: {
              description: "Lobby telemetry updated.",
            },
            400: {
              description: "Valid currentClientCount is required.",
            },
            401: {
              description: "Server auth is invalid.",
            },
          },
        },
      },
      "/api/users": {
        get: {
          tags: ["Users"],
          summary: "Get users.",
          parameters: [
            {
              name: "limit",
              in: "query",
              required: false,
              schema: {
                type: "integer",
                default: 20,
                minimum: 1,
                maximum: 100,
              },
            },
          ],
          responses: {
            200: {
              description: "Users returned.",
            },
            500: {
              description: "Failed to query Users table.",
            },
          },
        },
      },
      "/api/users/{ID}": {
        get: {
          tags: ["Users"],
          summary: "Get a user by login ID.",
          parameters: [
            {
              name: "ID",
              in: "path",
              required: true,
              schema: {
                type: "string",
              },
            },
          ],
          responses: {
            200: {
              description: "User returned.",
            },
            404: {
              description: "User not found.",
            },
            500: {
              description: "Failed to query Users table.",
            },
          },
        },
      },
    },
  },
  apis: [],
};

const swaggerSpec = swaggerJSDoc(swaggerOptions);

module.exports = {
  swaggerSpec,
};
