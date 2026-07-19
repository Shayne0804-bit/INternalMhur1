const {
  SlashCommandBuilder,
  EmbedBuilder,
  MessageFlags,
  PermissionFlagsBits,
  AttachmentBuilder
} = require('discord.js');

const {
  progress,
  awardMessageXp,
  awardXpAmount,
  getUserLevel,
  getUserRank,
  getLeaderboard,
  getAllRanked,
  levelFromXp,
  countRanked
} = require('../services/levelService');
const { syncMemberLevelRole } = require('./levelRoles');
const { getConfig } = require('../services/guildConfigService');
const { renderRankCard } = require('./rankCard');

const ACCENT = 0x9d4bff;
const OK = 0x25ff85;

// XP granted per full minute spent in a voice channel (not muted/alone).
const XP_PER_VOICE_MINUTE = 5;
// In-memory voice session start timestamps, keyed by `guildId:userId`.
const voiceSessions = new Map();

// Unicode progress bar, `size` cells filled proportionally to current/needed.
function progressBar(current, needed, size = 16) {
  const ratio = needed > 0 ? Math.max(0, Math.min(1, current / needed)) : 0;
  const filled = Math.round(ratio * size);
  return '█'.repeat(filled) + '░'.repeat(size - filled);
}

// Rank medal for the top 3.
function medal(rank) {
  return { 1: '🥇', 2: '🥈', 3: '🥉' }[rank] || `**#${rank}**`;
}

// ---------------------------------------------------------------------------
// Command definitions
// ---------------------------------------------------------------------------

const builders = [
  new SlashCommandBuilder()
    .setName('rank')
    .setDescription('Show your level and XP progression')
    .addUserOption((o) => o.setName('member').setDescription('Member (yourself by default)')),

  new SlashCommandBuilder()
    .setName('leaderboard')
    .setDescription('Ranking of the most active members on the server'),

  new SlashCommandBuilder()
    .setName('synclevelroles')
    .setDescription('Grant level roles to all members based on their current level (owner only)')
    .setDefaultMemberPermissions(PermissionFlagsBits.Administrator)
];

const commandNames = new Set(builders.map((b) => b.name));

// ---------------------------------------------------------------------------
// Handlers
// ---------------------------------------------------------------------------

async function handleRank(interaction) {
  const target = interaction.options.getUser('member') || interaction.user;
  const doc = await getUserLevel(interaction.guildId, target.id);

  if (!doc) {
    return interaction.reply({
      embeds: [new EmbedBuilder().setColor(ACCENT).setTitle('📊 No activity')
        .setDescription(`${target} hasn't earned any XP on this server yet.`)],
      flags: MessageFlags.Ephemeral
    });
  }

  const p = progress(doc.xp);
  const rank = await getUserRank(interaction.guildId, target.id);
  const total = await countRanked(interaction.guildId);

  await interaction.deferReply();

  // Render the rank card image; fall back to a text embed on failure.
  try {
    const buffer = await renderRankCard({
      username: target.tag || target.username,
      avatarURL: target.displayAvatarURL({ extension: 'png', size: 256 }),
      level: p.level,
      rank,
      totalRanked: total,
      current: p.current,
      needed: p.needed,
      totalXp: doc.xp
    });
    const file = new AttachmentBuilder(buffer, { name: 'rank.png' });
    return interaction.editReply({ files: [file] });
  } catch (err) {
    console.error('[bot] rank card render failed:', err.message);
    const bar = progressBar(p.current, p.needed);
    const embed = new EmbedBuilder()
      .setColor(ACCENT)
      .setAuthor({ name: target.tag, iconURL: target.displayAvatarURL() })
      .setTitle(`📊 Level ${p.level}`)
      .addFields(
        { name: 'Rank', value: `${medal(rank)} / ${total}`, inline: true },
        { name: 'Total XP', value: `${doc.xp.toLocaleString('en-US')}`, inline: true },
        { name: 'Messages', value: `${doc.messageCount.toLocaleString('en-US')}`, inline: true },
        { name: `Progress (level ${p.level} → ${p.level + 1})`,
          value: `\`${bar}\`\n**${p.current.toLocaleString('en-US')}** / ${p.needed.toLocaleString('en-US')} XP` }
      );
    return interaction.editReply({ embeds: [embed] });
  }
}

