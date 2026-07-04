const crypto = require('crypto');
const config = require('../config/env');

const LICENSE_ALPHABET = 'ABCDEFGHJKLMNPQRSTUVWXYZ23456789';

function normalizeLicenseKey(value) {
  return String(value || '')
    .trim()
    .toUpperCase()
    .replace(/[^A-Z0-9]/g, '');
}

function hmacSha256(value) {
  return crypto
    .createHmac('sha256', config.licenseKeyPepper)
    .update(String(value), 'utf8')
    .digest('hex');
}

function hashLicenseKey(key) {
  return hmacSha256(`license:${normalizeLicenseKey(key)}`);
}

function hashHwid(hwid) {
  return hmacSha256(`hwid:${String(hwid || '').trim().toLowerCase()}`);
}

function randomLicenseChunk(length) {
  const bytes = crypto.randomBytes(length);
  let chunk = '';

  for (let i = 0; i < length; i += 1) {
    chunk += LICENSE_ALPHABET[bytes[i] % LICENSE_ALPHABET.length];
  }

  return chunk;
}

function generateLicenseKey(prefix = 'RUGIR', groups = 5, groupLength = 5) {
  const normalizedPrefix = String(prefix || 'RUGIR')
    .toUpperCase()
    .replace(/[^A-Z0-9]/g, '')
    .slice(0, 12) || 'RUGIR';

  const chunks = [];
  for (let i = 0; i < groups; i += 1) {
    chunks.push(randomLicenseChunk(groupLength));
  }

  return `${normalizedPrefix}-${chunks.join('-')}`;
}

module.exports = {
  generateLicenseKey,
  hashHwid,
  hashLicenseKey,
  normalizeLicenseKey
};
