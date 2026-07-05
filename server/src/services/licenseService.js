const License = require('../models/License');
const { generateLicenseKey, hashLicenseKey } = require('../utils/licenseKeys');

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

module.exports = { createLicense };
