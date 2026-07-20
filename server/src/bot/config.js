const {
  SlashCommandBuilder,
  PermissionFlagsBits,
  EmbedBuilder,
  MessageFlags,
  ChannelType
} = require('discord.js');

const { getConfig } = require('../services/guildConfigService');
const { readManifest } = require('./updateAnnouncer');

const ACCENT = 0x9d4bff;

// Config keys settable via /config set, each bound to a role or channel option.
// kind drives which option is read and light validation.
const KEYS = {
  customer_role: { field: 'customerRoleId', kind: 'role', label: 'Customer role' },
  member_role: { field: 'memberRoleId', kind: 'role', label: 'Auto member role' },
  staff_role: { field: 'staffRoleId', kind: 'role', label: 'Staff role (tickets)' },
  welcome_channel: { field: 'welcomeChannelId', kind: 'channel', label: 'Welcome channel' },
  modlog_channel: { field: 'modlogChannelId', kind: 'channel', label: 'Mod-log channel' },
  update_channel: { field: 'updateChannelId', kind: 'channel', label: 'Update announcements channel' },
  ticket_category: { field: 'ticketCategoryId', kind: 'category', label: 'Ticket category' }
};

const builders = [
  new SlashCommandBuilder()
    .setName('config')
    .setDescription('Bot configuration (owner only)')
    .setDefaultMemberPermissions(PermissionFlagsBits.Administrator)
    .addSubcommand((s) =>
      s.setName('show').setDescription('Show the current configuration'))
    .addSubcommand((s) =>
      s.setName('set').setDescription('Set a role or channel setting')
        .addStringOption((o) =>
          o.setName('key').setDescription('Setting to change').setRequired(true)
            .addChoices(...Object.entries(KEYS).map(([k, v]) => ({ name: v.label, value: k }))))
        .addRoleOption((o) => o.setName('role').setDescription('Role (for *_role keys)'))
        .addChannelOption((o) => o.setName('channel').setDescription('Channel/category (for *_channel keys)')))
    .addSubcommand((s) =>
      s.setName('clear').setDescription('Clear a setting')
        .addStringOption((o) =>
          o.setName('key').setDescription('Setting to clear').setRequired(true)
            .addChoices(...Object.entries(KEYS).map(([k, v]) => ({ name: v.label, value: k })))))
    .addSubcommandGroup((g) =>
      g.setName('noxp').setDescription('Manage XP-excluded channels')
        .addSubcommand((s) =>
          s.setName('add').setDescription('Exclude a channel from XP')
            .addChannelOption((o) => o.setName('channel').setDescription('Channel').setRequired(true)))
        .addSubcommand((s) =>
          s.setName('remove').setDescription('Re-enable XP in a channel')
            .addChannelOption((o) => o.setName('channel').setDescription('Channel').setRequired(true))))
];

const commandNames = new Set(['config']);

function fmtRole(guild, id) {
  return id ? `<@&${id}>` : '—';
}
function fmtChannel(id) {
  return id ? `<#${id}>` : '—';
}

async function showConfig(interaction) {
  const c = await getConfig(interaction.guildId);
  const noxp = (c.noXpChannelIds || []).map((id) => `<#${id}>`).join(' ') || '—';
  const levelRoleCount = c.levelRoles ? c.levelRoles.size : 0;

  const embed = new EmbedBuilder()
    .setColor(ACCENT)
    .setTitle('⚙️ Server configuration')
    .addFields(
      { name: 'Customer role', value: fmtRole(interaction.guild, c.customerRoleId), inline: true },
      { name: 'Auto member role', value: fmtRole(interaction.guild, c.memberRoleId), inline: true },
      { name: 'Staff role', value: fmtRole(interaction.guild, c.staffRoleId), inline: true },
      { name: 'Welcome channel', value: fmtChannel(c.welcomeChannelId), inline: true },
      { name: 'Mod-log channel', value: fmtChannel(c.modlogChannelId), inline: true },
      { name: 'Update channel', value: fmtChannel(c.updateChannelId), inline: true },
      { name: 'Ticket category', value: fmtChannel(c.ticketCategoryId), inline: true },
      { name: 'XP-excluded channels', value: noxp, inline: false },
      { name: 'Auto level roles', value: `${levelRoleCount} configured`, inline: true }
    );
  return interaction.reply({ embeds: [embed], flags: MessageFlags.Ephemeral });
}

