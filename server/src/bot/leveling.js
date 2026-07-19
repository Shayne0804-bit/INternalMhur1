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
    .setDescription('Affiche ton niveau et ta progression XP')
    .addUserOption((o) => o.setName('membre').setDescription('Membre (toi par defaut)')),

  new SlashCommandBuilder()
    .setName('leaderboard')
    .setDescription('Classement des membres les plus actifs du serveur')
];

const commandNames = new Set(builders.map((b) => b.name));

// ---------------------------------------------------------------------------
// Handlers
// ---------------------------------------------------------------------------

async function handleRank(interaction) {
  const target = interaction.options.getUser('membre') || interaction.user;
  const doc = await getUserLevel(interaction.guildId, target.id);

  if (!doc) {
    return interaction.reply({
      embeds: [new EmbedBuilder().setColor(ACCENT).setTitle('📊 Aucune activite')
        .setDescription(`${target} n'a pas encore gagne d'XP sur ce serveur.`)],
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
    .setTitle(`📊 Niveau ${p.level}`)
    .addFields(
      { name: 'Rang', value: `${medal(rank)} / ${total}`, inline: true },
      { name: 'XP total', value: `${doc.xp.toLocaleString('fr-FR')}`, inline: true },
      { name: 'Messages', value: `${doc.messageCount.toLocaleString('fr-FR')}`, inline: true },
      { name: `Progression (niveau ${p.level} → ${p.level + 1})`,
        value: `\`${bar}\`\n**${p.current.toLocaleString('fr-FR')}** / ${p.needed.toLocaleString('fr-FR')} XP` }
    );

  return interaction.reply({ embeds: [embed] });
}

async function handleLeaderboard(interaction) {
  const rows = await getLeaderboard(interaction.guildId, 10);
  if (!rows.length) {
    return interaction.reply({
      embeds: [new EmbedBuilder().setColor(ACCENT).setTitle('🏆 Classement')
        .setDescription('Personne n\'a encore gagne d\'XP sur ce serveur.')],
      flags: MessageFlags.Ephemeral
    });
  }

  const lines = rows.map((r, i) => {
    const p = progress(r.xp);
    const name = r.username || `<@${r.userId}>`;
    return `${medal(i + 1)} ${name} — **Niv. ${p.level}** · ${r.xp.toLocaleString('fr-FR')} XP`;
  });

  const embed = new EmbedBuilder()
    .setColor(OK)
    .setTitle(`🏆 Classement — ${interaction.guild.name}`)
    .setDescription(lines.join('\n'))
    .setFooter({ text: 'Top 10 des membres les plus actifs' });

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
      content: 'Cette commande doit etre utilisee dans un serveur.',
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
            .setDescription(`🎉 ${message.author} passe **niveau ${result.newLevel}** !`)],
          allowedMentions: { users: [message.author.id] }
        }).catch(() => {});
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
