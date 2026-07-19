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
const VERIFIED_ROLE_NAME = 'Verified';

const DEFAULT_RULES = [
  '**1.** Be respectful. No harassment, hate speech or discrimination.',
  '**2.** No spam, advertising or self-promotion.',
  '**3.** No NSFW or illegal content.',
  '**4.** Use the right channels for the right topics.',
  '**5.** Follow the Discord Terms of Service.'
].join('\n');

const builders = [
  new SlashCommandBuilder()
    .setName('setupverification')
    .setDescription('Post rules + verify gate and lock the server for unverified members (owner only)')
    .setDefaultMemberPermissions(PermissionFlagsBits.Administrator)
    .addChannelOption((o) =>
      o.setName('channel').setDescription('Channel where the rules + verify button go')
        .addChannelTypes(ChannelType.GuildText).setRequired(true))
    .addStringOption((o) => o.setName('rules').setDescription('Custom rules text (use \\n for line breaks)'))
];

const commandNames = new Set(['setupverification']);

// Find or create the Verified role, placed just under the bot's top role.
async function ensureVerifiedRole(guild) {
  const existing = guild.roles.cache.find((r) => r.name === VERIFIED_ROLE_NAME);
  if (existing) return existing;
  return guild.roles.create({
    name: VERIFIED_ROLE_NAME,
    color: 0x57f287,
    reason: 'Verification system'
  });
}

// Lock the server: unverified members (only @everyone) should see ONLY the
// verification channel. Deny ViewChannel to @everyone on every other channel,
// allow it for the Verified role. The verification channel stays visible to all.
async function lockServer(guild, verifyChannel, verifiedRole) {
  const everyone = guild.roles.everyone;
  let locked = 0;

  for (const channel of guild.channels.cache.values()) {
    // Skip threads and the verification channel itself.
    if (channel.id === verifyChannel.id) continue;
    if (!channel.permissionOverwrites) continue;
    if (channel.type === ChannelType.PublicThread || channel.type === ChannelType.PrivateThread) continue;

    try {
      await channel.permissionOverwrites.edit(everyone.id, { ViewChannel: false });
      await channel.permissionOverwrites.edit(verifiedRole.id, { ViewChannel: true });
      locked += 1;
    } catch (err) {
      console.error(`[bot] verification lock ${channel.name}: ${err.message}`);
    }
  }

  // The verification channel: everyone can see it, verified members don't need to.
  try {
    await verifyChannel.permissionOverwrites.edit(everyone.id, { ViewChannel: true, SendMessages: false });
  } catch (err) {
    console.error(`[bot] verification channel perms: ${err.message}`);
  }

  return locked;
}

async function handleSetupVerification(interaction) {
  if (interaction.commandName !== 'setupverification') return false;

  const me = interaction.guild.members.me;
  if (!me.permissions.has(PermissionFlagsBits.ManageRoles) || !me.permissions.has(PermissionFlagsBits.ManageChannels)) {
    return interaction.reply({ content: '❌ I need **Manage Roles** and **Manage Channels**.', flags: MessageFlags.Ephemeral });
  }

  await interaction.deferReply({ flags: MessageFlags.Ephemeral });

  const channel = interaction.options.getChannel('channel');
  const rules = (interaction.options.getString('rules') || DEFAULT_RULES).replace(/\\n/g, '\n');

  const verifiedRole = await ensureVerifiedRole(interaction.guild);
  const locked = await lockServer(interaction.guild, channel, verifiedRole);

  // Persist config.
  const config = await getConfig(interaction.guild.id);
  config.verifiedRoleId = verifiedRole.id;
  config.verificationChannelId = channel.id;
  await config.save();

  // Post the rules + verify button.
  const embed = new EmbedBuilder()
    .setColor(ACCENT)
    .setTitle('📜 Server Rules')
    .setDescription(`${rules}\n\nClick **Accept & Verify** below to agree to the rules and unlock the rest of the server.`)
    .setFooter({ text: 'You must accept the rules to access the server.' });
  const row = new ActionRowBuilder().addComponents(
    new ButtonBuilder().setCustomId('verify:accept').setLabel('Accept & Verify').setEmoji('✅').setStyle(ButtonStyle.Success)
  );
  await channel.send({ embeds: [embed], components: [row] });

  return interaction.editReply({
    content: `✅ Verification set up in ${channel}.\n• Role: ${verifiedRole}\n• Locked **${locked}** channel(s) for unverified members.\n\n⚠️ Make sure my role is **above ${verifiedRole}** in the role list.`
  });
}

// Button handler (customId 'verify:accept'): grant the Verified role.
async function handleVerifyButton(interaction) {
  const config = await getConfig(interaction.guild.id);
  if (!config.verifiedRoleId) {
    return interaction.reply({ content: '❌ Verification is not configured.', flags: MessageFlags.Ephemeral });
  }
  const role = interaction.guild.roles.cache.get(config.verifiedRoleId)
    || await interaction.guild.roles.fetch(config.verifiedRoleId).catch(() => null);
  if (!role) {
    return interaction.reply({ content: '❌ The verified role no longer exists.', flags: MessageFlags.Ephemeral });
  }
  if (interaction.member.roles.cache.has(role.id)) {
    return interaction.reply({ content: '✅ You are already verified.', flags: MessageFlags.Ephemeral });
  }
  const me = interaction.guild.members.me;
  if (role.position >= me.roles.highest.position) {
    return interaction.reply({ content: "❌ I can't assign the verified role (it's above mine). Ask an admin.", flags: MessageFlags.Ephemeral });
  }
  await interaction.member.roles.add(role, 'Accepted the rules');
  return interaction.reply({ content: '✅ You are now verified — welcome! The rest of the server is unlocked.', flags: MessageFlags.Ephemeral });
}

module.exports = {
  verificationCommands: builders.map((b) => b.toJSON()),
  verificationCommandNames: commandNames,
  handleSetupVerification,
  handleVerifyButton
};
