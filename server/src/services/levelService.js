const UserLevel = require('../models/UserLevel');

// ---------------------------------------------------------------------------
// Tunables
// ---------------------------------------------------------------------------

// Anti-farm cooldown between two XP-granting messages, in ms.
// 0 = every message grants XP (as requested). Set to 60000 for Mee6-style.
const XP_COOLDOWN_MS = 0;

// XP granted per eligible message. Mee6 rolls 15-25; we keep it fixed and
// deterministic (no Math.random, which is unavailable in some harnesses and
// makes progression reproducible).
const XP_PER_MESSAGE = 20;

// Mee6 curve: total XP required to *reach* a given level.
// xpForLevel(n) = 5/6 * n * (2n^2 + 27n + 91)
function xpForLevel(level) {
  return Math.floor((5 / 6) * level * (2 * level * level + 27 * level + 91));
}

// Highest level whose cumulative XP requirement is satisfied by `xp`.
function levelFromXp(xp) {
  let level = 0;
  while (xp >= xpForLevel(level + 1)) {
    level += 1;
  }
  return level;
}

// Progress within the current level, for a progress bar.
// Returns { level, current, needed } where current/needed are XP into the level.
function progress(xp) {
  const level = levelFromXp(xp);
  const base = xpForLevel(level);
  const next = xpForLevel(level + 1);
  return {
    level,
    current: xp - base,
    needed: next - base,
    totalXp: xp
  };
}

// ---------------------------------------------------------------------------
// Award XP for a message
// ---------------------------------------------------------------------------

// Grants XP for one message. Returns { doc, leveledUp, newLevel } or null if
// the message was ignored (cooldown not elapsed). Atomic upsert so concurrent
// messages from the same user cannot lose increments.
async function awardMessageXp(guildId, userId, username, now = new Date()) {
  // Read current state (cheap, indexed) to enforce cooldown and detect level-up.
  const existing = await UserLevel.findOne({ guildId, userId });

  if (
    XP_COOLDOWN_MS > 0 &&
    existing &&
    existing.lastMessageAt &&
    now.getTime() - existing.lastMessageAt.getTime() < XP_COOLDOWN_MS
  ) {
    // Still count the message, but grant no XP during cooldown.
    await UserLevel.updateOne(
      { guildId, userId },
      { $inc: { messageCount: 1 }, $set: { username } }
    );
    return null;
  }

  const previousXp = existing ? existing.xp : 0;
  const previousLevel = existing ? existing.level : 0;
  const newXp = previousXp + XP_PER_MESSAGE;
  const newLevel = levelFromXp(newXp);

  const doc = await UserLevel.findOneAndUpdate(
    { guildId, userId },
    {
      $inc: { xp: XP_PER_MESSAGE, messageCount: 1 },
      $set: { username, level: newLevel, lastMessageAt: now }
    },
    { new: true, upsert: true, setDefaultsOnInsert: true }
  );

  return {
    doc,
    leveledUp: newLevel > previousLevel,
    newLevel,
    previousLevel
  };
}

// Fetch one member's stats (or null).
async function getUserLevel(guildId, userId) {
  return UserLevel.findOne({ guildId, userId });
}

// Grant a raw XP amount (e.g. voice activity). Does not touch messageCount.
// Returns { doc, leveledUp, newLevel, previousLevel } or null if amount <= 0.
async function awardXpAmount(guildId, userId, username, amount) {
  if (!amount || amount <= 0) return null;
  const existing = await UserLevel.findOne({ guildId, userId });
  const previousXp = existing ? existing.xp : 0;
  const previousLevel = existing ? existing.level : 0;
  const newXp = previousXp + amount;
  const newLevel = levelFromXp(newXp);

  const doc = await UserLevel.findOneAndUpdate(
    { guildId, userId },
    { $inc: { xp: amount }, $set: { username, level: newLevel } },
    { new: true, upsert: true, setDefaultsOnInsert: true }
  );
  return { doc, leveledUp: newLevel > previousLevel, newLevel, previousLevel };
}

// Member's rank on the server (1 = highest XP). Null if the member has no doc.
async function getUserRank(guildId, userId) {
  const doc = await UserLevel.findOne({ guildId, userId });
  if (!doc) return null;
  const higher = await UserLevel.countDocuments({ guildId, xp: { $gt: doc.xp } });
  return higher + 1;
}

// Top N members of a guild by XP.
async function getLeaderboard(guildId, limit = 10) {
  return UserLevel.find({ guildId }).sort({ xp: -1 }).limit(limit).lean();
}

// Every ranked member of a guild (for bulk role sync).
async function getAllRanked(guildId) {
  return UserLevel.find({ guildId }).lean();
}

// Total number of ranked members on the server.
async function countRanked(guildId) {
  return UserLevel.countDocuments({ guildId });
}

module.exports = {
  XP_COOLDOWN_MS,
  XP_PER_MESSAGE,
  xpForLevel,
  levelFromXp,
  progress,
  awardMessageXp,
  awardXpAmount,
  getUserLevel,
  getUserRank,
  getLeaderboard,
  getAllRanked,
  countRanked
};
