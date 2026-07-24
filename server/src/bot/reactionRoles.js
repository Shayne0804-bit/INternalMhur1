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

const { findLicenseByDiscordUser } = require('../services/licenseService');

const ACCENT = 0x9d4bff;

// Emoji pool used to decorate normal role buttons (in order).
const EMOJI_POOL = [
  '🔵', '🟢', '🟣', '🟡', '🟠', '🔴', '⚪', '🟤',
  '⭐', '✨', '🔥', '⚡', '🎯', '🎮', '🎨', '🎵',
  '💎', '👑', '🚀', '🌙', '☀️', '🍀', '🌸', '🦊', '🎲'
];

// Discord allows at most 25 buttons per message (5 rows x 5).
const MAX_BUTTONS = 25;

// Roles whose name contains any of these are "protected": clicking requests
// owner approval instead of self-assigning.
const PROTECTED_KEYWORDS = ['owner', 'admin', 'staff', 'moderator', 'modder', 'agencymember', 'mod'];
// Roles gated behind a valid linked license (e.g. vip).
const LICENSE_KEYWORDS = ['vip'];
// Roles never offered in the panel at all (punishment / system roles).
const EXCLUDED_KEYWORDS = ['muted', 'mute', 'everyone'];

function nameMatches(name, keywords) {
  const lower = name.toLowerCase();
  return keywords.some((k) => lower.includes(k));
}

function roleKind(role) {
  if (nameMatches(role.name, PROTECTED_KEYWORDS)) return 'protected';
  if (nameMatches(role.name, LICENSE_KEYWORDS)) return 'license';
  return 'normal';
}

const builders = [
  new SlashCommandBuilder()
    .setName('rolepanel')
    .setDescription('Auto-build a button role panel from the server roles (owner only)')
    .setDefaultMemberPermissions(PermissionFlagsBits.Administrator)
    .addChannelOption((o) =>
      o.setName('channel').setDescription('Channel to post the panel in')
        .addChannelTypes(ChannelType.GuildText).setRequired(true))
];

const commandNames = new Set(['rolepanel']);

let ownerIdGetter = () => null;

// Reply helper: always ephemeral (only the clicker sees it).
function ephemeral(interaction, content) {
  const method = interaction.deferred || interaction.replied ? 'editReply' : 'reply';
  return interaction[method]({ content, flags: MessageFlags.Ephemeral }).catch(() => {});
}

async function handleRolePanelCommand(interaction) {
  if (interaction.commandName !== 'rolepanel') return false;

  const channel = interaction.options.getChannel('channel');
  const guild = interaction.guild;
  const me = guild.members.me;

  await interaction.deferReply({ flags: MessageFlags.Ephemeral });

  // Assignable roles: below the bot, not managed, not @everyone, not excluded.
  const roles = [...guild.roles.cache.values()]
    .filter((r) => r.id !== guild.id && !r.managed && r.position < me.roles.highest.position)
    .filter((r) => !nameMatches(r.name, EXCLUDED_KEYWORDS))
    .sort((a, b) => b.position - a.position)
    .slice(0, MAX_BUTTONS);

  if (!roles.length) {
    return interaction.editReply({ content: '❌ No assignable roles found. Make sure my role is **above** the roles you want to hand out (Server Settings → Roles).' });
  }

  // Build one button per role. Protected -> 🔒 red, license -> 🎫 blurple,
  // normal -> pooled emoji, grey.
  const lines = [];
  const rows = [];
  let current = new ActionRowBuilder();
  roles.forEach((role, i) => {
    const kind = roleKind(role);
    const button = new ButtonBuilder().setCustomId(`rr:${role.id}`).setLabel(role.name.slice(0, 80));
    if (kind === 'protected') {
      button.setEmoji('🔒').setStyle(ButtonStyle.Danger);
      lines.push(`🔒 **${role.name}** — approval required`);
    } else if (kind === 'license') {
      button.setEmoji('🎫').setStyle(ButtonStyle.Primary);
      lines.push(`🎫 **${role.name}** — license required`);
    } else {
      button.setEmoji(EMOJI_POOL[i % EMOJI_POOL.length]).setStyle(ButtonStyle.Secondary);
      lines.push(`${EMOJI_POOL[i % EMOJI_POOL.length]} **${role.name}**`);
    }
    if (current.components.length === 5) {
      rows.push(current);
      current = new ActionRowBuilder();
    }
    current.addComponents(button);
  });
  if (current.components.length) rows.push(current);

  const embed = new EmbedBuilder()
    .setColor(ACCENT)
    .setTitle('🎭 Self-assign roles')
    .setDescription([
      'Click a button to get or remove a role.',
      '🔒 needs owner approval · 🎫 needs a valid linked license.',
      '',
      ...lines
    ].join('\n'));

  const sent = await channel.send({ embeds: [embed], components: rows }).catch((err) => {
    console.error('[bot] rolepanel send:', err.message);
    return null;
  });
  if (!sent) {
    return interaction.editReply({ content: '❌ Could not post in that channel (missing permissions?).' });
  }
  return interaction.editReply({ content: `✅ Panel posted in ${channel} with **${roles.length}** roles.` });
}

