const {
  SlashCommandBuilder,
  PermissionFlagsBits,
  EmbedBuilder,
  MessageFlags
} = require('discord.js');

const { getUserLevel, getUserRank, countRanked, progress } = require('../services/levelService');

const OK = 0x25ff85;
const ERR = 0xff3b6b;
const INFO = 0x9d4bff;

function embed(color, title, description) {
  const e = new EmbedBuilder().setColor(color).setTitle(title);
  if (description) e.setDescription(description);
  return e;
}

async function replyOk(interaction, title, description) {
  const payload = { embeds: [embed(OK, title, description)] };
  return interaction.deferred || interaction.replied
    ? interaction.editReply(payload)
    : interaction.reply(payload);
}

async function replyErr(interaction, message) {
  const payload = { embeds: [embed(ERR, '❌ Error', message)], flags: MessageFlags.Ephemeral };
  return interaction.deferred || interaction.replied
    ? interaction.editReply({ embeds: payload.embeds })
    : interaction.reply(payload);
}

// "10m", "2h", "1d", "45s" -> milliseconds (max 28 days for timeouts).
function parseDuration(input) {
  const match = /^(\d+)\s*(s|m|h|d)$/i.exec(String(input || '').trim());
  if (!match) return null;
  const value = Number(match[1]);
  const unit = match[2].toLowerCase();
  const mult = { s: 1000, m: 60000, h: 3600000, d: 86400000 }[unit];
  return value * mult;
}

// ---------------------------------------------------------------------------
// Command definitions
// ---------------------------------------------------------------------------

const builders = [
  new SlashCommandBuilder()
    .setName('kick')
    .setDescription('Kick a member from the server')
    .setDefaultMemberPermissions(PermissionFlagsBits.KickMembers)
    .addUserOption((o) => o.setName('member').setDescription('Member to kick').setRequired(true))
    .addStringOption((o) => o.setName('reason').setDescription('Reason')),

  new SlashCommandBuilder()
    .setName('ban')
    .setDescription('Ban a member from the server')
    .setDefaultMemberPermissions(PermissionFlagsBits.BanMembers)
    .addUserOption((o) => o.setName('member').setDescription('Member to ban').setRequired(true))
    .addStringOption((o) => o.setName('reason').setDescription('Reason'))
    .addIntegerOption((o) =>
      o.setName('delete_days').setDescription('Delete messages from the last N days (0-7)').setMinValue(0).setMaxValue(7)),

  new SlashCommandBuilder()
    .setName('unban')
    .setDescription('Unban a user by ID')
    .setDefaultMemberPermissions(PermissionFlagsBits.BanMembers)
    .addStringOption((o) => o.setName('user_id').setDescription('User ID').setRequired(true))
    .addStringOption((o) => o.setName('reason').setDescription('Reason')),

  new SlashCommandBuilder()
    .setName('timeout')
    .setDescription('Mute a member for a duration (e.g. 10m, 2h, 1d)')
    .setDefaultMemberPermissions(PermissionFlagsBits.ModerateMembers)
    .addUserOption((o) => o.setName('member').setDescription('Member').setRequired(true))
    .addStringOption((o) => o.setName('duration').setDescription('Duration: 45s, 10m, 2h, 1d').setRequired(true))
    .addStringOption((o) => o.setName('reason').setDescription('Reason')),

  new SlashCommandBuilder()
    .setName('untimeout')
    .setDescription('Remove a member\'s timeout')
    .setDefaultMemberPermissions(PermissionFlagsBits.ModerateMembers)
    .addUserOption((o) => o.setName('member').setDescription('Member').setRequired(true)),

  new SlashCommandBuilder()
    .setName('clear')
    .setDescription('Delete messages from the channel (1-100)')
    .setDefaultMemberPermissions(PermissionFlagsBits.ManageMessages)
    .addIntegerOption((o) => o.setName('amount').setDescription('Number of messages').setRequired(true).setMinValue(1).setMaxValue(100)),

  new SlashCommandBuilder()
    .setName('slowmode')
    .setDescription('Set the channel slowmode (seconds, 0 = off)')
    .setDefaultMemberPermissions(PermissionFlagsBits.ManageChannels)
    .addIntegerOption((o) => o.setName('seconds').setDescription('0 to 21600').setRequired(true).setMinValue(0).setMaxValue(21600)),

  new SlashCommandBuilder()
    .setName('nick')
    .setDescription('Change a member\'s nickname')
    .setDefaultMemberPermissions(PermissionFlagsBits.ManageNicknames)
    .addUserOption((o) => o.setName('member').setDescription('Member').setRequired(true))
    .addStringOption((o) => o.setName('nickname').setDescription('New nickname (empty = reset)')),

  new SlashCommandBuilder()
    .setName('say')
    .setDescription('Make the bot speak in the channel')
    .setDefaultMemberPermissions(PermissionFlagsBits.ManageMessages)
    .addStringOption((o) => o.setName('message').setDescription('Message').setRequired(true)),

  new SlashCommandBuilder()
    .setName('role')
    .setDescription('Role management')
    .setDefaultMemberPermissions(PermissionFlagsBits.ManageRoles)
    .addSubcommand((s) =>
      s.setName('add').setDescription('Add a role to a member')
        .addUserOption((o) => o.setName('member').setDescription('Member').setRequired(true))
        .addRoleOption((o) => o.setName('role').setDescription('Role').setRequired(true)))
    .addSubcommand((s) =>
      s.setName('remove').setDescription('Remove a role from a member')
        .addUserOption((o) => o.setName('member').setDescription('Member').setRequired(true))
        .addRoleOption((o) => o.setName('role').setDescription('Role').setRequired(true)))
    .addSubcommand((s) =>
      s.setName('create').setDescription('Create a role')
        .addStringOption((o) => o.setName('name').setDescription('Role name').setRequired(true))
        .addStringOption((o) => o.setName('color').setDescription('Hex e.g. #9d4bff')))
    .addSubcommand((s) =>
      s.setName('delete').setDescription('Delete a role')
        .addRoleOption((o) => o.setName('role').setDescription('Role').setRequired(true))),

  new SlashCommandBuilder()
    .setName('userinfo')
    .setDescription('Info about a member')
    .addUserOption((o) => o.setName('member').setDescription('Member (yourself by default)')),

  new SlashCommandBuilder()
    .setName('serverinfo')
    .setDescription('Info about the server')
];

