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

const ReactionRolePanel = require('../models/ReactionRolePanel');

const ACCENT = 0x9d4bff;

// Emoji pool assigned to roles in order (enough for a large role list).
const EMOJI_POOL = [
  '🔵', '🟢', '🟣', '🟡', '🟠', '🔴', '⚪', '🟤',
  '⭐', '✨', '🔥', '⚡', '🎯', '🎮', '🎨', '🎵',
  '💎', '👑', '🛡️', '⚔️', '🏆', '🚀', '🌙', '☀️',
  '🍀', '🌸', '🦊', '🐺', '🐉', '🦅', '🐍', '🎲'
];

// Roles whose name contains any of these are "protected": clicking requests
// owner approval instead of self-assigning. (agencymember, staff, mod, admin,
// owner, moderator, modder...)
const PROTECTED_KEYWORDS = ['owner', 'admin', 'staff', 'moderator', 'modder', 'agencymember', 'mod'];
// Roles never offered in the panel at all (punishment / system roles).
const EXCLUDED_KEYWORDS = ['muted', 'mute', 'everyone'];

function nameMatches(name, keywords) {
  const lower = name.toLowerCase();
  return keywords.some((k) => lower.includes(k));
}

// Emoji key from a live reaction (custom id, else unicode name).
function reactionKey(reaction) {
  return reaction.emoji.id || reaction.emoji.name;
}

const builders = [
  new SlashCommandBuilder()
    .setName('rolepanel')
    .setDescription('Auto-build a reaction-role panel from the server roles (owner only)')
    .setDefaultMemberPermissions(PermissionFlagsBits.Administrator)
    .addChannelOption((o) =>
      o.setName('channel').setDescription('Channel to post the panel in')
        .addChannelTypes(ChannelType.GuildText).setRequired(true))
];

const commandNames = new Set(['rolepanel']);

let ownerIdGetter = () => null;

async function handleRolePanelCommand(interaction) {
  if (interaction.commandName !== 'rolepanel') return false;

  const channel = interaction.options.getChannel('channel');
  const guild = interaction.guild;
  const me = guild.members.me;

  await interaction.deferReply({ flags: MessageFlags.Ephemeral });

  // Auto-select assignable roles: below the bot, not managed (bot/integration),
  // not @everyone, not excluded. Highest first for a nicer display.
  const roles = [...guild.roles.cache.values()]
    .filter((r) => r.id !== guild.id && !r.managed && r.position < me.roles.highest.position)
    .filter((r) => !nameMatches(r.name, EXCLUDED_KEYWORDS))
    .sort((a, b) => b.position - a.position)
    .slice(0, EMOJI_POOL.length);

  if (!roles.length) {
    return interaction.editReply({ content: '❌ No assignable roles found. Make sure my role is **above** the roles you want to hand out (Server Settings → Roles).' });
  }

  // Build emoji->role mappings, flag protected ones.
  const pairs = roles.map((role, i) => ({
    emoji: EMOJI_POOL[i],
    roleId: role.id,
    roleName: role.name,
    protected: nameMatches(role.name, PROTECTED_KEYWORDS)
  }));

  const lines = pairs.map((p) =>
    `${p.emoji} → <@&${p.roleId}>${p.protected ? '  🔒 *(approval required)*' : ''}`);

  const embed = new EmbedBuilder()
    .setColor(ACCENT)
    .setTitle('🎭 Reaction roles')
    .setDescription([
      'React to get a role — remove your reaction to lose it.',
      '🔒 roles need owner approval before you receive them.',
      '',
      ...lines
    ].join('\n'));

  const message = await channel.send({ embeds: [embed] }).catch((err) => {
    console.error('[bot] rolepanel send:', err.message);
    return null;
  });
  if (!message) {
    return interaction.editReply({ content: '❌ Could not post in that channel (missing permissions?).' });
  }

  for (const p of pairs) {
    await message.react(p.emoji).catch((err) => console.error(`[bot] react ${p.emoji}:`, err.message));
  }

  await ReactionRolePanel.create({
    guildId: guild.id,
    channelId: channel.id,
    messageId: message.id,
    mappings: pairs.map((p) => ({ emojiKey: p.emoji, roleId: p.roleId, protected: p.protected }))
  });

  const protectedCount = pairs.filter((p) => p.protected).length;
  return interaction.editReply({
    content: `✅ Panel posted in ${channel} with **${pairs.length}** roles (${protectedCount} protected).`
  });
}

// Look up the mapping for a reaction on a panel message.
async function mappingForReaction(reaction) {
  const panel = await ReactionRolePanel.findOne({ messageId: reaction.message.id });
  if (!panel) return null;
  const key = reactionKey(reaction);
  return panel.mappings.find((m) => m.emojiKey === key) || null;
}

