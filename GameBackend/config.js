const fs = require("fs");
const path = require("path");

const configPath = path.join(__dirname, "config.json");
const serverRuntimeConfigPath = path.join(path.resolve(__dirname, ".."), "ServerRuntime.ini");

function trimRuntimeValue(value) {
  const trimmedValue = String(value ?? "").trim();
  if (trimmedValue.length >= 2 && trimmedValue.startsWith("\"") && trimmedValue.endsWith("\"")) {
    return trimmedValue.slice(1, -1).trim();
  }

  return trimmedValue;
}

function loadServerRuntimeHost() {
  if (!fs.existsSync(serverRuntimeConfigPath)) {
    return "";
  }

  const rawConfig = fs.readFileSync(serverRuntimeConfigPath, "utf8");
  let currentSection = "";

  for (const rawLine of rawConfig.split(/\r?\n/)) {
    const line = rawLine.replace(/^\uFEFF/, "").trim();
    if (!line || line.startsWith(";") || line.startsWith("#")) {
      continue;
    }

    const sectionMatch = line.match(/^\[([^\]]+)\]$/);
    if (sectionMatch) {
      currentSection = sectionMatch[1].trim();
      continue;
    }

    if (currentSection !== "ProjectP.Server") {
      continue;
    }

    const equalsIndex = line.indexOf("=");
    if (equalsIndex <= 0) {
      continue;
    }

    const key = line.slice(0, equalsIndex).trim();
    if (key !== "ServerHost") {
      continue;
    }

    return trimRuntimeValue(line.slice(equalsIndex + 1));
  }

  return "";
}

function applyServerRuntimeOverrides(parsedConfig) {
  const serverHost = loadServerRuntimeHost();
  if (!serverHost) {
    return parsedConfig;
  }

  parsedConfig.dungeonManager = parsedConfig.dungeonManager ?? {};
  parsedConfig.monitor = parsedConfig.monitor ?? {};
  parsedConfig.dungeonManager.publicHost = serverHost;
  parsedConfig.monitor.lobbyHost = serverHost;

  return parsedConfig;
}

function loadConfig() {
  if (!fs.existsSync(configPath)) {
    throw new Error(
      `Missing config file: ${configPath}. Copy GameBackend/config.example.json to GameBackend/config.json and fill in your local MySQL settings.`,
    );
  }

  const rawConfig = fs.readFileSync(configPath, "utf8");
  const parsedConfig = JSON.parse(rawConfig);

  if (!parsedConfig.server || !parsedConfig.database || !parsedConfig.serverAuth) {
    throw new Error("config.json must define 'server', 'database', and 'serverAuth' sections.");
  }

  if (typeof parsedConfig.serverAuth.dungeonStateKey !== "string" || !parsedConfig.serverAuth.dungeonStateKey.trim()) {
    throw new Error("config.json must define 'serverAuth.dungeonStateKey'.");
  }

  return applyServerRuntimeOverrides(parsedConfig);
}

module.exports = {
  loadConfig,
};
