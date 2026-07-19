const { EmbedBuilder } = require('discord.js');
const { getConfig } = require('../services/guildConfigService');

const COLORS = {
  kick: 0xe67e22,
  ban: 0xff3b6b,
  unban: 0x25ff85,
  timeout: 0xffb800,
  untimeout: 0x25ff85,
  clear: 0x9d4bff,
  warn: 0xffb800,
  escalation: 0xff3b6b
};

// Post a moderation action to the configured mod-log channel. No-op if unset.
// fields: { action, moderator, target, reason, extra }
async function logModAction(guild, { action, moderator, target, reason, extra }) {
  try {
    const config = await getConfig(guild.id);
    if (!config.modlogChannelId) return;
    const channel = guild.channels.cache.get(config.modlogChannelId)
      || await guild.channels.fetch(config.modlogChannelId).catch(() => null);
    if (!channel || !channel.isTextBased()) return;

    const embed = new EmbedBuilder()
      .setColor(COLORS[action] || 0x9d4bff)
      .setTitle(`🛡️ ${action.charAt(0).toUpperCase() + action.slice(1)}`)
      .setTimestamp();

    if (target) embed.addFields({ name: 'Target', value: typeof target === 'string' ? target : `${target} (\`${target.id}\`)`, inline: true });
    if (moderator) embed.addFields({ name: 'Moderator', value: `${moderator}`, inline: true });
    if (reason) embed.addFields({ name: 'Reason', value: String(reason), inline: false });
    if (extra) embed.addFields({ name: 'Details', value: String(extra), inline: false });

    await channel.send({ embeds: [embed], allowedMentions: { parse: [] } }).catch(() => {});
  } catch (err) {
    console.error('[bot] modlog:', err.message);
  }
}

module.exports = { logModAction };
