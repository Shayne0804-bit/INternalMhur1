const { PermissionFlagsBits } = require('discord.js');
const Warning = require('../models/Warning');
const { logModAction } = require('./modlog');

// ---------------------------------------------------------------------------
// Tunables
// ---------------------------------------------------------------------------

// Anti-spam: more than MAX_MSGS within WINDOW_MS from one member triggers.
const WINDOW_MS = 7000;
const MAX_MSGS = 5;

// Discord invite links (discord.gg/xxx, discord.com/invite/xxx).
const INVITE_RE = /(discord\.(gg|io|me|li)|discord(app)?\.com\/invite)\/[a-z0-9-]+/i;

// Simple word filter. Case-insensitive whole-word-ish match.
const BANNED_WORDS = ['nigger', 'faggot', 'kike', 'retard'];
const BANNED_RE = new RegExp(`\\b(${BANNED_WORDS.join('|')})\\b`, 'i');

// In-memory message timestamps per `guildId:userId` for rate detection.
const recent = new Map();

// Auto-warn helper: records a warning and logs it (feeds the escalation system
// on the next manual /warn, and gives moderators a trail).
async function autoWarn(message, reason) {
  try {
    await Warning.create({
      guildId: message.guild.id,
      userId: message.author.id,
      moderatorId: message.client.user.id,
      reason: `[automod] ${reason}`
    });
    const count = await Warning.countDocuments({ guildId: message.guild.id, userId: message.author.id });
    await logModAction(message.guild, {
      action: 'warn',
      moderator: message.client.user,
      target: message.author,
      reason: `[automod] ${reason}`,
      extra: `Total warnings: ${count}`
    });

    // Escalate on the same thresholds as manual warns.
    const member = message.member;
    if (member) {
      if (count >= 5 && member.kickable) {
        await member.kick('Auto-escalation: 5 automod warnings').catch(() => {});
      } else if (count >= 3 && member.moderatable) {
        await member.timeout(60 * 60 * 1000, 'Auto-escalation: 3 automod warnings').catch(() => {});
      }
    }
  } catch (err) {
    console.error('[bot] automod autoWarn:', err.message);
  }
}

// Returns true if the member is exempt (admins/mods are never auto-moderated).
function isExempt(message) {
  const m = message.member;
  if (!m) return true;
  return m.permissions.has(PermissionFlagsBits.ManageMessages)
    || m.permissions.has(PermissionFlagsBits.Administrator);
}

async function deleteAndWarn(message, reason) {
  await message.delete().catch(() => {});
  await message.channel.send({
    content: `⚠️ ${message.author}, ${reason}`,
    allowedMentions: { users: [message.author.id] }
  }).then((m) => setTimeout(() => m.delete().catch(() => {}), 5000)).catch(() => {});
  await autoWarn(message, reason);
}

function attachAutomod(client) {
  client.on('messageCreate', async (message) => {
    try {
      if (message.author.bot || !message.guild) return;
      if (isExempt(message)) return;

      // --- Anti-spam (rate) — works without MessageContent intent ---
      const key = `${message.guild.id}:${message.author.id}`;
      const now = Date.now();
      const arr = (recent.get(key) || []).filter((t) => now - t < WINDOW_MS);
      arr.push(now);
      recent.set(key, arr);
      if (arr.length > MAX_MSGS) {
        recent.set(key, []);
        await deleteAndWarn(message, 'please stop spamming.');
        return;
      }

      // --- Content-based checks — require the MessageContent intent ---
      const content = message.content || '';
      if (!content) return; // intent not enabled: skip content checks silently

      if (INVITE_RE.test(content)) {
        await deleteAndWarn(message, 'Discord invite links are not allowed.');
        return;
      }
      if (BANNED_RE.test(content)) {
        await deleteAndWarn(message, 'watch your language.');
        return;
      }
    } catch (err) {
      console.error('[bot] automod error:', err.message);
    }
  });
}

module.exports = { attachAutomod };
