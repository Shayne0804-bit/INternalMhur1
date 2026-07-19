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
  const payload = { embeds: [embed(ERR, '❌ Erreur', message)], flags: MessageFlags.Ephemeral };
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
    .setDescription('Expulse un membre du serveur')
    .setDefaultMemberPermissions(PermissionFlagsBits.KickMembers)
    .addUserOption((o) => o.setName('membre').setDescription('Membre a expulser').setRequired(true))
    .addStringOption((o) => o.setName('raison').setDescription('Raison')),

  new SlashCommandBuilder()
    .setName('ban')
    .setDescription('Bannit un membre du serveur')
    .setDefaultMemberPermissions(PermissionFlagsBits.BanMembers)
    .addUserOption((o) => o.setName('membre').setDescription('Membre a bannir').setRequired(true))
    .addStringOption((o) => o.setName('raison').setDescription('Raison'))
    .addIntegerOption((o) =>
      o.setName('jours_messages').setDescription('Supprimer les messages des N derniers jours (0-7)').setMinValue(0).setMaxValue(7)),

  new SlashCommandBuilder()
    .setName('unban')
    .setDescription('Debannit un utilisateur par ID')
    .setDefaultMemberPermissions(PermissionFlagsBits.BanMembers)
    .addStringOption((o) => o.setName('user_id').setDescription("ID de l'utilisateur").setRequired(true))
    .addStringOption((o) => o.setName('raison').setDescription('Raison')),

  new SlashCommandBuilder()
    .setName('timeout')
    .setDescription('Rend un membre muet pour une duree (ex: 10m, 2h, 1d)')
    .setDefaultMemberPermissions(PermissionFlagsBits.ModerateMembers)
    .addUserOption((o) => o.setName('membre').setDescription('Membre').setRequired(true))
    .addStringOption((o) => o.setName('duree').setDescription('Duree: 45s, 10m, 2h, 1d').setRequired(true))
    .addStringOption((o) => o.setName('raison').setDescription('Raison')),

  new SlashCommandBuilder()
    .setName('untimeout')
    .setDescription('Retire le timeout d\'un membre')
    .setDefaultMemberPermissions(PermissionFlagsBits.ModerateMembers)
    .addUserOption((o) => o.setName('membre').setDescription('Membre').setRequired(true)),

  new SlashCommandBuilder()
    .setName('clear')
    .setDescription('Supprime des messages du salon (1-100)')
    .setDefaultMemberPermissions(PermissionFlagsBits.ManageMessages)
    .addIntegerOption((o) => o.setName('nombre').setDescription('Nombre de messages').setRequired(true).setMinValue(1).setMaxValue(100)),

  new SlashCommandBuilder()
    .setName('slowmode')
    .setDescription('Definit le mode lent du salon (secondes, 0 = off)')
    .setDefaultMemberPermissions(PermissionFlagsBits.ManageChannels)
    .addIntegerOption((o) => o.setName('secondes').setDescription('0 a 21600').setRequired(true).setMinValue(0).setMaxValue(21600)),

  new SlashCommandBuilder()
    .setName('nick')
    .setDescription('Change le pseudo d\'un membre')
    .setDefaultMemberPermissions(PermissionFlagsBits.ManageNicknames)
    .addUserOption((o) => o.setName('membre').setDescription('Membre').setRequired(true))
    .addStringOption((o) => o.setName('pseudo').setDescription('Nouveau pseudo (vide = reset)')),

  new SlashCommandBuilder()
    .setName('say')
    .setDescription('Fait parler le bot dans le salon')
    .setDefaultMemberPermissions(PermissionFlagsBits.ManageMessages)
    .addStringOption((o) => o.setName('message').setDescription('Message').setRequired(true)),

  new SlashCommandBuilder()
    .setName('role')
    .setDescription('Gestion des roles')
    .setDefaultMemberPermissions(PermissionFlagsBits.ManageRoles)
    .addSubcommand((s) =>
      s.setName('add').setDescription('Ajoute un role a un membre')
        .addUserOption((o) => o.setName('membre').setDescription('Membre').setRequired(true))
        .addRoleOption((o) => o.setName('role').setDescription('Role').setRequired(true)))
    .addSubcommand((s) =>
      s.setName('remove').setDescription('Retire un role a un membre')
        .addUserOption((o) => o.setName('membre').setDescription('Membre').setRequired(true))
        .addRoleOption((o) => o.setName('role').setDescription('Role').setRequired(true)))
    .addSubcommand((s) =>
      s.setName('create').setDescription('Cree un role')
        .addStringOption((o) => o.setName('nom').setDescription('Nom du role').setRequired(true))
        .addStringOption((o) => o.setName('couleur').setDescription('Hex ex: #9d4bff')))
    .addSubcommand((s) =>
      s.setName('delete').setDescription('Supprime un role')
        .addRoleOption((o) => o.setName('role').setDescription('Role').setRequired(true))),

  new SlashCommandBuilder()
    .setName('userinfo')
    .setDescription('Infos sur un membre')
    .addUserOption((o) => o.setName('membre').setDescription('Membre (toi par defaut)')),

  new SlashCommandBuilder()
    .setName('serverinfo')
    .setDescription('Infos sur le serveur')
];

