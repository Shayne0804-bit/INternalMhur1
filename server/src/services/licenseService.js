const License = require('../models/License');
const { generateLicenseKey, hashLicenseKey, normalizeLicenseKey } = require('../utils/licenseKeys');

function badRequest(message) {
  const err = new Error(message);
  err.statusCode = 400;
  return err;
}

function resolveExpiry({ tier, expiresAt, daysValid }) {
  if (tier === 'lifetime') {
    return null;
  }

  if (expiresAt) {
    const date = new Date(expiresAt);
    if (Number.isNaN(date.getTime())) {
      throw badRequest('invalid_expires_at');
    }
    return date;
  }

  if (daysValid !== undefined && daysValid !== null && daysValid !== '') {
    const days = Number(daysValid);
    if (!Number.isFinite(days) || days <= 0) {
      throw badRequest('invalid_days_valid');
    }
    return new Date(Date.now() + days * 24 * 60 * 60 * 1000);
  }

  return null;
}

// Single source of truth for license creation, shared by the admin HTTP route
// and the Discord bot (same process, same DB).
async function createLicense({
  tier = 'premium',
  maxActivations = 1,
  daysValid,
  expiresAt,
  notes = '',
  prefix = 'RUGIR'
} = {}) {
  if (!['trial', 'premium', 'lifetime'].includes(tier)) {
    throw badRequest('invalid_tier');
  }

  const activations = Number(maxActivations);
  if (!Number.isInteger(activations) || activations < 1 || activations > 10) {
    throw badRequest('invalid_max_activations');
  }

  const plainKey = generateLicenseKey(prefix);
  const license = await License.create({
    keyHash: hashLicenseKey(plainKey),
    tier,
    maxActivations: activations,
    expiresAt: resolveExpiry({ tier, expiresAt, daysValid }),
    notes: notes || ''
  });

  return { key: plainKey, license };
}

// Look up a license from a plain key (as typed by a member).
async function findLicenseByKey(key) {
  const normalized = normalizeLicenseKey(key);
  if (!normalized) {
    return null;
  }
  return License.findOne({ keyHash: hashLicenseKey(normalized) });
}

// Bind (or re-bind) the Discord identity that verified this key.
async function bindDiscordUser(license, userId, username) {
  license.discordUserId = userId;
  license.discordUsername = username;
  await license.save();
  return license;
}

async function getLicenseById(licenseId) {
  return License.findById(licenseId);
}

// Clear the bound HWIDs so the key can be activated on a new machine.
async function resetLicenseHwid(licenseId) {
  return License.findByIdAndUpdate(licenseId, { hwidHashes: [] }, { new: true });
}

module.exports = {
  createLicense,
  findLicenseByKey,
  getLicenseById,
  bindDiscordUser,
  resetLicenseHwid
};
