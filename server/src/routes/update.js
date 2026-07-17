const fs = require('fs');
const path = require('path');
const crypto = require('crypto');
const express = require('express');

const router = express.Router();

// Directory holding the published build + its manifest. Override with
// UPDATE_DIR (absolute) if the binary is mounted elsewhere on Railway.
const UPDATE_DIR = process.env.UPDATE_DIR
  ? process.env.UPDATE_DIR
  : path.join(__dirname, '..', '..', 'updates');

const DLL_PATH = path.join(UPDATE_DIR, 'RUGIR.dll');
const MANIFEST_PATH = path.join(UPDATE_DIR, 'manifest.json');

// SHA-256 is cached and only recomputed when the file's size/mtime changes, so
// we never re-hash 8 MB on every poll while staying correct after a redeploy.
let hashCache = { key: '', hash: '' };

function readVersion() {
  try {
    const raw = fs.readFileSync(MANIFEST_PATH, 'utf8');
    const parsed = JSON.parse(raw);
    return typeof parsed.version === 'string' ? parsed.version : null;
  } catch (err) {
    return null;
  }
}

function computeHash() {
  const st = fs.statSync(DLL_PATH);
  const key = `${st.size}:${st.mtimeMs}`;
  if (hashCache.key === key && hashCache.hash) {
    return hashCache.hash;
  }

  const buf = fs.readFileSync(DLL_PATH);
  const hash = crypto.createHash('sha256').update(buf).digest('hex');
  hashCache = { key, hash };
  return hash;
}

// GET /api/update/manifest -> { version, hash, file }
router.get('/manifest', (req, res) => {
  const version = readVersion();
  if (!version || !fs.existsSync(DLL_PATH)) {
    return res.status(503).json({ ok: false, error: 'update_unavailable' });
  }

  let hash;
  try {
    hash = computeHash();
  } catch (err) {
    return res.status(503).json({ ok: false, error: 'update_unavailable' });
  }

  return res.json({
    version,
    hash,
    file: '/api/update/download'
  });
});

// GET /api/update/download -> raw DLL bytes with Content-Length (drives the
// client progress bar) and no-cache so a redeploy is always picked up.
router.get('/download', (req, res) => {
  if (!fs.existsSync(DLL_PATH)) {
    return res.status(404).json({ ok: false, error: 'update_unavailable' });
  }

  let size;
  try {
    size = fs.statSync(DLL_PATH).size;
  } catch (err) {
    return res.status(503).json({ ok: false, error: 'update_unavailable' });
  }

  res.setHeader('Content-Type', 'application/octet-stream');
  res.setHeader('Content-Length', size);
  res.setHeader('Content-Disposition', 'attachment; filename="RUGIR.dll"');
  res.setHeader('Cache-Control', 'no-store');

  const stream = fs.createReadStream(DLL_PATH);
  stream.on('error', () => {
    if (!res.headersSent) {
      res.status(500).end();
    } else {
      res.destroy();
    }
  });
  stream.pipe(res);
});

module.exports = router;
