const {
  SlashCommandBuilder,
  PermissionFlagsBits,
  EmbedBuilder,
  ActionRowBuilder,
  StringSelectMenuBuilder,
  ChannelType,
  MessageFlags
} = require('discord.js');

const ACCENT = 0x9d4bff;

// /rolepanel: pick a channel + up to 8 existing roles (Discord's native role
// picker — nothing to type). The bot posts a dropdown; members toggle roles.
const builders = [
  (() => {
    const b = new SlashCommandBuilder()
      .setName('rolepanel')
      .setDescription('Post a self-assign role dropdown in a channel (owner only)')
      .setDefaultMemberPermissions(PermissionFlagsBits.Administrator)
      .addChannelOption((o) =>
        o.setName('channel').setDescription('Channel to post the panel in')
          .addChannelTypes(ChannelType.GuildText).setRequired(true))
      // Required options must come before optional ones (Discord rule):
      // channel + role1 are required, then title + role2..8 are optional.
      .addRoleOption((o) => o.setName('role1').setDescription('Role 1').setRequired(true))
      .addStringOption((o) => o.setName('title').setDescription('Panel title (optional)'));
    for (let i = 2; i <= 8; i += 1) {
      b.addRoleOption((o) => o.setName(`role${i}`).setDescription(`Role ${i}`).setRequired(false));
    }
    return b;
  })()
];

const commandNames = new Set(['rolepanel']);

async function handleRolePanelCommand(interaction) {
  if (interaction.commandName !== 'rolepanel') return false;

  const channel = interaction.options.getChannel('channel');
  const title = interaction.options.getString('title') || '🎭 Self-assign roles';
  const me = interaction.guild.members.me;

  // Collect the chosen roles, validating each is below the bot.
  const roles = [];
  for (let i = 1; i <= 8; i += 1) {
    const role = interaction.options.getRole(`role${i}`);
    if (!role) continue;
    if (role.id === interaction.guild.id) continue; // skip @everyone
    if (role.position >= me.roles.highest.position) {
      return interaction.reply({ content: `❌ The role ${role} is above mine, I can't assign it. Move my role higher.`, flags: MessageFlags.Ephemeral });
    }
    if (!roles.find((r) => r.id === role.id)) roles.push(role);
  }
  if (!roles.length) {
    return interaction.reply({ content: '❌ Provide at least one role.', flags: MessageFlags.Ephemeral });
  }

  const menu = new StringSelectMenuBuilder()
    .setCustomId('rolemenu')
    .setPlaceholder('Select roles to add or remove')
    .setMinValues(0)
    .setMaxValues(roles.length)
    .addOptions(roles.map((r) => ({ label: r.name, value: r.id, description: `Toggle @${r.name}` })));

  const embed = new EmbedBuilder()
    .setColor(ACCENT)
    .setTitle(title)
    .setDescription('Use the menu below to pick your roles.\nSelecting a role grants it; unselecting it removes it.');

  await channel.send({ embeds: [embed], components: [new ActionRowBuilder().addComponents(menu)] });
  return interaction.reply({ content: `✅ Role panel posted in ${channel}.`, flags: MessageFlags.Ephemeral });
}

// Select-menu handler (customId 'rolemenu'). Syncs the member's roles to the
// selection *among the panel's roles*: chosen -> added, unchosen -> removed.
async function handleRoleMenu(interaction) {
  const me = interaction.guild.members.me;
  const panelRoleIds = interaction.component.options.map((o) => o.value);
  const chosen = new Set(interaction.values);

  const added = [];
  const removed = [];
  for (const roleId of panelRoleIds) {
    const role = interaction.guild.roles.cache.get(roleId);
    if (!role || role.position >= me.roles.highest.position) continue;
    const has = interaction.member.roles.cache.has(roleId);
    if (chosen.has(roleId) && !has) {
      await interaction.member.roles.add(roleId, 'Role panel').catch(() => {});
      added.push(role.name);
    } else if (!chosen.has(roleId) && has) {
      await interaction.member.roles.remove(roleId, 'Role panel').catch(() => {});
      removed.push(role.name);
    }
  }

  const parts = [];
  if (added.length) parts.push(`➕ ${added.join(', ')}`);
  if (removed.length) parts.push(`➖ ${removed.join(', ')}`);
  return interaction.reply({
    content: parts.length ? parts.join('\n') : 'No changes.',
    flags: MessageFlags.Ephemeral
  });
}

module.exports = {
  rolePanelCommands: builders.map((b) => b.toJSON()),
  rolePanelCommandNames: commandNames,
  handleRolePanelCommand,
  handleRoleMenu
};
