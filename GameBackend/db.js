const mysql = require("mysql2/promise");
const argon2 = require("argon2");
const crypto = require("crypto");

const LOGIN_TOKEN_TTL_HOURS = 24;
const ACTIVE_LOGIN_GRACE_SECONDS = 120;

function hashLoginToken(token) {
  return crypto.createHash("sha256").update(token, "utf8").digest("hex");
}

function createDatabase(poolConfig) {
  return mysql.createPool({
    host: poolConfig.host,
    port: poolConfig.port,
    user: poolConfig.user,
    password: poolConfig.password,
    database: poolConfig.database,
    connectionLimit: poolConfig.connectionLimit ?? 10,
    waitForConnections: true,
    queueLimit: 0,
    charset: "utf8mb4",
  });
}

async function testConnection(pool) {
  const [rows] = await pool.query("SELECT 1 AS ok");
  return rows[0];
}

async function fetchUsers(pool, limit = 20) {
  const safeLimit = Number.isInteger(limit) ? Math.max(1, Math.min(limit, 100)) : 20;
  const [rows] = await pool.query(
    `
      SELECT
        user_Index,
        ID,
        username,
        status,
        created_at,
        last_login_at
      FROM users
      ORDER BY user_Index ASC
      LIMIT ?
    `,
    [safeLimit],
  );

  return rows;
}

async function fetchUserByID(pool, ID) {
  const [rows] = await pool.query(
    `
      SELECT
        user_Index,
        ID,
        username,
        status,
        created_at,
        last_login_at
      FROM users
      WHERE ID = ?
      LIMIT 1
    `,
    [ID],
  );

  return rows[0] ?? null;
}

async function fetchUserByIndex(pool, userIndex) {
  const [rows] = await pool.query(
    `
      SELECT
        user_Index,
        ID,
        username,
        status,
        created_at,
        last_login_at
      FROM users
      WHERE user_Index = ?
        AND status = 1
      LIMIT 1
    `,
    [userIndex],
  );

  return rows[0] ?? null;
}

async function authenticateUser(pool, ID, password) {
  const [rows] = await pool.query(
    `
      SELECT
        user_Index,
        ID,
        username,
        password_hash,
        status
      FROM users
      WHERE ID = ?
      LIMIT 1
    `,
    [ID],
  );

  const user = rows[0];
  if (!user || user.status !== 1) {
    return null;
  }

  let passwordMatches = false;
  try {
    passwordMatches = await argon2.verify(user.password_hash, password);
  } catch {
    passwordMatches = false;
  }

  if (!passwordMatches) {
    return null;
  }

  await pool.query(
    `
      UPDATE users
      SET last_login_at = CURRENT_TIMESTAMP
      WHERE user_Index = ?
    `,
    [user.user_Index],
  );

  return {
    user_Index: user.user_Index,
    ID: user.ID,
    username: user.username,
    status: user.status,
  };
}

async function hasActiveLoginToken(pool, userIndex) {
  const [rows] = await pool.query(
    `
      SELECT token_id
      FROM login_tokens
      WHERE user_Index = ?
        AND revoked_at IS NULL
        AND expires_at > CURRENT_TIMESTAMP
        AND COALESCE(last_seen_at, created_at) > DATE_SUB(CURRENT_TIMESTAMP, INTERVAL ? SECOND)
      LIMIT 1
    `,
    [userIndex, ACTIVE_LOGIN_GRACE_SECONDS],
  );

  return rows.length > 0;
}

async function createLoginToken(pool, userIndex, ipAddress = null) {
  const token = crypto.randomBytes(32).toString("hex");
  const tokenHash = hashLoginToken(token);

  await pool.query(
    `
      INSERT INTO login_tokens (user_Index, token_hash, ip_address, last_seen_at, expires_at)
      VALUES (?, ?, ?, CURRENT_TIMESTAMP, DATE_ADD(CURRENT_TIMESTAMP, INTERVAL ? HOUR))
      ON DUPLICATE KEY UPDATE
        token_hash = VALUES(token_hash),
        ip_address = VALUES(ip_address),
        created_at = CURRENT_TIMESTAMP,
        last_seen_at = CURRENT_TIMESTAMP,
        expires_at = VALUES(expires_at),
        revoked_at = NULL
    `,
    [userIndex, tokenHash, ipAddress, LOGIN_TOKEN_TTL_HOURS],
  );

  return token;
}

async function touchLoginToken(pool, token) {
  const tokenHash = hashLoginToken(token);
  const [result] = await pool.query(
    `
      UPDATE login_tokens
      SET last_seen_at = CURRENT_TIMESTAMP
      WHERE token_hash = ?
        AND revoked_at IS NULL
        AND expires_at > CURRENT_TIMESTAMP
    `,
    [tokenHash],
  );

  return result.affectedRows > 0;
}

async function verifyLoginToken(pool, token) {
  const tokenHash = hashLoginToken(token);
  const [rows] = await pool.query(
    `
      SELECT
        users.user_Index,
        users.ID,
        users.username,
        users.status
      FROM login_tokens
      INNER JOIN users ON users.user_Index = login_tokens.user_Index
      WHERE login_tokens.token_hash = ?
        AND login_tokens.revoked_at IS NULL
        AND login_tokens.expires_at > CURRENT_TIMESTAMP
        AND users.status = 1
      LIMIT 1
    `,
    [tokenHash],
  );

  const user = rows[0];
  if (!user) {
    return null;
  }

  await pool.query(
    `
      UPDATE login_tokens
      SET last_seen_at = CURRENT_TIMESTAMP
      WHERE token_hash = ?
    `,
    [tokenHash],
  );

  return {
    user_Index: user.user_Index,
    ID: user.ID,
    username: user.username,
    status: user.status,
  };
}

async function revokeLoginToken(pool, token) {
  const tokenHash = hashLoginToken(token);
  const [result] = await pool.query(
    `
      UPDATE login_tokens
      SET revoked_at = CURRENT_TIMESTAMP
      WHERE token_hash = ?
        AND revoked_at IS NULL
    `,
    [tokenHash],
  );

  return result.affectedRows > 0;
}

async function revokeActiveLoginTokensForUser(pool, userIndex) {
  const [result] = await pool.query(
    `
      UPDATE login_tokens
      SET revoked_at = CURRENT_TIMESTAMP
      WHERE user_Index = ?
        AND revoked_at IS NULL
        AND expires_at > CURRENT_TIMESTAMP
    `,
    [userIndex],
  );

  return result.affectedRows;
}

async function createUser(pool, ID, username, password) {
  const passwordHash = await argon2.hash(password);
  const [result] = await pool.query(
    `
      INSERT INTO users (ID, username, password_hash)
      VALUES (?, ?, ?)
    `,
    [ID, username, passwordHash],
  );

  return {
    user_Index: result.insertId,
    ID,
    username,
    status: 1,
  };
}

module.exports = {
  authenticateUser,
  createLoginToken,
  createUser,
  createDatabase,
  fetchUserByID,
  fetchUserByIndex,
  fetchUsers,
  hasActiveLoginToken,
  revokeActiveLoginTokensForUser,
  revokeLoginToken,
  testConnection,
  touchLoginToken,
  verifyLoginToken,
};
