const {
  SlashCommandBuilder,
  PermissionFlagsBits,
  EmbedBuilder,
  MessageFlags
} = require('discord.js');

const Warning = require('../models/Warning');
const { logModAction } = require('./modlog');

const OK = 0x25ff85;
const ERR = 0xff3b6b;
const WARN_COLOR = 0xffb800;

// Escalation thresholds: warning count -> automatic action.
const TIMEOUT_AT = 3;           // 3rd warning -> timeout
const TIMEOUT_MS = 60 * 60 * 1000; // 1 hour
const KICK_AT = 5;              // 5th warning -> kick

const builders = [
  new SlashCommandBuilder()
    .setName('warn')
    .setDescription('Warn a member')
    .setDefaultMemberPermissions(PermissionFlagsBits.ModerateMembers)
    .addUserOption((o) => o.setName('member').setDescription('Member to warn').setRequired(true))
    .addStringOption((o) => o.setName('reason').setDescription('Reason')),

  new SlashCommandBuilder()
    .setName('warnings')
    .setDescription("List a member's warnings")
    .setDefaultMemberPermissions(PermissionFlagsBits.ModerateMembers)
    .addUserOption((o) => o.setName('member').setDescription('Member').setRequired(true)),

  new SlashCommandBuilder()
    .setName('delwarn')
    .setDescription('Delete a warning by id (or clear all for a member)')
    .setDefaultMemberPermissions(PermissionFlagsBits.ModerateMembers)
    .addStringOption((o) => o.setName('id').setDescription('Warning id'))
    .addUserOption((o) => o.setName('member').setDescription('Clear ALL warnings for this member'))
];

const commandNames = new Set(['warn', 'warnings', 'delwarn']);

async function handleWarn(interaction) {
  const user = interaction.options.getUser('member');
  const member = interaction.options.getMember('member');
  const reason = interaction.options.getString('reason') || 'No reason';

  await Warning.create({
    guildId: interaction.guildId,
    userId: user.id,
    moderatorId: interaction.user.id,
    reason
  });
  const count = await Warning.countDocuments({ guildId: interaction.guildId, userId: user.id });

  await logModAction(interaction.guild, {
    action: 'warn', moderator: interaction.user, target: user, reason, extra: `Total warnings: ${count}`
  });

  // Auto-escalation.
  let escalation = null;
  if (member) {
    if (count >= KICK_AT && member.kickable) {
      await member.kick(`Auto-escalation: reached ${KICK_AT} warnings`).catch(() => {});
      escalation = `⛔ Auto-kick (reached ${KICK_AT} warnings).`;
    } else if (count >= TIMEOUT_AT && member.moderatable) {
      await member.timeout(TIMEOUT_MS, `Auto-escalation: reached ${TIMEOUT_AT} warnings`).catch(() => {});
      escalation = `⏳ Auto-timeout 1h (reached ${TIMEOUT_AT} warnings).`;
    }
  }
  if (escalation) {
    await logModAction(interaction.guild, {
      action: 'escalation', moderator: interaction.client.user, target: user, reason: escalation
    });
  }

  const embed = new EmbedBuilder()
    .setColor(WARN_COLOR)
    .setTitle('⚠️ Member warned')
    .setDescription(`${user} has been warned.\n**Reason:** ${reason}\n**Total warnings:** ${count}`);
  if (escalation) embed.addFields({ name: 'Escalation', value: escalation });
  return interaction.reply({ embeds: [embed] });
}

async function handleWarnings(interaction) {
  const user = interaction.options.getUser('member');
  const rows = await Warning.find({ guildId: interaction.guildId, userId: user.id }).sort({ createdAt: -1 }).limit(15);
  if (!rows.length) {
    return interaction.reply({ embeds: [new EmbedBuilder().setColor(OK).setDescription(`✅ ${user} has no warnings.`)], flags: MessageFlags.Ephemeral });
  }
  const lines = rows.map((w) => {
    const when = `<t:${Math.floor(new Date(w.createdAt).getTime() / 1000)}:R>`;
    return `\`${w._id}\` • ${when} • by <@${w.moderatorId}>\n> ${w.reason}`;
  });
  const embed = new EmbedBuilder()
    .setColor(WARN_COLOR)
    .setTitle(`⚠️ Warnings for ${user.tag}`)
    .setDescription(lines.join('\n'))
    .setFooter({ text: `${rows.length} warning(s)` });
  return interaction.reply({ embeds: [embed], flags: MessageFlags.Ephemeral });
}

async function handleDelwarn(interaction) {
  const id = interaction.options.getString('id');
  const member = interaction.options.getUser('member');

  if (member) {
    const res = await Warning.deleteMany({ guildId: interaction.guildId, userId: member.id });
    return interaction.reply({ embeds: [new EmbedBuilder().setColor(OK).setDescription(`✅ Cleared ${res.deletedCount} warning(s) for ${member}.`)], flags: MessageFlags.Ephemeral });
  }
  if (!id) {
    return interaction.reply({ content: '❌ Provide a warning `id` or a `member` to clear all.', flags: MessageFlags.Ephemeral });
  }
  const deleted = await Warning.findOneAndDelete({ _id: id, guildId: interaction.guildId }).catch(() => null);
  if (!deleted) {
    return interaction.reply({ content: '❌ Warning not found (check the id).', flags: MessageFlags.Ephemeral });
  }
  return interaction.reply({ embeds: [new EmbedBuilder().setColor(OK).setDescription(`✅ Warning \`${id}\` deleted.`)], flags: MessageFlags.Ephemeral });
}

async function handleWarnCommand(interaction) {
  if (!commandNames.has(interaction.commandName)) return false;
  if (!interaction.inGuild()) {
    await interaction.reply({ content: 'Use this in a server.', flags: MessageFlags.Ephemeral });
    return true;
  }
  if (interaction.commandName === 'warn') await handleWarn(interaction);
  else if (interaction.commandName === 'warnings') await handleWarnings(interaction);
  else if (interaction.commandName === 'delwarn') await handleDelwarn(interaction);
  return true;
}

module.exports = {
  warnCommands: builders.map((b) => b.toJSON()),
  warnCommandNames: commandNames,
  handleWarnCommand
};