const commandNames = new Set(builders.map((b) => b.name));

// ---------------------------------------------------------------------------
// Handlers
// ---------------------------------------------------------------------------

async function handleKick(interaction) {
  const member = interaction.options.getMember('member');
  const reason = interaction.options.getString('reason') || 'No reason';
  if (!member) return replyErr(interaction, 'Member not found on this server.');
  if (!member.kickable) return replyErr(interaction, "I can't kick this member (role too high or missing permission).");
  await member.kick(reason);
  return replyOk(interaction, '👢 Member kicked', `${member.user.tag} has been kicked.\n**Reason:** ${reason}`);
}

async function handleBan(interaction) {
  const user = interaction.options.getUser('member');
  const member = interaction.options.getMember('member');
  const reason = interaction.options.getString('reason') || 'No reason';
  const days = interaction.options.getInteger('delete_days') || 0;
  if (member && !member.bannable) return replyErr(interaction, "I can't ban this member (role too high or missing permission).");
  await interaction.guild.members.ban(user.id, { reason, deleteMessageSeconds: days * 86400 });
  return replyOk(interaction, '🔨 Member banned', `${user.tag} has been banned.\n**Reason:** ${reason}`);
}

async function handleUnban(interaction) {
  const userId = interaction.options.getString('user_id').trim();
  const reason = interaction.options.getString('reason') || 'No reason';
  try {
    await interaction.guild.bans.remove(userId, reason);
  } catch (err) {
    return replyErr(interaction, `Could not unban (${err.message}). Check the ID.`);
  }
  return replyOk(interaction, '♻️ Unbanned', `User \`${userId}\` has been unbanned.`);
}

async function handleTimeout(interaction) {
  const member = interaction.options.getMember('member');
  const durationStr = interaction.options.getString('duration');
  const reason = interaction.options.getString('reason') || 'No reason';
  if (!member) return replyErr(interaction, 'Member not found.');
  if (!member.moderatable) return replyErr(interaction, "I can't time out this member (role too high or missing permission).");
  const ms = parseDuration(durationStr);
  if (!ms) return replyErr(interaction, 'Invalid duration. Formats: 45s, 10m, 2h, 1d.');
  const capped = Math.min(ms, 28 * 86400000);
  await member.timeout(capped, reason);
  return replyOk(interaction, '⏳ Timeout applied', `${member.user.tag} is timed out for **${durationStr}**.\n**Reason:** ${reason}`);
}

async function handleUntimeout(interaction) {
  const member = interaction.options.getMember('member');
  if (!member) return replyErr(interaction, 'Member not found.');
  await member.timeout(null);
  return replyOk(interaction, '✅ Timeout removed', `${member.user.tag} is no longer timed out.`);
}

async function handleClear(interaction) {
  const count = interaction.options.getInteger('amount');
  if (!interaction.channel || typeof interaction.channel.bulkDelete !== 'function') {
    return replyErr(interaction, 'This channel does not support bulk deletion.');
  }
  const deleted = await interaction.channel.bulkDelete(count, true);
  return interaction.reply({
    embeds: [embed(OK, '🧹 Messages deleted', `${deleted.size} message(s) deleted.`)],
    flags: MessageFlags.Ephemeral
  });
}

