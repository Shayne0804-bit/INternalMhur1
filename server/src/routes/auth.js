const express = require('express');
const jwt = require('jsonwebtoken');
const config = require('../config/env');
const AuthLog = require('../models/AuthLog');
const License = require('../models/License');
const { hashHwid, hashLicenseKey, normalizeLicenseKey } = require('../utils/licenseKeys');

const router = express.Router();

async function writeAuthLog(req, fields) {
  try {
    await AuthLog.create({
      ...fields,
      ip: req.ip,
      userAgent: req.get('user-agent') || null
    });
  } catch (err) {
    // Auth must not fail because logging failed.
  }
}

function createSessionToken(license, hwidHash) {
  return jwt.sign(
    {
      tier: license.tier,
      hwid: hwidHash.slice(0, 16)
    },
    config.jwtSecret,
    {
      subject: license._id.toString(),
      issuer: 'rugir-auth',
      audience: 'rugir-dll',
      expiresIn: config.tokenTtl
    }
  );
}

router.post('/verify', async (req, res, next) => {
  try {
    const key = normalizeLicenseKey(req.body.key);
    const hwid = String(req.body.hwid || '').trim();
    const version = String(req.body.version || '').trim().slice(0, 64);

    if (!key || !hwid) {
      await writeAuthLog(req, {
        ok: false,
        reason: 'missing_key_or_hwid',
        version
      });

      return res.status(400).json({
        ok: false,
        error: 'missing_key_or_hwid'
      });
    }

    const keyHash = hashLicenseKey(key);
    const hwidHash = hashHwid(hwid);
    const license = await License.findOne({ keyHash });

    if (!license) {
      await writeAuthLog(req, {
        keyHash,
        hwidHash,
        ok: false,
        reason: 'invalid_key',
        version
      });

      return res.status(401).json({
        ok: false,
        error: 'invalid_key'
      });
    }

    if (license.status !== 'active') {
      await writeAuthLog(req, {
        licenseId: license._id,
        keyHash,
        hwidHash,
        ok: false,
        reason: `license_${license.status}`,
        version
      });

      return res.status(403).json({
        ok: false,
        error: `license_${license.status}`
      });
    }

    if (license.isExpired()) {
      license.status = 'expired';
      await license.save();
      await writeAuthLog(req, {
        licenseId: license._id,
        keyHash,
        hwidHash,
        ok: false,
        reason: 'license_expired',
        version
      });

      return res.status(403).json({
        ok: false,
        error: 'license_expired'
      });
    }

    if (!license.hwidHashes.includes(hwidHash)) {
      if (license.hwidHashes.length >= license.maxActivations) {
        await writeAuthLog(req, {
          licenseId: license._id,
          keyHash,
          hwidHash,
          ok: false,
          reason: 'hwid_limit_reached',
          version
        });

        return res.status(403).json({
          ok: false,
          error: 'hwid_limit_reached'
        });
      }

      license.hwidHashes.push(hwidHash);
    }

    license.lastSeenAt = new Date();
    license.lastVersion = version || null;
    license.lastIp = req.ip;
    await license.save();

    const token = createSessionToken(license, hwidHash);
    await writeAuthLog(req, {
      licenseId: license._id,
      keyHash,
      hwidHash,
      ok: true,
      reason: 'ok',
      version
    });

    return res.json({
      ok: true,
      tier: license.tier,
      expiresAt: license.expiresAt,
      token,
      serverTime: new Date().toISOString()
    });
  } catch (err) {
    return next(err);
  }
});

module.exports = router;