const commandNames = new Set(builders.map((b) => b.name));

// ---------------------------------------------------------------------------
// Handlers
// ---------------------------------------------------------------------------

async function handleKick(interaction) {
  const member = interaction.options.getMember('membre');
  const reason = interaction.options.getString('raison') || 'Aucune raison';
  if (!member) return replyErr(interaction, 'Membre introuvable sur ce serveur.');
  if (!member.kickable) return replyErr(interaction, "Je ne peux pas expulser ce membre (role trop haut ou permission manquante).");
  await member.kick(reason);
  return replyOk(interaction, '👢 Membre expulse', `${member.user.tag} a ete expulse.\n**Raison:** ${reason}`);
}

async function handleBan(interaction) {
  const user = interaction.options.getUser('membre');
  const member = interaction.options.getMember('membre');
  const reason = interaction.options.getString('raison') || 'Aucune raison';
  const days = interaction.options.getInteger('jours_messages') || 0;
  if (member && !member.bannable) return replyErr(interaction, "Je ne peux pas bannir ce membre (role trop haut ou permission manquante).");
  await interaction.guild.members.ban(user.id, { reason, deleteMessageSeconds: days * 86400 });
  return replyOk(interaction, '🔨 Membre banni', `${user.tag} a ete banni.\n**Raison:** ${reason}`);
}

async function handleUnban(interaction) {
  const userId = interaction.options.getString('user_id').trim();
  const reason = interaction.options.getString('raison') || 'Aucune raison';
  try {
    await interaction.guild.bans.remove(userId, reason);
  } catch (err) {
    return replyErr(interaction, `Impossible de debannir (${err.message}). Verifie l'ID.`);
  }
  return replyOk(interaction, '♻️ Debanni', `L'utilisateur \`${userId}\` a ete debanni.`);
}

async function handleTimeout(interaction) {
  const member = interaction.options.getMember('membre');
  const durationStr = interaction.options.getString('duree');
  const reason = interaction.options.getString('raison') || 'Aucune raison';
  if (!member) return replyErr(interaction, 'Membre introuvable.');
  if (!member.moderatable) return replyErr(interaction, "Je ne peux pas isoler ce membre (role trop haut ou permission manquante).");
  const ms = parseDuration(durationStr);
  if (!ms) return replyErr(interaction, 'Duree invalide. Formats: 45s, 10m, 2h, 1d.');
  const capped = Math.min(ms, 28 * 86400000);
  await member.timeout(capped, reason);
  return replyOk(interaction, '⏳ Timeout applique', `${member.user.tag} est isole pour **${durationStr}**.\n**Raison:** ${reason}`);
}

async function handleUntimeout(interaction) {
  const member = interaction.options.getMember('membre');
  if (!member) return replyErr(interaction, 'Membre introuvable.');
  await member.timeout(null);
  return replyOk(interaction, '✅ Timeout retire', `${member.user.tag} n'est plus isole.`);
}