async function handleSlowmode(interaction) {
  const seconds = interaction.options.getInteger('seconds');
  await interaction.channel.setRateLimitPerUser(seconds);
  return replyOk(interaction, '🐌 Slowmode', seconds === 0 ? 'Slowmode disabled.' : `Slowmode set to ${seconds}s.`);
}

async function handleNick(interaction) {
  const member = interaction.options.getMember('member');
  const nick = interaction.options.getString('nickname') || null;
  if (!member) return replyErr(interaction, 'Member not found.');
  if (!member.manageable) return replyErr(interaction, "I can't change this member's nickname (role too high).");
  await member.setNickname(nick);
  return replyOk(interaction, '✏️ Nickname changed', `${member.user.tag} -> **${nick || '(reset)'}**`);
}

async function handleSay(interaction) {
  const message = interaction.options.getString('message');
  await interaction.channel.send({ content: message, allowedMentions: { parse: [] } });
  return interaction.reply({ embeds: [embed(OK, '📨 Sent', null)], flags: MessageFlags.Ephemeral });
}

async function handleRole(interaction) {
  const sub = interaction.options.getSubcommand();
  const me = interaction.guild.members.me;

  if (sub === 'add' || sub === 'remove') {
    const member = interaction.options.getMember('member');
    const role = interaction.options.getRole('role');
    if (!member) return replyErr(interaction, 'Member not found.');
    if (role.position >= me.roles.highest.position) {
      return replyErr(interaction, `The role ${role} is above mine, I can't manage it.`);
    }
    if (sub === 'add') {
      await member.roles.add(role);
      return replyOk(interaction, '✅ Role added', `${role} added to ${member.user.tag}.`);
    }
    await member.roles.remove(role);
    return replyOk(interaction, '✅ Role removed', `${role} removed from ${member.user.tag}.`);
  }

  if (sub === 'create') {
    const name = interaction.options.getString('name');
    const colorStr = interaction.options.getString('color');
    let color;
    if (colorStr) {
      const parsed = parseInt(colorStr.replace('#', ''), 16);
      if (!Number.isNaN(parsed)) color = parsed;
    }
    const role = await interaction.guild.roles.create({ name, color, reason: `Created by ${interaction.user.tag}` });
    return replyOk(interaction, '✅ Role created', `${role} created.`);
  }

  if (sub === 'delete') {
    const role = interaction.options.getRole('role');
    if (role.position >= me.roles.highest.position) {
      return replyErr(interaction, `The role ${role} is above mine, I can't delete it.`);
    }
    const name = role.name;
    await role.delete(`Deleted by ${interaction.user.tag}`);
    return replyOk(interaction, '🗑️ Role deleted', `The role **${name}** has been deleted.`);
  }
  return replyErr(interaction, 'Unknown subcommand.');
}

// Discord timestamp helpers. F = full date+time, R = relative.
function tsFull(dateOrMs) {
  const ms = dateOrMs instanceof Date ? dateOrMs.getTime() : Number(dateOrMs);
  return `<t:${Math.floor(ms / 1000)}:F>`;
}
function tsBoth(dateOrMs) {
  const ms = dateOrMs instanceof Date ? dateOrMs.getTime() : Number(dateOrMs);
  const unix = Math.floor(ms / 1000);
  return `<t:${unix}:F> (<t:${unix}:R>)`;
}

// Key permissions worth surfacing, mapped to readable labels.
const KEY_PERMS = [
  [PermissionFlagsBits.Administrator, 'Administrator'],
  [PermissionFlagsBits.ManageGuild, 'Manage Server'],
  [PermissionFlagsBits.BanMembers, 'Ban Members'],
  [PermissionFlagsBits.KickMembers, 'Kick Members'],
  [PermissionFlagsBits.ModerateMembers, 'Timeout'],
  [PermissionFlagsBits.ManageMessages, 'Manage Messages'],
  [PermissionFlagsBits.ManageRoles, 'Manage Roles'],
  [PermissionFlagsBits.ManageChannels, 'Manage Channels']
];

