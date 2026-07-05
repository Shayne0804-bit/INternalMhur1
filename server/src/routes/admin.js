const express = require('express');
const License = require('../models/License');
const { adminAuth } = require('../middleware/adminAuth');
const { createLicense } = require('../services/licenseService');

const router = express.Router();

router.use(adminAuth);

router.post('/licenses', async (req, res, next) => {
  try {
    const { key, license } = await createLicense({
      tier: req.body.tier || 'premium',
      maxActivations: req.body.maxActivations !== undefined ? req.body.maxActivations : 1,
      daysValid: req.body.daysValid,
      expiresAt: req.body.expiresAt,
      notes: req.body.notes || '',
      prefix: req.body.prefix || 'RUGIR'
    });

    return res.status(201).json({
      ok: true,
      key,
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