// Send an approval request for a protected role to the owner (DM).
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

  if (ownerId) {
    const owner = await client.users.fetch(ownerId).catch(() => null);
    if (owner) {
      const sent = await owner.send(payload).catch(() => null);
      if (sent) return true;
    }
  }
  return false;
}

// Member clicked a role button (customId 'rr:<roleId>').
async function handleRoleButton(interaction, roleId) {
  const guild = interaction.guild;
  const me = guild.members.me;
  const role = guild.roles.cache.get(roleId) || await guild.roles.fetch(roleId).catch(() => null);
  if (!role) return ephemeral(interaction, '❌ That role no longer exists.');
  if (role.position >= me.roles.highest.position) {
    return ephemeral(interaction, "❌ I can't manage that role (it's above mine). Ask an admin.");
  }

  const member = interaction.member;
  const has = member.roles.cache.has(role.id);
  const kind = roleKind(role);

  // Toggle off works for any kind the member already holds.
  if (has) {
    await member.roles.remove(role.id, 'Role panel').catch(() => {});
    return ephemeral(interaction, `➖ Removed **${role.name}**.`);
  }

  if (kind === 'protected') {
    const ok = await requestApproval(guild, member, role, interaction.client);
    return ephemeral(interaction, ok
      ? `🔒 Your request for **${role.name}** was sent to the owner for approval.`
      : `🔒 Request pending, but I couldn't reach the owner.`);
  }

  if (kind === 'license') {
    const license = await findLicenseByDiscordUser(interaction.user.id);
    const valid = license && license.status === 'active' && !license.isExpired();
    if (!valid) {
      return ephemeral(interaction,
        `🎫 The **${role.name}** role requires a valid license linked to your account. ` +
        (license
          ? 'Your linked license is not active (expired or revoked). Renew it, then try again.'
          : 'Use **/check** to link your license key, then click again.'));
    }
    await member.roles.add(role.id, 'VIP: valid linked license').catch(() => {});
    return ephemeral(interaction, `🎫 You now have **${role.name}**!`);
  }

  await member.roles.add(role.id, 'Role panel').catch(() => {});
  return ephemeral(interaction, `➕ Added **${role.name}**.`);
}

// Owner clicked Approve/Deny on a role request (customId 'rrapp:ok|no:role:member').
async function handleApprovalButton(interaction, approve, roleId, memberId) {
  await interaction.deferUpdate().catch(() => {});
  let content;
  if (approve) {
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

module.exports = {
  rolePanelCommands: builders.map((b) => b.toJSON()),
  rolePanelCommandNames: commandNames,
  handleRolePanelCommand,
  handleRoleButton,
  handleApprovalButton
};
