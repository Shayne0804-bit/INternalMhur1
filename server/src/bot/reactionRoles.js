const {
  SlashCommandBuilder,
  PermissionFlagsBits,
  EmbedBuilder,
  ActionRowBuilder,
  ButtonBuilder,
  ButtonStyle,
  MessageFlags
} = require('discord.js');

const ACCENT = 0x9d4bff;

// Owner posts a self-assign panel with up to 5 role buttons.
const builders = [
  new SlashCommandBuilder()
    .setName('reactionrole')
    .setDescription('Post a self-assign role panel (owner only)')
    .setDefaultMemberPermissions(PermissionFlagsBits.Administrator)
    .addStringOption((o) => o.setName('title').setDescription('Panel title').setRequired(true))
    .addRoleOption((o) => o.setName('role1').setDescription('Role 1').setRequired(true))
    .addStringOption((o) => o.setName('label1').setDescription('Button label 1').setRequired(true))
    .addRoleOption((o) => o.setName('role2').setDescription('Role 2'))
    .addStringOption((o) => o.setName('label2').setDescription('Button label 2'))
    .addRoleOption((o) => o.setName('role3').setDescription('Role 3'))
    .addStringOption((o) => o.setName('label3').setDescription('Button label 3'))
    .addRoleOption((o) => o.setName('role4').setDescription('Role 4'))
    .addStringOption((o) => o.setName('label4').setDescription('Button label 4'))
    .addRoleOption((o) => o.setName('role5').setDescription('Role 5'))
    .addStringOption((o) => o.setName('label5').setDescription('Button label 5'))
];

const commandNames = new Set(['reactionrole']);

async function handleReactionRoleCommand(interaction) {
  if (interaction.commandName !== 'reactionrole') return false;

  const me = interaction.guild.members.me;
  const buttons = [];
  const lines = [];
  for (let i = 1; i <= 5; i += 1) {
    const role = interaction.options.getRole(`role${i}`);
    if (!role) continue;
    const label = interaction.options.getString(`label${i}`) || role.name;
    if (role.position >= me.roles.highest.position) {
      return interaction.reply({ content: `❌ The role ${role} is above mine, I can't assign it.`, flags: MessageFlags.Ephemeral });
    }
    buttons.push(
      new ButtonBuilder().setCustomId(`rr:${role.id}`).setLabel(label).setStyle(ButtonStyle.Secondary)
    );
    lines.push(`• **${label}** → ${role}`);
  }

  const embed = new EmbedBuilder()
    .setColor(ACCENT)
    .setTitle(interaction.options.getString('title'))
    .setDescription(['Click a button to get or remove a role.', '', ...lines].join('\n'));

  const row = new ActionRowBuilder().addComponents(...buttons);
  await interaction.channel.send({ embeds: [embed], components: [row] });
  return interaction.reply({ content: '✅ Reaction-role panel posted.', flags: MessageFlags.Ephemeral });
}

// Button entry point (ns === 'rr'). Toggles the role on the member.
async function handleReactionRoleButton(interaction, roleId) {
  const role = interaction.guild.roles.cache.get(roleId)
    || await interaction.guild.roles.fetch(roleId).catch(() => null);
  if (!role) {
    return interaction.reply({ content: '❌ That role no longer exists.', flags: MessageFlags.Ephemeral });
  }
  const me = interaction.guild.members.me;
  if (role.position >= me.roles.highest.position) {
    return interaction.reply({ content: "❌ I can't manage that role (it's above mine).", flags: MessageFlags.Ephemeral });
  }
  const member = interaction.member;
  if (member.roles.cache.has(role.id)) {
    await member.roles.remove(role, 'Reaction role toggle');
    return interaction.reply({ content: `➖ Removed ${role}.`, flags: MessageFlags.Ephemeral });
  }
  await member.roles.add(role, 'Reaction role toggle');
  return interaction.reply({ content: `➕ Added ${role}.`, flags: MessageFlags.Ephemeral });
}

module.exports = {
  reactionRoleCommands: builders.map((b) => b.toJSON()),
  reactionRoleCommandNames: commandNames,
  handleReactionRoleCommand,
  handleReactionRoleButton
};
