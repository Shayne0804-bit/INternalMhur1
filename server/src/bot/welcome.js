const {
  EmbedBuilder,
  ChannelType,
  PermissionFlagsBits
} = require('discord.js');

const { getConfig } = require('../services/guildConfigService');

const ACCENT = 0x9d4bff;
const WELCOME_CHANNEL_NAME = 'welcome';

// Creates a read-only welcome channel: @everyone can view but not send,
// admins send via their Administrator permission (bypasses the deny).
async function createWelcomeChannel(guild) {
  const me = guild.members.me;
  if (!me || !me.permissions.has(PermissionFlagsBits.ManageChannels)) {
    console.error(`[bot] welcome: pas la permission ManageChannels sur ${guild.name}.`);
    return null;
  }

  try {
    return await guild.channels.create({
      name: WELCOME_CHANNEL_NAME,
      type: ChannelType.GuildText,
      topic: 'Welcome messages — new members are greeted here.',
      reason: 'Auto-created welcome channel (locked, read-only for members)',
      permissionOverwrites: [
        {
          id: guild.roles.everyone.id,
          allow: [PermissionFlagsBits.ViewChannel, PermissionFlagsBits.ReadMessageHistory],
          deny: [
            PermissionFlagsBits.SendMessages,
            PermissionFlagsBits.SendMessagesInThreads,
            PermissionFlagsBits.CreatePublicThreads,
            PermissionFlagsBits.CreatePrivateThreads,
            PermissionFlagsBits.AddReactions
          ]
        },
        {
          // Ensure the bot itself can always post here.
          id: me.id,
          allow: [PermissionFlagsBits.ViewChannel, PermissionFlagsBits.SendMessages]
        }
      ]
    });
  } catch (err) {
    console.error(`[bot] welcome: creation du salon echouee sur ${guild.name}: ${err.message}`);
    return null;
  }
}

// Resolve the welcome channel: persisted id -> existing "welcome" channel ->
// create a fresh locked one. Persists the id so it is created only once.
async function resolveWelcomeChannel(guild) {
  const config = await getConfig(guild.id);

  if (config.welcomeChannelId) {
    const cached = guild.channels.cache.get(config.welcomeChannelId)
      || await guild.channels.fetch(config.welcomeChannelId).catch(() => null);
    if (cached) return cached;
    // Stored channel was deleted — clear and fall through to recreate.
    config.welcomeChannelId = null;
  }

  // Reuse an existing channel literally named "welcome" if present.
  const existing = guild.channels.cache.find(
    (c) => c.type === ChannelType.GuildText && c.name === WELCOME_CHANNEL_NAME
  );
  const channel = existing || await createWelcomeChannel(guild);
  if (!channel) return null;

  config.welcomeChannelId = channel.id;
  await config.save();
  return channel;
}

function buildWelcomeEmbed(member) {
  const memberNumber = member.guild.memberCount;
  return new EmbedBuilder()
    .setColor(ACCENT)
    .setTitle('👋 Welcome!')
    .setDescription(`Hey ${member}, welcome to **${member.guild.name}**!\nYou are member **#${memberNumber}**. Glad to have you here.`)
    .setThumbnail(member.user.displayAvatarURL({ size: 256 }))
    .setFooter({ text: `${member.user.tag}` })
    .setTimestamp(member.joinedAt || undefined);
}

function attachWelcome(client) {
  client.on('guildMemberAdd', async (member) => {
    try {
      if (member.user.bot) return;

      // Auto-role: grant the configured member role on join.
      const config = await getConfig(member.guild.id);
      if (config.memberRoleId) {
        const role = member.guild.roles.cache.get(config.memberRoleId)
          || await member.guild.roles.fetch(config.memberRoleId).catch(() => null);
        const me = member.guild.members.me;
        if (role && me && role.position < me.roles.highest.position) {
          await member.roles.add(role, 'Auto-role on join').catch(() => {});
        }
      }

      const channel = await resolveWelcomeChannel(member.guild);
      if (!channel) return;
      await channel.send({
        embeds: [buildWelcomeEmbed(member)],
        allowedMentions: { users: [member.id] }
      });
    } catch (err) {
      console.error('[bot] welcome error:', err.message);
    }
  });
}

module.exports = { attachWelcome };
