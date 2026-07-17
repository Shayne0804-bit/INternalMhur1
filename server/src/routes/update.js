const fs = require('fs');
const path = require('path');
const crypto = require('crypto');
const express = require('express');

const router = express.Router();

// Directory holding published builds + resolution index. Override with
// UPDATE_DIR (absolute) if the payload is mounted elsewhere on Railway.
const UPDATE_DIR = process.env.UPDATE_DIR
  ? process.env.UPDATE_DIR
  : path.join(__dirname, '..', '..', 'updates');

// Legacy single payload (interactive cheat-update path, game unchanged). Kept
// for backward-compat and maintained by tools/publish_update.ps1.
const DLL_PATH = path.join(UPDATE_DIR, 'RUGIR.dll');
const MANIFEST_PATH = path.join(UPDATE_DIR, 'manifest.json');

// Multi-binary resolution table (option B): builds keyed by config + gameBuild.
//   updates/index.json:
//   { "builds": [ { "config":"agency", "gameBuild":"0x66A1B2C3",
//                   "version":"1.0.2", "file":"agency/0x66A1B2C3/RUGIR.dll" } ] }
const INDEX_PATH = path.join(UPDATE_DIR, 'index.json');

// SHA-256 cache keyed on absolute path + size + mtime, so we never re-hash an
// 8 MB DLL on every poll but stay correct after a redeploy.
const hashCache = new Map();

function computeHash(absPath) {
  const st = fs.statSync(absPath);
  const key = `${absPath}:${st.size}:${st.mtimeMs}`;
  const cached = hashCache.get(key);
  if (cached) return cached;

  const buf = fs.readFileSync(absPath);
  const hash = crypto.createHash('sha256').update(buf).digest('hex');
  hashCache.set(key, hash);
  return hash;
}

function readVersion() {
  try {
    const parsed = JSON.parse(fs.readFileSync(MANIFEST_PATH, 'utf8'));
    return typeof parsed.version === 'string' ? parsed.version : null;
  } catch (err) {
    return null;
  }
}

function readIndex() {
  try {
    const parsed = JSON.parse(fs.readFileSync(INDEX_PATH, 'utf8'));
    return Array.isArray(parsed.builds) ? parsed.builds : [];
  } catch (err) {
    return [];
  }
}

// Case-insensitive gameBuild match (0x66A1B2C3). Config exact.
function findBuild(config, gameBuild) {
  if (!config || !gameBuild) return null;
  const g = String(gameBuild).toLowerCase();
  const c = String(config).toLowerCase();
  return readIndex().find(
    (b) => String(b.config).toLowerCase() === c &&
           String(b.gameBuild).toLowerCase() === g &&
           typeof b.file === 'string'
  ) || null;
}

// Resolve a build entry's DLL to an absolute path, guarding against traversal.
function resolveBuildFile(entry) {
  const abs = path.resolve(UPDATE_DIR, entry.file);
  if (!abs.startsWith(path.resolve(UPDATE_DIR))) return null; // traversal guard
  if (!fs.existsSync(abs)) return null;
  return abs;
}

// GET /api/update/manifest?config=&client=&game=
//   - with game: option B resolution. 200 {version,hash,file} or 204 (no build).
//   - without game: legacy single payload (interactive cheat-update path).
router.get('/manifest', (req, res) => {
  const { config, game } = req.query;

  if (game) {
    const entry = findBuild(config, game);
    if (!entry) {
      // Server reachable but no compatible build for this (config, game) yet.
      return res.status(204).end();
    }
    const abs = resolveBuildFile(entry);
    if (!abs) {
      return res.status(204).end();
    }
    let hash;
    try {
      hash = computeHash(abs);
    } catch (err) {
      return res.status(204).end();
    }
    return res.json({
      version: entry.version || '',
      hash,
      file: `/api/update/download?config=${encodeURIComponent(config)}&game=${encodeURIComponent(game)}`
    });
  }

  // Legacy path (no game key): single current payload.
  const version = readVersion();
  if (!version || !fs.existsSync(DLL_PATH)) {
    return res.status(503).json({ ok: false, error: 'update_unavailable' });
  }
  let hash;
  try {
    hash = computeHash(DLL_PATH);
  } catch (err) {
    return res.status(503).json({ ok: false, error: 'update_unavailable' });
  }
  return res.json({ version, hash, file: '/api/update/download' });
});

// GET /api/update/download?config=&game=
//   - with game: streams the (config, game)-resolved binary.
//   - without game: streams the legacy single payload.
router.get('/download', (req, res) => {
  const { config, game } = req.query;

  let dllPath = DLL_PATH;
  if (game) {
    const entry = findBuild(config, game);
    const abs = entry ? resolveBuildFile(entry) : null;
    if (!abs) {
      return res.status(404).json({ ok: false, error: 'update_unavailable' });
    }
    dllPath = abs;
  }

  if (!fs.existsSync(dllPath)) {
    return res.status(404).json({ ok: false, error: 'update_unavailable' });
  }

  let size;
  try {
    size = fs.statSync(dllPath).size;
  } catch (err) {
    return res.status(503).json({ ok: false, error: 'update_unavailable' });
  }

  res.setHeader('Content-Type', 'application/octet-stream');
  res.setHeader('Content-Length', size);
  res.setHeader('Content-Disposition', 'attachment; filename="RUGIR.dll"');
  res.setHeader('Cache-Control', 'no-store');

  const stream = fs.createReadStream(dllPath);
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
