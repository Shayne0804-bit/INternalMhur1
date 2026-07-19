const {
  SlashCommandBuilder,
  PermissionFlagsBits,
  EmbedBuilder,
  MessageFlags
} = require('discord.js');

const {
  findLicenseByKeyOrUser,
  getLicenseById,
  revokeLicense,
  extendLicense,
  listLicenses,
  getLicenseStats
} = require('../services/licenseService');

const ACCENT = 0x9d4bff;
const OK = 0x25ff85;
const ERR = 0xff3b6b;

function ts(date) {
  return date ? `<t:${Math.floor(new Date(date).getTime() / 1000)}:F>` : 'Lifetime';
}

function statusEmoji(status) {
  return { active: '✅', revoked: '⛔', expired: '⏳' }[status] || '❓';
}

function licenseEmbed(license, title = '📄 License') {
  return new EmbedBuilder()
    .setColor(ACCENT)
    .setTitle(title)
    .addFields(
      { name: 'Key', value: '```' + (license.keyPlain || 'hidden') + '```' },
      { name: 'Status', value: `${statusEmoji(license.status)} ${license.status}`, inline: true },
      { name: 'Tier', value: String(license.tier), inline: true },
      { name: 'Activations', value: `${license.hwidHashes.length}/${license.maxActivations}`, inline: true },
      { name: 'Expiration', value: ts(license.expiresAt), inline: true },
      { name: 'Discord', value: license.discordUserId ? `<@${license.discordUserId}>` : '—', inline: true },
      { name: 'Created', value: ts(license.createdAt), inline: true }
    )
    .setFooter({ text: `ID: ${license._id}` });
}

const builders = [
  new SlashCommandBuilder()
    .setName('license')
    .setDescription('License management (owner only)')
    .setDefaultMemberPermissions(PermissionFlagsBits.Administrator)
    .addSubcommand((s) =>
      s.setName('lookup').setDescription('Look up a license by key or user')
        .addStringOption((o) => o.setName('query').setDescription('License key or Discord user id').setRequired(false))
        .addUserOption((o) => o.setName('user').setDescription('Discord member').setRequired(false)))
    .addSubcommand((s) =>
      s.setName('revoke').setDescription('Revoke a license by id')
        .addStringOption((o) => o.setName('id').setDescription('License id').setRequired(true)))
    .addSubcommand((s) =>
      s.setName('extend').setDescription('Extend a license by N days')
        .addStringOption((o) => o.setName('id').setDescription('License id').setRequired(true))
        .addIntegerOption((o) => o.setName('days').setDescription('Days to add (negative to shorten)').setRequired(true)))
    .addSubcommand((s) =>
      s.setName('list').setDescription('List recent licenses')
        .addStringOption((o) => o.setName('status').setDescription('Filter by status')
          .addChoices(
            { name: 'active', value: 'active' },
            { name: 'revoked', value: 'revoked' },
            { name: 'expired', value: 'expired' }
          ))),

  new SlashCommandBuilder()
    .setName('stats')
    .setDescription('License statistics (owner only)')
    .setDefaultMemberPermissions(PermissionFlagsBits.Administrator)
];

const commandNames = new Set(['license', 'stats']);

async function handleLookup(interaction) {
  const user = interaction.options.getUser('user');
  const query = user ? user.id : (interaction.options.getString('query') || '').trim();
  if (!query) {
    return interaction.reply({ content: '❌ Provide a key or a user.', flags: MessageFlags.Ephemeral });
  }
  const license = await findLicenseByKeyOrUser(query);
  if (!license) {
    return interaction.reply({ content: '❌ No license found.', flags: MessageFlags.Ephemeral });
  }
  return interaction.reply({ embeds: [licenseEmbed(license)], flags: MessageFlags.Ephemeral });
}

async function handleRevoke(interaction) {
  const id = interaction.options.getString('id').trim();
  const license = await revokeLicense(id).catch(() => null);
  if (!license) {
    return interaction.reply({ content: '❌ License not found (check the id).', flags: MessageFlags.Ephemeral });
  }
  return interaction.reply({ embeds: [licenseEmbed(license, '⛔ License revoked')], flags: MessageFlags.Ephemeral });
}

async function handleExtend(interaction) {
  const id = interaction.options.getString('id').trim();
  const days = interaction.options.getInteger('days');
  const license = await extendLicense(id, days).catch(() => null);
  if (!license) {
    return interaction.reply({ content: '❌ License not found (check the id).', flags: MessageFlags.Ephemeral });
  }
  const verb = days >= 0 ? 'extended' : 'shortened';
  return interaction.reply({ embeds: [licenseEmbed(license, `⏱️ License ${verb} by ${Math.abs(days)}d`)], flags: MessageFlags.Ephemeral });
}

async function handleList(interaction) {
  const status = interaction.options.getString('status') || undefined;
  const rows = await listLicenses({ status, limit: 15 });
  if (!rows.length) {
    return interaction.reply({ content: 'No licenses found.', flags: MessageFlags.Ephemeral });
  }
  const lines = rows.map((l) => {
    const who = l.discordUserId ? `<@${l.discordUserId}>` : '—';
    const exp = l.expiresAt ? `<t:${Math.floor(new Date(l.expiresAt).getTime() / 1000)}:R>` : 'lifetime';
    return `${statusEmoji(l.status)} \`${l._id}\` · ${l.tier} · ${who} · exp ${exp}`;
  });
  const embed = new EmbedBuilder()
    .setColor(ACCENT)
    .setTitle(`📋 Licenses${status ? ` (${status})` : ''}`)
    .setDescription(lines.join('\n'))
    .setFooter({ text: `${rows.length} shown` });
  return interaction.reply({ embeds: [embed], flags: MessageFlags.Ephemeral });
}

async function handleStats(interaction) {
  await interaction.deferReply({ flags: MessageFlags.Ephemeral });
  const s = await getLicenseStats();
  const embed = new EmbedBuilder()
    .setColor(OK)
    .setTitle('📊 License statistics')
    .addFields(
      { name: 'Total', value: String(s.total), inline: true },
      { name: '✅ Active', value: String(s.active), inline: true },
      { name: '⏳ Expired', value: String(s.expired), inline: true },
      { name: '⛔ Revoked', value: String(s.revoked), inline: true },
      { name: '♾️ Lifetime', value: String(s.lifetime), inline: true },
      { name: '🆕 New this month', value: String(s.newThisMonth), inline: true },
      { name: '💻 Total activations', value: String(s.activations), inline: true },
      { name: '🔗 Linked to Discord', value: String(s.boundToDiscord), inline: true }
    );
  return interaction.editReply({ embeds: [embed] });
}

async function handleLicenseCommand(interaction) {
  if (interaction.commandName === 'stats') {
    await handleStats(interaction);
    return true;
  }
  if (interaction.commandName !== 'license') return false;
  const sub = interaction.options.getSubcommand();
  if (sub === 'lookup') await handleLookup(interaction);
  else if (sub === 'revoke') await handleRevoke(interaction);
  else if (sub === 'extend') await handleExtend(interaction);
  else if (sub === 'list') await handleList(interaction);
  return true;
}

module.exports = {
  licenseCommands: builders.map((b) => b.toJSON()),
  licenseCommandNames: commandNames,
  handleLicenseCommand
};