async function handleUserinfo(interaction) {
  const member = interaction.options.getMember('member') || interaction.member;
  const user = member.user;

  const roles = member.roles.cache
    .filter((r) => r.id !== interaction.guild.id)
    .sort((a, b) => b.position - a.position)
    .map((r) => r.toString())
    .slice(0, 20)
    .join(' ') || 'None';

  // Leveling stats for this member on this guild.
  const levelDoc = await getUserLevel(interaction.guildId, user.id);
  let levelValue = 'No activity yet';
  if (levelDoc) {
    const p = progress(levelDoc.xp);
    const rank = await getUserRank(interaction.guildId, user.id);
    const total = await countRanked(interaction.guildId);
    levelValue = `**Level ${p.level}** · Rank #${rank}/${total}\n${levelDoc.xp.toLocaleString('en-US')} XP · ${levelDoc.messageCount.toLocaleString('en-US')} messages`;
  }

  // Notable permissions held on the server.
  const perms = member.permissions;
  const grantedPerms = KEY_PERMS.filter(([flag]) => perms.has(flag)).map(([, label]) => label);
  const permsValue = perms.has(PermissionFlagsBits.Administrator)
    ? 'Administrator (all)'
    : (grantedPerms.length ? grantedPerms.join(', ') : 'None notable');

  // Highest role as an at-a-glance identity marker.
  const topRole = member.roles.highest && member.roles.highest.id !== interaction.guild.id
    ? member.roles.highest.toString()
    : 'None';

  const e = embed(INFO, `👤 ${user.tag}`)
    .setThumbnail(user.displayAvatarURL({ size: 256 }))
    .addFields(
      { name: 'User', value: `${user} \`${user.id}\`` },
      { name: 'Account created', value: tsBoth(user.createdTimestamp), inline: false },
      { name: 'Joined server', value: member.joinedTimestamp ? tsBoth(member.joinedTimestamp) : '—', inline: false }
    );

  // Boosting status, only when relevant.
  if (member.premiumSinceTimestamp) {
    e.addFields({ name: 'Boosting since', value: tsFull(member.premiumSinceTimestamp), inline: true });
  }
  // Active timeout, only when relevant.
  if (member.communicationDisabledUntilTimestamp && member.communicationDisabledUntilTimestamp > Date.now()) {
    e.addFields({ name: '⏳ Timed out until', value: tsFull(member.communicationDisabledUntilTimestamp), inline: true });
  }

  e.addFields(
    { name: '📊 Progression', value: levelValue, inline: false },
    { name: 'Top role', value: topRole, inline: true },
    { name: 'Key permissions', value: permsValue, inline: false },
    { name: `Roles (${member.roles.cache.size - 1})`, value: roles }
  );

  return interaction.reply({ embeds: [e] });
}

async function handleServerinfo(interaction) {
  const g = interaction.guild;
  const owner = await g.fetchOwner().catch(() => null);
  const e = embed(INFO, `🏠 ${g.name}`)
    .setThumbnail(g.iconURL())
    .addFields(
      { name: 'ID', value: g.id, inline: true },
      { name: 'Owner', value: owner ? owner.user.tag : '—', inline: true },
      { name: 'Created', value: tsFull(g.createdTimestamp), inline: true },
      { name: 'Members', value: String(g.memberCount), inline: true },
      { name: 'Channels', value: String(g.channels.cache.size), inline: true },
      { name: 'Roles', value: String(g.roles.cache.size), inline: true },
      { name: 'Boosts', value: `${g.premiumSubscriptionCount || 0} (level ${g.premiumTier})`, inline: true }
    );
  return interaction.reply({ embeds: [e] });
}

const handlers = {
  kick: handleKick,
  ban: handleBan,
  unban: handleUnban,
  timeout: handleTimeout,
  untimeout: handleUntimeout,
  clear: handleClear,
  slowmode: handleSlowmode,
  nick: handleNick,
  say: handleSay,
  role: handleRole,
  userinfo: handleUserinfo,
  serverinfo: handleServerinfo
};

async function handleModerationCommand(interaction) {
  const handler = handlers[interaction.commandName];
  if (!handler) return false;
  if (!interaction.inGuild()) {
    await replyErr(interaction, 'This command must be used in a server.');
    return true;
  }
  // inGuild() only guarantees guildId, not the cached guild object. With only
  // the Guilds intent (or right after a restart) the guild may not be cached,
  // and interaction.guild is null -> every handler crashes. Resolve it.
  if (!interaction.guild) {
    try {
      await interaction.client.guilds.fetch(interaction.guildId);
    } catch (err) {
      await replyErr(interaction, `Server unavailable (${err.message}). Try again in a moment.`);
      return true;
    }
  }
  // Some handlers use interaction.guild.members.me (role management).
  // members.me can be null if the guild was just fetched cold.
  if (interaction.guild && !interaction.guild.members.me) {
    await interaction.guild.members.fetchMe().catch(() => {});
  }
  await handler(interaction);
  return true;
}

module.exports = {
  moderationCommands: builders.map((b) => b.toJSON()),
  moderationCommandNames: commandNames,
  handleModerationCommand
};
