const {
  SlashCommandBuilder,
  PermissionFlagsBits,
  EmbedBuilder,
  ActionRowBuilder,
  ButtonBuilder,
  ButtonStyle,
  ChannelType,
  MessageFlags
} = require('discord.js');

const { getConfig } = require('../services/guildConfigService');

const ACCENT = 0x9d4bff;
const TICKET_PREFIX = 'ticket-';

const builders = [
  new SlashCommandBuilder()
    .setName('ticketpanel')
    .setDescription('Post the ticket panel in this channel (owner only)')
    .setDefaultMemberPermissions(PermissionFlagsBits.Administrator),

  new SlashCommandBuilder()
    .setName('close')
    .setDescription('Close the current ticket channel')
    .setDefaultMemberPermissions(PermissionFlagsBits.ManageChannels)
];

const commandNames = new Set(['ticketpanel', 'close']);

function panelPayload() {
  const embed = new EmbedBuilder()
    .setColor(ACCENT)
    .setTitle('🎫 Support')
    .setDescription('Need help? Click the button below to open a private ticket with the staff.');
  const row = new ActionRowBuilder().addComponents(
    new ButtonBuilder().setCustomId('ticket:open').setLabel('Open a ticket').setEmoji('🎫').setStyle(ButtonStyle.Primary)
  );
  return { embeds: [embed], components: [row] };
}

// Find an already-open ticket owned by this member (topic stores their id).
function findExistingTicket(guild, userId) {
  return guild.channels.cache.find(
    (c) => c.type === ChannelType.GuildText
      && c.name.startsWith(TICKET_PREFIX)
      && c.topic === `ticket-owner:${userId}`
  );
}

async function openTicket(interaction) {
  const guild = interaction.guild;
  const me = guild.members.me;
  if (!me || !me.permissions.has(PermissionFlagsBits.ManageChannels)) {
    return interaction.reply({ content: '❌ I lack the **Manage Channels** permission.', flags: MessageFlags.Ephemeral });
  }

  const existing = findExistingTicket(guild, interaction.user.id);
  if (existing) {
    return interaction.reply({ content: `❌ You already have an open ticket: ${existing}.`, flags: MessageFlags.Ephemeral });
  }

  const config = await getConfig(guild.id);
  const overwrites = [
    { id: guild.roles.everyone.id, deny: [PermissionFlagsBits.ViewChannel] },
    {
      id: interaction.user.id,
      allow: [PermissionFlagsBits.ViewChannel, PermissionFlagsBits.SendMessages, PermissionFlagsBits.ReadMessageHistory, PermissionFlagsBits.AttachFiles]
    },
    {
      id: me.id,
      allow: [PermissionFlagsBits.ViewChannel, PermissionFlagsBits.SendMessages, PermissionFlagsBits.ManageChannels]
    }
  ];
  if (config.staffRoleId) {
    overwrites.push({
      id: config.staffRoleId,
      allow: [PermissionFlagsBits.ViewChannel, PermissionFlagsBits.SendMessages, PermissionFlagsBits.ReadMessageHistory]
    });
  }

  const channel = await guild.channels.create({
    name: `${TICKET_PREFIX}${interaction.user.username}`.slice(0, 90).toLowerCase(),
    type: ChannelType.GuildText,
    parent: config.ticketCategoryId || null,
    topic: `ticket-owner:${interaction.user.id}`,
    permissionOverwrites: overwrites,
    reason: `Ticket opened by ${interaction.user.tag}`
  }).catch((err) => {
    console.error('[bot] ticket create:', err.message);
    return null;
  });

  if (!channel) {
    return interaction.reply({ content: '❌ Could not create the ticket channel.', flags: MessageFlags.Ephemeral });
  }

  const embed = new EmbedBuilder()
    .setColor(ACCENT)
    .setTitle('🎫 Ticket opened')
    .setDescription(`${interaction.user}, describe your issue and the staff will help you shortly.`);
  const row = new ActionRowBuilder().addComponents(
    new ButtonBuilder().setCustomId('ticket:close').setLabel('Close ticket').setEmoji('🔒').setStyle(ButtonStyle.Danger)
  );
  const mention = config.staffRoleId ? `<@&${config.staffRoleId}>` : '';
  await channel.send({
    content: `${interaction.user} ${mention}`.trim(),
    embeds: [embed],
    components: [row],
    allowedMentions: { users: [interaction.user.id], roles: config.staffRoleId ? [config.staffRoleId] : [] }
  });

  return interaction.reply({ content: `✅ Ticket created: ${channel}.`, flags: MessageFlags.Ephemeral });
}

async function closeTicket(interaction) {
  const channel = interaction.channel;
  if (!channel || channel.type !== ChannelType.GuildText || !channel.name.startsWith(TICKET_PREFIX)) {
    return interaction.reply({ content: '❌ This is not a ticket channel.', flags: MessageFlags.Ephemeral });
  }
  await interaction.reply({ embeds: [new EmbedBuilder().setColor(ACCENT).setDescription('🔒 Closing this ticket in 5 seconds…')] });
  setTimeout(() => {
    channel.delete(`Ticket closed by ${interaction.user.tag}`).catch(() => {});
  }, 5000);
}

// Slash command entry point.
async function handleTicketCommand(interaction) {
  if (interaction.commandName === 'ticketpanel') {
    await interaction.channel.send(panelPayload());
    await interaction.reply({ content: '✅ Ticket panel posted.', flags: MessageFlags.Ephemeral });
    return true;
  }
  if (interaction.commandName === 'close') {
    await closeTicket(interaction);
    return true;
  }
  return false;
}

// Button entry point (ns === 'ticket').
async function handleTicketButton(interaction, action) {
  if (action === 'open') return openTicket(interaction);
  if (action === 'close') return closeTicket(interaction);
  return false;
}

module.exports = {
  ticketCommands: builders.map((b) => b.toJSON()),
  ticketCommandNames: commandNames,
  handleTicketCommand,
  handleTicketButton
};
