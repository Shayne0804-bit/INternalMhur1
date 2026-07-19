const {
  SlashCommandBuilder,
  EmbedBuilder,
  MessageFlags
} = require('discord.js');

const {
  progress,
  awardMessageXp,
  getUserLevel,
  getUserRank,
  getLeaderboard,
  countRanked
} = require('../services/levelService');
const { syncMemberLevelRole } = require('./levelRoles');

const ACCENT = 0x9d4bff;
const OK = 0x25ff85;

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
    .setDescription('Ranking of the most active members on the server')
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

  return interaction.reply({ embeds: [embed] });
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

const handlers = {
  rank: handleRank,
  leaderboard: handleLeaderboard
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
}

module.exports = {
  levelingCommands: builders.map((b) => b.toJSON()),
  levelingCommandNames: commandNames,
  handleLevelingCommand,
  attachLeveling
};
