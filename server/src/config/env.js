const dotenv = require('dotenv');

dotenv.config();

function readEnv(name, fallback = undefined) {
  const value = process.env[name];
  if (value === undefined || value === '') {
    return fallback;
  }

  return value;
}

function readRequiredEnv(name) {
  const value = readEnv(name);
  if (!value) {
    throw new Error(`Missing required environment variable: ${name}`);
  }

  return value;
}

function parseAllowedOrigins(value) {
  if (!value) {
    return [];
  }

  return value
    .split(',')
    .map((origin) => origin.trim())
    .filter(Boolean);
}

module.exports = {
  nodeEnv: readEnv('NODE_ENV', 'development'),
  port: Number(readEnv('PORT', '3000')),
  mongoUri: readRequiredEnv('MONGODB_URI'),
  jwtSecret: readRequiredEnv('JWT_SECRET'),
  licenseKeyPepper: readRequiredEnv('LICENSE_KEY_PEPPER'),
  adminToken: readRequiredEnv('ADMIN_TOKEN'),
  tokenTtl: readEnv('TOKEN_TTL', '15m'),
  allowedOrigins: parseAllowedOrigins(readEnv('ALLOWED_ORIGINS', ''))
};