// Send an approval request for a protected role to the owner.
async function requestApproval(guild, member, role, client) {
  const ownerId = ownerIdGetter();
  const embed = new EmbedBuilder()
    .setColor(0xffb800)
    .setTitle('🔒 Role request')
    .setDescription(`${member} is requesting the **${role.name}** role.`)
    .addFields(
      { name: 'Member', value: `${member} (\`${member.id}\`)`, inline: true },
      { name: 'Role', value: `${role}`, inline: true }
    );
  const row = new ActionRowBuilder().addComponents(
    new ButtonBuilder().setCustomId(`rrapp:ok:${role.id}:${member.id}`).setLabel('Approve').setEmoji('✅').setStyle(ButtonStyle.Success),
    new ButtonBuilder().setCustomId(`rrapp:no:${role.id}:${member.id}`).setLabel('Deny').setEmoji('⛔').setStyle(ButtonStyle.Danger)
  );
  const payload = { content: ownerId ? `<@${ownerId}>` : undefined, embeds: [embed], components: [row], allowedMentions: { users: ownerId ? [ownerId] : [] } };

  // Prefer DM to the owner; that keeps approvals private.
  if (ownerId) {
    const owner = await client.users.fetch(ownerId).catch(() => null);
    if (owner) {
      const sent = await owner.send(payload).catch(() => null);
      if (sent) return true;
    }
  }
  return false;
}

// Owner clicked Approve/Deny on a role request.
async function handleApprovalButton(interaction, approve, roleId, memberId) {
  await interaction.deferUpdate().catch(() => {});
  let content;
  if (approve) {
    // The button may be in a DM: resolve the guild from the role.
    const guild = interaction.client.guilds.cache.find((g) => g.roles.cache.has(roleId));
    if (!guild) { content = '❌ Guild/role no longer available.'; }
    else {
      const member = await guild.members.fetch(memberId).catch(() => null);
      const role = guild.roles.cache.get(roleId);
      const me = guild.members.me;
      if (member && role && role.position < me.roles.highest.position) {
        await member.roles.add(roleId, 'Role request approved').catch(() => {});
        content = `✅ Approved **${role.name}** for <@${memberId}>.`;
      } else {
        content = '❌ Could not assign the role (member left or role too high).';
      }
    }
  } else {
    content = `⛔ Denied the role request for <@${memberId}>.`;
  }
  await interaction.message.edit({ content, embeds: interaction.message.embeds, components: [] }).catch(() => {});
}

function attachReactionRoles(client, { getOwnerId } = {}) {
  if (typeof getOwnerId === 'function') ownerIdGetter = getOwnerId;

  client.on('messageReactionAdd', async (reaction, user) => {
    try {
      if (user.bot) return;
      if (reaction.partial) await reaction.fetch().catch(() => {});
      const mapping = await mappingForReaction(reaction);
      if (!mapping) return;
      const guild = reaction.message.guild;
      const member = await guild.members.fetch(user.id).catch(() => null);
      const me = guild.members.me;
      const role = guild.roles.cache.get(mapping.roleId);
      if (!member || !role || role.position >= me.roles.highest.position) return;

      if (mapping.protected) {
        // Don't grant directly: ask the owner. Remove the member's reaction so
        // they can re-request later, and DM them the pending status.
        await reaction.users.remove(user.id).catch(() => {});
        const ok = await requestApproval(guild, member, role, client);
        await user.send(ok
          ? `🔒 Your request for **${role.name}** was sent to the owner for approval.`
          : `🔒 Your request for **${role.name}** is pending, but I couldn't reach the owner.`).catch(() => {});
        return;
      }

      await member.roles.add(mapping.roleId, 'Reaction role').catch(() => {});
    } catch (err) {
      console.error('[bot] reaction add:', err.message);
    }
  });

  client.on('messageReactionRemove', async (reaction, user) => {
    try {
      if (user.bot) return;
      if (reaction.partial) await reaction.fetch().catch(() => {});
      const mapping = await mappingForReaction(reaction);
      if (!mapping || mapping.protected) return; // protected roles aren't self-removable here
      const guild = reaction.message.guild;
      const member = await guild.members.fetch(user.id).catch(() => null);
      const me = guild.members.me;
      const role = guild.roles.cache.get(mapping.roleId);
      if (!member || !role || role.position >= me.roles.highest.position) return;
      await member.roles.remove(mapping.roleId, 'Reaction role').catch(() => {});
    } catch (err) {
      console.error('[bot] reaction remove:', err.message);
    }
  });
}

module.exports = {
  rolePanelCommands: builders.map((b) => b.toJSON()),
  rolePanelCommandNames: commandNames,
  handleRolePanelCommand,
  handleApprovalButton,
  attachReactionRoles
};
