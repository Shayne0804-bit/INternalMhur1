const express = require('express');
const License = require('../models/License');
const { adminAuth } = require('../middleware/adminAuth');
const { generateLicenseKey, hashLicenseKey } = require('../utils/licenseKeys');

const router = express.Router();

router.use(adminAuth);

function parseExpiry(body) {
  if (body.expiresAt) {
    const expiresAt = new Date(body.expiresAt);
    if (Number.isNaN(expiresAt.getTime())) {
      const error = new Error('invalid_expires_at');
      error.statusCode = 400;
      throw error;
    }

    return expiresAt;
  }

  if (body.daysValid) {
    const daysValid = Number(body.daysValid);
    if (!Number.isFinite(daysValid) || daysValid <= 0) {
      const error = new Error('invalid_days_valid');
      error.statusCode = 400;
      throw error;
    }

    return new Date(Date.now() + daysValid * 24 * 60 * 60 * 1000);
  }

  return null;
}

router.post('/licenses', async (req, res, next) => {
  try {
    const tier = req.body.tier || 'premium';
    const maxActivations = Number(req.body.maxActivations || 1);

    if (!['trial', 'premium', 'lifetime'].includes(tier)) {
      return res.status(400).json({ ok: false, error: 'invalid_tier' });
    }

    if (!Number.isInteger(maxActivations) || maxActivations < 1 || maxActivations > 10) {
      return res.status(400).json({ ok: false, error: 'invalid_max_activations' });
    }

    const plainKey = generateLicenseKey(req.body.prefix || 'RUGIR');
    const license = await License.create({
      keyHash: hashLicenseKey(plainKey),
      tier,
      maxActivations,
      expiresAt: tier === 'lifetime' ? null : parseExpiry(req.body),
      notes: req.body.notes || ''
    });

    return res.status(201).json({
      ok: true,
      key: plainKey,
      license: license.toAdminJSON()
    });
  } catch (err) {
    return next(err);
  }
});

router.get('/licenses', async (req, res, next) => {
  try {
    const licenses = await License.find()
      .sort({ createdAt: -1 })
      .limit(200);

    return res.json({
      ok: true,
      licenses: licenses.map((license) => license.toAdminJSON())
    });
  } catch (err) {
    return next(err);
  }
});

router.post('/licenses/:id/revoke', async (req, res, next) => {
  try {
    const license = await License.findByIdAndUpdate(
      req.params.id,
      { status: 'revoked' },
      { new: true }
    );

    if (!license) {
      return res.status(404).json({ ok: false, error: 'license_not_found' });
    }

    return res.json({
      ok: true,
      license: license.toAdminJSON()
    });
  } catch (err) {
    return next(err);
  }
});

router.post('/licenses/:id/reset-hwid', async (req, res, next) => {
  try {
    const license = await License.findByIdAndUpdate(
      req.params.id,
      { hwidHashes: [] },
      { new: true }
    );

    if (!license) {
      return res.status(404).json({ ok: false, error: 'license_not_found' });
    }

    return res.json({
      ok: true,
      license: license.toAdminJSON()
    });
  } catch (err) {
    return next(err);
  }
});

module.exports = router;