async function handleClear(interaction) {
  const count = interaction.options.getInteger('nombre');
  if (!interaction.channel || typeof interaction.channel.bulkDelete !== 'function') {
    return replyErr(interaction, 'Ce salon ne supporte pas la suppression groupee.');
  }
  const deleted = await interaction.channel.bulkDelete(count, true);
  return interaction.reply({
    embeds: [embed(OK, '🧹 Messages supprimes', `${deleted.size} message(s) supprime(s).`)],
    flags: MessageFlags.Ephemeral
  });
}

async function handleSlowmode(interaction) {
  const seconds = interaction.options.getInteger('secondes');
  await interaction.channel.setRateLimitPerUser(seconds);
  return replyOk(interaction, '🐌 Mode lent', seconds === 0 ? 'Mode lent desactive.' : `Mode lent regle a ${seconds}s.`);
}

async function handleNick(interaction) {
  const member = interaction.options.getMember('membre');
  const nick = interaction.options.getString('pseudo') || null;
  if (!member) return replyErr(interaction, 'Membre introuvable.');
  if (!member.manageable) return replyErr(interaction, "Je ne peux pas changer le pseudo de ce membre (role trop haut).");
  await member.setNickname(nick);
  return replyOk(interaction, '✏️ Pseudo modifie', `${member.user.tag} -> **${nick || '(reset)'}**`);
}

async function handleSay(interaction) {
  const message = interaction.options.getString('message');
  await interaction.channel.send({ content: message, allowedMentions: { parse: [] } });
  return interaction.reply({ embeds: [embed(OK, '📨 Envoye', null)], flags: MessageFlags.Ephemeral });
}

async function handleRole(interaction) {
  const sub = interaction.options.getSubcommand();
  const me = interaction.guild.members.me;

  if (sub === 'add' || sub === 'remove') {
    const member = interaction.options.getMember('membre');
    const role = interaction.options.getRole('role');
    if (!member) return replyErr(interaction, 'Membre introuvable.');
    if (role.position >= me.roles.highest.position) {
      return replyErr(interaction, `Le role ${role} est au-dessus du mien, je ne peux pas le gerer.`);
    }
    if (sub === 'add') {
      await member.roles.add(role);
      return replyOk(interaction, '✅ Role ajoute', `${role} ajoute a ${member.user.tag}.`);
    }
    await member.roles.remove(role);
    return replyOk(interaction, '✅ Role retire', `${role} retire de ${member.user.tag}.`);
  }

  if (sub === 'create') {
    const name = interaction.options.getString('nom');
    const colorStr = interaction.options.getString('couleur');
    let color;
    if (colorStr) {
      const parsed = parseInt(colorStr.replace('#', ''), 16);
      if (!Number.isNaN(parsed)) color = parsed;
    }
    const role = await interaction.guild.roles.create({ name, color, reason: `Cree par ${interaction.user.tag}` });
    return replyOk(interaction, '✅ Role cree', `${role} cree.`);
  }

  if (sub === 'delete') {
    const role = interaction.options.getRole('role');
    if (role.position >= me.roles.highest.position) {
      return replyErr(interaction, `Le role ${role} est au-dessus du mien, je ne peux pas le supprimer.`);
    }
    const name = role.name;
    await role.delete(`Supprime par ${interaction.user.tag}`);
    return replyOk(interaction, '🗑️ Role supprime', `Le role **${name}** a ete supprime.`);
  }
  return replyErr(interaction, 'Sous-commande inconnue.');
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
  [PermissionFlagsBits.Administrator, 'Administrateur'],
  [PermissionFlagsBits.ManageGuild, 'Gerer le serveur'],
  [PermissionFlagsBits.BanMembers, 'Bannir'],
  [PermissionFlagsBits.KickMembers, 'Expulser'],
  [PermissionFlagsBits.ModerateMembers, 'Timeout'],
  [PermissionFlagsBits.ManageMessages, 'Gerer messages'],
  [PermissionFlagsBits.ManageRoles, 'Gerer roles'],
  [PermissionFlagsBits.ManageChannels, 'Gerer salons']
];