async function handleLeaderboard(interaction) {
  const rows = await getLeaderboard(interaction.guildId, 10);
  if (!rows.length) {
    return interaction.reply({
      embeds: [new EmbedBuilder().setColor(ACCENT).setTitle('🏆 Leaderboard')
        .setDescription('Nobody has earned any XP on this server yet.')],
      flags: MessageFlags.Ephemeral
    });
  }

  const lines = rows.map((r, i) => {
    const p = progress(r.xp);
    const name = r.username || `<@${r.userId}>`;
    return `${medal(i + 1)} ${name} — **Lvl ${p.level}** · ${r.xp.toLocaleString('en-US')} XP`;
  });

  const embed = new EmbedBuilder()
    .setColor(OK)
    .setTitle(`🏆 Leaderboard — ${interaction.guild.name}`)
    .setDescription(lines.join('\n'))
    .setFooter({ text: 'Top 10 most active members' });

  return interaction.reply({ embeds: [embed] });
}

async function handleSyncLevelRoles(interaction) {
  await interaction.deferReply({ flags: MessageFlags.Ephemeral });
  const rows = await getAllRanked(interaction.guildId);
  let synced = 0;
  for (const row of rows) {
    const member = await interaction.guild.members.fetch(row.userId).catch(() => null);
    if (!member) continue;
    await syncMemberLevelRole(member, levelFromXp(row.xp)).catch(() => {});
    synced += 1;
  }
  return interaction.editReply({ content: `✅ Synced level roles for ${synced} member(s).` });
}

const handlers = {
  rank: handleRank,
  leaderboard: handleLeaderboard,
  synclevelroles: handleSyncLevelRoles
};

async function handleLevelingCommand(interaction) {
  const handler = handlers[interaction.commandName];
  if (!handler) return false;
  if (!interaction.inGuild()) {
    await interaction.reply({
      content: 'This command must be used in a server.',
      flags: MessageFlags.Ephemeral
    });
    return true;
  }
  await handler(interaction);
  return true;
}

// ---------------------------------------------------------------------------
// Message listener — grants XP and announces level-ups
// ---------------------------------------------------------------------------

function attachLeveling(client) {
  client.on('messageCreate', async (message) => {
    try {
      // Ignore bots, DMs, and system messages.
      if (message.author.bot || !message.guild) return;

      // Skip XP-excluded channels.
      const config = await getConfig(message.guild.id);
      if ((config.noXpChannelIds || []).includes(message.channelId)) return;

      const result = await awardMessageXp(
        message.guild.id,
        message.author.id,
        message.author.tag || message.author.username
      );

      if (result && result.leveledUp) {
        await message.channel.send({
          embeds: [new EmbedBuilder()
            .setColor(OK)
            .setDescription(`🎉 ${message.author} reached **level ${result.newLevel}**!`)],
          allowedMentions: { users: [message.author.id] }
        }).catch(() => {});

        // Grant/replace the level-based role for the new level.
        const member = message.member
          || await message.guild.members.fetch(message.author.id).catch(() => null);
        if (member) {
          await syncMemberLevelRole(member, result.newLevel).catch((e) =>
            console.error('[bot] levelRoles sync error:', e.message));
        }
      }
    } catch (err) {
      console.error('[bot] leveling error:', err.message);
    }
  });

  // Voice XP: measure time spent in a voice channel and grant XP per minute.
  // A "session" runs while the member is in a channel and not the only human.
  client.on('voiceStateUpdate', async (oldState, newState) => {
    try {
      const member = newState.member || oldState.member;
      if (!member || member.user.bot) return;
      const guildId = (newState.guild || oldState.guild).id;
      const key = `${guildId}:${member.id}`;

      const wasIn = Boolean(oldState.channelId);
      const isIn = Boolean(newState.channelId);

      // Joined a voice channel.
      if (!wasIn && isIn) {
        voiceSessions.set(key, Date.now());
        return;
      }
      // Left voice entirely, or moved: settle the elapsed session.
      if (wasIn) {
        const start = voiceSessions.get(key);
        if (start) {
          const minutes = Math.floor((Date.now() - start) / 60000);
          if (minutes > 0) {
            await awardXpAmount(guildId, member.id, member.user.tag || member.user.username, minutes * XP_PER_VOICE_MINUTE).catch(() => {});
          }
        }
        if (isIn) {
          voiceSessions.set(key, Date.now()); // moved channel: restart timer
        } else {
          voiceSessions.delete(key); // left voice
        }
      }
    } catch (err) {
      console.error('[bot] voice xp error:', err.message);
    }
  });
}

module.exports = {
  levelingCommands: builders.map((b) => b.toJSON()),
  levelingCommandNames: commandNames,
  handleLevelingCommand,
  attachLeveling
};
