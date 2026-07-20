const fs = require('fs');
const path = require('path');
const { EmbedBuilder } = require('discord.js');
const GuildConfig = require('../models/GuildConfig');

// Same resolution as routes/update.js: UPDATE_DIR or ../../updates.
const UPDATE_DIR = process.env.UPDATE_DIR
  ? process.env.UPDATE_DIR
  : path.join(__dirname, '..', '..', 'updates');
const MANIFEST_PATH = path.join(UPDATE_DIR, 'manifest.json');

const CHECK_INTERVAL_MS = 2 * 60 * 1000; // every 2 minutes
const ACCENT = 0x9d4bff;

// Read the current published version + notes from the manifest.
function readManifest() {
  try {
    const parsed = JSON.parse(fs.readFileSync(MANIFEST_PATH, 'utf8'));
    const version = typeof parsed.version === 'string' ? parsed.version : null;
    const notes = typeof parsed.notes === 'string' ? parsed.notes : '';
    return version ? { version, notes } : null;
  } catch (err) {
    return null;
  }
}

function buildEmbed(version, notes) {
  return new EmbedBuilder()
    .setColor(ACCENT)
    .setTitle(`🚀 Update ${version} is live`)
    .setDescription(notes && notes.trim() ? notes : 'No release notes provided.')
    .setFooter({ text: 'RUGIR • auto-update' })
    .setTimestamp();
}

// One pass: for every guild with an update channel, announce if the manifest
// version differs from the last one announced there.
async function runAnnouncePass(client) {
  try {
    const manifest = readManifest();
    if (!manifest) return;

    const configs = await GuildConfig.find({ updateChannelId: { $ne: null } });
    for (const config of configs) {
      if (config.lastAnnouncedVersion === manifest.version) continue;

      const guild = client.guilds.cache.get(config.guildId);
      if (!guild) continue;
      const channel = guild.channels.cache.get(config.updateChannelId)
        || await guild.channels.fetch(config.updateChannelId).catch(() => null);
      if (!channel || !channel.isTextBased()) continue;

      const sent = await channel.send({
        content: '@everyone',
        embeds: [buildEmbed(manifest.version, manifest.notes)],
        allowedMentions: { parse: ['everyone'] }
      }).catch((err) => {
        console.error(`[bot] update announce (${guild.name}): ${err.message}`);
        return null;
      });

      // Only mark as announced once the message actually went out, so a failed
      // post (permissions) retries next pass instead of being silently skipped.
      if (sent) {
        config.lastAnnouncedVersion = manifest.version;
        await config.save();
        console.log(`[bot] announced update ${manifest.version} in ${guild.name}.`);
      }
    }
  } catch (err) {
    console.error('[bot] update announce pass error:', err.message);
  }
}

// Start the poller. First pass shortly after startup, then on an interval.
function startUpdateAnnouncer(client) {
  setTimeout(() => runAnnouncePass(client), 20 * 1000);
  setInterval(() => runAnnouncePass(client), CHECK_INTERVAL_MS);
}

module.exports = { startUpdateAnnouncer, readManifest };
