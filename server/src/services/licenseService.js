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
    keyPlain: plainKey,
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

// Find the license currently bound to a Discord user (1 user -> 1 license).
async function findLicenseByDiscordUser(userId) {
  if (!userId) {
    return null;
  }
  return License.findOne({ discordUserId: userId });
}

// Bind (or re-bind) the Discord identity that verified this key. Exclusive:
// detaches the user from any other license they were previously bound to.
async function bindDiscordUser(license, userId, username, keyPlain) {
  await License.updateMany(
    { discordUserId: userId, _id: { $ne: license._id } },
    { $set: { discordUserId: null, discordUsername: null } }
  );
  license.discordUserId = userId;
  license.discordUsername = username;
  // Backfill the plain key for licenses created before this field existed.
  if (!license.keyPlain && keyPlain) {
    license.keyPlain = keyPlain;
  }
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

// Revoke a license (immediately invalid, keeps the record).
async function revokeLicense(licenseId) {
  return License.findByIdAndUpdate(licenseId, { status: 'revoked' }, { new: true });
}

// Extend (or shorten) a license by a number of days. A lifetime license
// (expiresAt null) stays lifetime. Reactivates an expired license if the new
// date is in the future. Returns the updated license or null.
async function extendLicense(licenseId, days) {
  const license = await License.findById(licenseId);
  if (!license) return null;
  if (license.expiresAt === null && license.tier === 'lifetime') {
    return license; // lifetime: nothing to extend
  }
  const base = license.expiresAt && license.expiresAt > new Date()
    ? license.expiresAt.getTime()
    : Date.now();
  license.expiresAt = new Date(base + days * 24 * 60 * 60 * 1000);
  if (license.status === 'expired' && license.expiresAt > new Date()) {
    license.status = 'active';
  }
  await license.save();
  return license;
}

// Resolve a license from a plain key OR a Discord user id (owner lookup helper).
async function findLicenseByKeyOrUser(query) {
  const byKey = await findLicenseByKey(query);
  if (byKey) return byKey;
  return findLicenseByDiscordUser(query);
}

// List licenses, newest first (owner overview). Optional status filter.
async function listLicenses({ status, limit = 20 } = {}) {
  const filter = status ? { status } : {};
  return License.find(filter).sort({ createdAt: -1 }).limit(limit);
}

// Aggregate counts for the /stats dashboard.
async function getLicenseStats() {
  const now = new Date();
  const startOfMonth = new Date(now.getFullYear(), now.getMonth(), 1);

  const [total, active, revoked, expiredFlag, lifetime, newThisMonth, all] = await Promise.all([
    License.countDocuments({}),
    License.countDocuments({ status: 'active' }),
    License.countDocuments({ status: 'revoked' }),
    License.countDocuments({ status: 'expired' }),
    License.countDocuments({ tier: 'lifetime' }),
    License.countDocuments({ createdAt: { $gte: startOfMonth } }),
    License.find({}, 'hwidHashes expiresAt status').lean()
  ]);

  // Active-but-past-expiry (status not yet swept) + total activations.
  let expiredByDate = 0;
  let activations = 0;
  for (const l of all) {
    activations += (l.hwidHashes ? l.hwidHashes.length : 0);
    if (l.status === 'active' && l.expiresAt && new Date(l.expiresAt) <= now) {
      expiredByDate += 1;
    }
  }

  return {
    total,
    active,
    revoked,
    expired: expiredFlag + expiredByDate,
    lifetime,
    newThisMonth,
    activations,
    boundToDiscord: await License.countDocuments({ discordUserId: { $ne: null } })
  };
}

// Licenses expiring within `days` from now (for reminders), still active,
// bound to a Discord user, and not yet reminded for this window.
async function findLicensesExpiringWithin(days, reminderField) {
  const now = new Date();
  const horizon = new Date(now.getTime() + days * 24 * 60 * 60 * 1000);
  return License.find({
    status: 'active',
    discordUserId: { $ne: null },
    expiresAt: { $ne: null, $gt: now, $lte: horizon },
    [reminderField]: { $ne: true }
  });
}

// Sweep active licenses whose expiry has passed and flag them expired.
// Returns the licenses that were just expired (with discordUserId to strip roles).
async function sweepExpiredLicenses() {
  const now = new Date();
  const toExpire = await License.find({
    status: 'active',
    expiresAt: { $ne: null, $lte: now }
  });
  if (toExpire.length) {
    await License.updateMany(
      { _id: { $in: toExpire.map((l) => l._id) } },
      { $set: { status: 'expired' } }
    );
  }
  return toExpire;
}

module.exports = {
  createLicense,
  findLicenseByKey,
  findLicenseByDiscordUser,
  getLicenseById,
  bindDiscordUser,
  resetLicenseHwid,
  revokeLicense,
  extendLicense,
  findLicenseByKeyOrUser,
  listLicenses,
  getLicenseStats,
  findLicensesExpiringWithin,
  sweepExpiredLicenses
};