async function handleUserinfo(interaction) {
  const member = interaction.options.getMember('membre') || interaction.member;
  const user = member.user;

  const roles = member.roles.cache
    .filter((r) => r.id !== interaction.guild.id)
    .sort((a, b) => b.position - a.position)
    .map((r) => r.toString())
    .slice(0, 20)
    .join(' ') || 'Aucun';

  // Leveling stats for this member on this guild.
  const levelDoc = await getUserLevel(interaction.guildId, user.id);
  let levelValue = 'Aucune activite';
  if (levelDoc) {
    const p = progress(levelDoc.xp);
    const rank = await getUserRank(interaction.guildId, user.id);
    const total = await countRanked(interaction.guildId);
    levelValue = `**Niveau ${p.level}** · Rang #${rank}/${total}\n${levelDoc.xp.toLocaleString('fr-FR')} XP · ${levelDoc.messageCount.toLocaleString('fr-FR')} messages`;
  }

  // Notable permissions held on the server.
  const perms = member.permissions;
  const grantedPerms = KEY_PERMS.filter(([flag]) => perms.has(flag)).map(([, label]) => label);
  const permsValue = perms.has(PermissionFlagsBits.Administrator)
    ? 'Administrateur (toutes)'
    : (grantedPerms.length ? grantedPerms.join(', ') : 'Aucune notable');

  // Highest role as an at-a-glance identity marker.
  const topRole = member.roles.highest && member.roles.highest.id !== interaction.guild.id
    ? member.roles.highest.toString()
    : 'Aucun';

  const e = embed(INFO, `👤 ${user.tag}`)
    .setThumbnail(user.displayAvatarURL({ size: 256 }))
    .addFields(
      { name: 'Utilisateur', value: `${user} \`${user.id}\`` },
      { name: 'Compte cree le', value: tsBoth(user.createdTimestamp), inline: false },
      { name: 'A rejoint le', value: member.joinedTimestamp ? tsBoth(member.joinedTimestamp) : '—', inline: false }
    );

  // Boosting status, only when relevant.
  if (member.premiumSinceTimestamp) {
    e.addFields({ name: 'Booste depuis', value: tsFull(member.premiumSinceTimestamp), inline: true });
  }
  // Active timeout, only when relevant.
  if (member.communicationDisabledUntilTimestamp && member.communicationDisabledUntilTimestamp > Date.now()) {
    e.addFields({ name: '⏳ Timeout jusqu\'a', value: tsFull(member.communicationDisabledUntilTimestamp), inline: true });
  }

  e.addFields(
    { name: '📊 Progression', value: levelValue, inline: false },
    { name: 'Role principal', value: topRole, inline: true },
    { name: 'Permissions cles', value: permsValue, inline: false },
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
      { name: 'Proprietaire', value: owner ? owner.user.tag : '—', inline: true },
      { name: 'Cree', value: tsFull(g.createdTimestamp), inline: true },
      { name: 'Membres', value: String(g.memberCount), inline: true },
      { name: 'Salons', value: String(g.channels.cache.size), inline: true },
      { name: 'Roles', value: String(g.roles.cache.size), inline: true },
      { name: 'Boosts', value: `${g.premiumSubscriptionCount || 0} (niveau ${g.premiumTier})`, inline: true }
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
    await replyErr(interaction, 'Cette commande doit etre utilisee dans un serveur.');
    return true;
  }
  // inGuild() ne garantit que guildId, pas l'objet guild. Avec le seul intent
  // Guilds (ou juste apres un redemarrage) la guild peut ne pas etre en cache,
  // et interaction.guild vaut null -> tous les handlers crashent. On la resout.
  if (!interaction.guild) {
    try {
      await interaction.client.guilds.fetch(interaction.guildId);
    } catch (err) {
      await replyErr(interaction, `Serveur inaccessible (${err.message}). Reessaie dans un instant.`);
      return true;
    }
  }
  // Certains handlers utilisent interaction.guild.members.me (gestion des roles).
  // members.me peut etre null si la guild vient d'etre fetch a froid.
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