async function setConfig(interaction) {
  const key = interaction.options.getString('key');
  const spec = KEYS[key];
  if (!spec) return interaction.reply({ content: '❌ Unknown key.', flags: MessageFlags.Ephemeral });

  const config = await getConfig(interaction.guildId);

  if (spec.kind === 'role') {
    const role = interaction.options.getRole('role');
    if (!role) return interaction.reply({ content: '❌ Provide a `role` for this key.', flags: MessageFlags.Ephemeral });
    config[spec.field] = role.id;
    await config.save();
    return interaction.reply({ content: `✅ **${spec.label}** set to ${role}.`, flags: MessageFlags.Ephemeral });
  }

  const channel = interaction.options.getChannel('channel');
  if (!channel) return interaction.reply({ content: '❌ Provide a `channel` for this key.', flags: MessageFlags.Ephemeral });
  if (spec.kind === 'category' && channel.type !== ChannelType.GuildCategory) {
    return interaction.reply({ content: '❌ This key needs a **category**, not a text channel.', flags: MessageFlags.Ephemeral });
  }
  if (spec.kind === 'channel' && channel.type !== ChannelType.GuildText) {
    return interaction.reply({ content: '❌ This key needs a **text channel**.', flags: MessageFlags.Ephemeral });
  }
  config[spec.field] = channel.id;
  // Baseline the update channel to the current version so setting it doesn't
  // immediately re-announce the release that's already live.
  if (key === 'update_channel') {
    const manifest = readManifest();
    config.lastAnnouncedVersion = manifest ? manifest.version : null;
  }
  await config.save();
  return interaction.reply({ content: `✅ **${spec.label}** set to ${channel}.`, flags: MessageFlags.Ephemeral });
}

async function clearConfig(interaction) {
  const key = interaction.options.getString('key');
  const spec = KEYS[key];
  if (!spec) return interaction.reply({ content: '❌ Unknown key.', flags: MessageFlags.Ephemeral });
  const config = await getConfig(interaction.guildId);
  config[spec.field] = null;
  await config.save();
  return interaction.reply({ content: `✅ **${spec.label}** cleared.`, flags: MessageFlags.Ephemeral });
}

async function noxp(interaction) {
  const sub = interaction.options.getSubcommand();
  const channel = interaction.options.getChannel('channel');
  const config = await getConfig(interaction.guildId);
  const set = new Set(config.noXpChannelIds || []);

  if (sub === 'add') {
    set.add(channel.id);
    config.noXpChannelIds = [...set];
    await config.save();
    return interaction.reply({ content: `✅ ${channel} is now XP-excluded.`, flags: MessageFlags.Ephemeral });
  }
  set.delete(channel.id);
  config.noXpChannelIds = [...set];
  await config.save();
  return interaction.reply({ content: `✅ ${channel} earns XP again.`, flags: MessageFlags.Ephemeral });
}

async function handleConfigCommand(interaction) {
  if (interaction.commandName !== 'config') return false;
  if (!interaction.inGuild()) {
    await interaction.reply({ content: 'Use this in a server.', flags: MessageFlags.Ephemeral });
    return true;
  }
  const group = interaction.options.getSubcommandGroup(false);
  if (group === 'noxp') {
    await noxp(interaction);
    return true;
  }
  const sub = interaction.options.getSubcommand();
  if (sub === 'show') await showConfig(interaction);
  else if (sub === 'set') await setConfig(interaction);
  else if (sub === 'clear') await clearConfig(interaction);
  return true;
}

module.exports = {
  configCommands: builders.map((b) => b.toJSON()),
  configCommandNames: commandNames,
  handleConfigCommand
};
