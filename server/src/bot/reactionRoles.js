const {
  SlashCommandBuilder,
  PermissionFlagsBits,
  EmbedBuilder,
  ChannelType,
  MessageFlags
} = require('discord.js');

const ReactionRolePanel = require('../models/ReactionRolePanel');

const ACCENT = 0x9d4bff;
const MAX_PAIRS = 6;

// Parse an emoji string into { key, reactable }.
// Custom emoji <:name:id> / <a:name:id> -> key=id, reactable="name:id".
// Unicode emoji -> key=char, reactable=char.
function parseEmoji(input) {
  const raw = (input || '').trim();
  const custom = /^<(a?):(\w+):(\d+)>$/.exec(raw);
  if (custom) {
    return { key: custom[3], reactable: `${custom[2]}:${custom[3]}` };
  }
  return { key: raw, reactable: raw };
}

// Emoji key from a live reaction (custom id, else unicode name).
function reactionKey(reaction) {
  return reaction.emoji.id || reaction.emoji.name;
}

// /rolepanel: channel + up to 6 emoji/role pairs. Required options first.
const builders = [
  (() => {
    const b = new SlashCommandBuilder()
      .setName('rolepanel')
      .setDescription('Post an emoji reaction-role panel (owner only)')
      .setDefaultMemberPermissions(PermissionFlagsBits.Administrator)
      .addChannelOption((o) =>
        o.setName('channel').setDescription('Channel to post the panel in')
          .addChannelTypes(ChannelType.GuildText).setRequired(true))
      .addStringOption((o) => o.setName('emoji1').setDescription('Emoji for role 1').setRequired(true))
      .addRoleOption((o) => o.setName('role1').setDescription('Role 1').setRequired(true))
      .addStringOption((o) => o.setName('title').setDescription('Panel title (optional)'));
    for (let i = 2; i <= MAX_PAIRS; i += 1) {
      b.addStringOption((o) => o.setName(`emoji${i}`).setDescription(`Emoji for role ${i}`));
      b.addRoleOption((o) => o.setName(`role${i}`).setDescription(`Role ${i}`));
    }
    return b;
  })()
];

const commandNames = new Set(['rolepanel']);

async function handleRolePanelCommand(interaction) {
  if (interaction.commandName !== 'rolepanel') return false;

  const channel = interaction.options.getChannel('channel');
  const title = interaction.options.getString('title') || '🎭 Reaction roles';
  const me = interaction.guild.members.me;

  // Collect emoji/role pairs.
  const pairs = [];
  for (let i = 1; i <= MAX_PAIRS; i += 1) {
    const emoji = interaction.options.getString(`emoji${i}`);
    const role = interaction.options.getRole(`role${i}`);
    if (!emoji || !role) continue;
    if (role.id === interaction.guild.id) continue;
    if (role.position >= me.roles.highest.position) {
      return interaction.reply({ content: `❌ The role ${role} is above mine, I can't assign it. Move my role above it in Server Settings → Roles.`, flags: MessageFlags.Ephemeral });
    }
    pairs.push({ emoji: parseEmoji(emoji), display: emoji.trim(), roleId: role.id, roleName: role.name });
  }
  if (!pairs.length) {
    return interaction.reply({ content: '❌ Provide at least one emoji + role pair.', flags: MessageFlags.Ephemeral });
  }

  const lines = pairs.map((p) => `${p.display} → <@&${p.roleId}>`);
  const embed = new EmbedBuilder()
    .setColor(ACCENT)
    .setTitle(title)
    .setDescription(['React to get a role — remove your reaction to lose it.', '', ...lines].join('\n'));

  const message = await channel.send({ embeds: [embed] }).catch((err) => {
    console.error('[bot] rolepanel send:', err.message);
    return null;
  });
  if (!message) {
    return interaction.reply({ content: '❌ Could not post in that channel (missing permissions?).', flags: MessageFlags.Ephemeral });
  }

  // React with each emoji so members just click.
  for (const p of pairs) {
    await message.react(p.emoji.reactable).catch((err) =>
      console.error(`[bot] rolepanel react ${p.emoji.reactable}:`, err.message));
  }

  await ReactionRolePanel.create({
    guildId: interaction.guild.id,
    channelId: channel.id,
    messageId: message.id,
    mappings: pairs.map((p) => ({ emojiKey: p.emoji.key, roleId: p.roleId }))
  });

  return interaction.reply({ content: `✅ Reaction-role panel posted in ${channel}.`, flags: MessageFlags.Ephemeral });
}

// Resolve which role an emoji maps to on a given panel message.
async function roleForReaction(reaction) {
  const panel = await ReactionRolePanel.findOne({ messageId: reaction.message.id });
  if (!panel) return null;
  const key = reactionKey(reaction);
  const mapping = panel.mappings.find((m) => m.emojiKey === key);
  return mapping ? mapping.roleId : null;
}

// Attach add/remove reaction listeners for role toggling.
function attachReactionRoles(client) {
  client.on('messageReactionAdd', async (reaction, user) => {
    try {
      if (user.bot) return;
      if (reaction.partial) await reaction.fetch().catch(() => {});
      const roleId = await roleForReaction(reaction);
      if (!roleId) return;
      const guild = reaction.message.guild;
      const member = await guild.members.fetch(user.id).catch(() => null);
      const me = guild.members.me;
      const role = guild.roles.cache.get(roleId);
      if (!member || !role || role.position >= me.roles.highest.position) return;
      await member.roles.add(roleId, 'Reaction role').catch(() => {});
    } catch (err) {
      console.error('[bot] reaction add:', err.message);
    }
  });

  client.on('messageReactionRemove', async (reaction, user) => {
    try {
      if (user.bot) return;
      if (reaction.partial) await reaction.fetch().catch(() => {});
      const roleId = await roleForReaction(reaction);
      if (!roleId) return;
      const guild = reaction.message.guild;
      const member = await guild.members.fetch(user.id).catch(() => null);
      const me = guild.members.me;
      const role = guild.roles.cache.get(roleId);
      if (!member || !role || role.position >= me.roles.highest.position) return;
      await member.roles.remove(roleId, 'Reaction role').catch(() => {});
    } catch (err) {
      console.error('[bot] reaction remove:', err.message);
    }
  });
}

module.exports = {
  rolePanelCommands: builders.map((b) => b.toJSON()),
  rolePanelCommandNames: commandNames,
  handleRolePanelCommand,
  attachReactionRoles
};
