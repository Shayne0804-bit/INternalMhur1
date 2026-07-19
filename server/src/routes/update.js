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

// Staged-rollout chain (legacy cheat-update path). Lets the server serve the
// NEXT hop for a client version instead of always the latest, so users climb
// one release at a time (e.g. 1.0.22 -> 1.0.23 -> 1.0.24).
//   updates/chain.json:
//   {
//     "default": "1.0.23",                         // served to old/unknown clients
//     "latest":  "1.0.24",
//     "files": {
//       "1.0.23": { "file": "RUGIR.dll",        "notes": "Updater improved" },
//       "1.0.24": { "file": "1.0.24/RUGIR.dll", "notes": "Ability hack crash fixed" }
//     },
//     "hops": { "1.0.23": "1.0.24" }               // client 1.0.23 -> 1.0.24
//   }
const CHAIN_PATH = path.join(UPDATE_DIR, 'chain.json');

function readChain() {
  try {
    const parsed = JSON.parse(fs.readFileSync(CHAIN_PATH, 'utf8'));
    if (parsed && parsed.files && typeof parsed.files === 'object') return parsed;
    return null;
  } catch (err) {
    return null;
  }
}

// Resolve which version to serve a legacy client, given its reported version.
// Returns { version, file (abs), notes } or null if the chain can't resolve.
function resolveChainTarget(clientVersion) {
  const chain = readChain();
  if (!chain) return null;

  const hops = chain.hops || {};
  const files = chain.files || {};
  const c = clientVersion ? String(clientVersion) : '';

  let target;
  if (c && Object.prototype.hasOwnProperty.call(hops, c)) {
    target = hops[c];                 // explicit next hop for this version
  } else if (c && c === chain.latest) {
    target = c;                       // already latest -> report same (client idles)
  } else {
    target = chain.default;           // old / unknown / no-version client
  }

  const entry = files[target];
  if (!entry || typeof entry.file !== 'string') return null;

  const abs = path.resolve(UPDATE_DIR, entry.file);
  if (!abs.startsWith(path.resolve(UPDATE_DIR))) return null; // traversal guard
  if (!fs.existsSync(abs)) return null;

  return { version: target, file: abs, notes: entry.notes || '' };
}

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

function readNotes() {
  try {
    const parsed = JSON.parse(fs.readFileSync(MANIFEST_PATH, 'utf8'));
    return typeof parsed.notes === 'string' ? parsed.notes : '';
  } catch (err) {
    return '';
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

  // Legacy path (no game key): staged-rollout chain if configured, else single
  // current payload. The chain serves the NEXT hop for the client's version.
  const chainTarget = resolveChainTarget(req.query.client);
  if (chainTarget) {
    let hash;
    try {
      hash = computeHash(chainTarget.file);
    } catch (err) {
      return res.status(503).json({ ok: false, error: 'update_unavailable' });
    }
    return res.json({
      version: chainTarget.version,
      hash,
      notes: chainTarget.notes,
      file: `/api/update/download?client=${encodeURIComponent(req.query.client || '')}`
    });
  }

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
  return res.json({ version, hash, notes: readNotes(), file: '/api/update/download' });
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
  } else {
    // Legacy path: resolve the same staged-rollout hop the manifest promised,
    // keyed on the client's reported version. Falls back to the single payload.
    const chainTarget = resolveChainTarget(req.query.client);
    if (chainTarget) {
      dllPath = chainTarget.file;
    }
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
