const crypto = require('crypto');
const config = require('../config/env');

function timingSafeStringEqual(left, right) {
  const leftBuffer = Buffer.from(String(left || ''), 'utf8');
  const rightBuffer = Buffer.from(String(right || ''), 'utf8');

  if (leftBuffer.length !== rightBuffer.length) {
    return false;
  }

  return crypto.timingSafeEqual(leftBuffer, rightBuffer);
}

function adminAuth(req, res, next) {
  const token = req.get('x-admin-token');
  if (!token || !timingSafeStringEqual(token, config.adminToken)) {
    return res.status(401).json({
      ok: false,
      error: 'unauthorized'
    });
  }

  return next();
}

module.exports = {
  adminAuth
};
