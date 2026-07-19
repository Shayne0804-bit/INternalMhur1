const {
  Client,
  GatewayIntentBits,
  Events,
  REST,
  Routes,
  SlashCommandBuilder,
  MessageFlags,
  ActivityType,
  Partials
} = require('discord.js');

const {
  createLicense,
  findLicenseByKey,
  findLicenseByDiscordUser,
  getLicenseById,
  bindDiscordUser,
  resetLicenseHwid
} = require('../services/licenseService');
const {
  buildPanel,
  buildCustomModal,
  buildKeyEmbed,
  buildKeyCheckModal,
  buildMemberLicenseView,
  buildResetRequest
} = require('./ui');
const {
  moderationCommands,
  moderationCommandNames,
  handleModerationCommand
} = require('./moderation');
const {
  levelingCommands,
  levelingCommandNames,
  handleLevelingCommand,
  attachLeveling
} = require('./leveling');
const { attachWelcome } = require('./welcome');
const {
  configCommands,
  configCommandNames,
  handleConfigCommand
} = require('./config');
const {
  licenseCommands,
  licenseCommandNames,
  handleLicenseCommand
} = require('./license');
const { grantCustomerRole } = require('./customerRole');
const { startExpiryScheduler } = require('./expiryScheduler');
const {
  ticketCommands,
  ticketCommandNames,
  handleTicketCommand,
  handleTicketButton
} = require('./tickets');
const {
  rolePanelCommands,
  rolePanelCommandNames,
  handleRolePanelCommand,
  handleApprovalButton,
  attachReactionRoles
} = require('./reactionRoles');
const {
  verificationCommands,
  verificationCommandNames,
  handleSetupVerification,
  handleVerifyButton
} = require('./verification');
const {
  warnCommands,
  warnCommandNames,
  handleWarnCommand
} = require('./warns');
const { attachAutomod } = require('./automod');

const commands = [
  new SlashCommandBuilder()
    .setName('licences')
    .setDescription('License creation panel (owner only)')
    .toJSON(),
  new SlashCommandBuilder()
    .setName('check')
    .setDescription('Check your license and its expiration date')
    .toJSON(),
  ...moderationCommands,
  ...levelingCommands,
  ...configCommands,
  ...licenseCommands,
  ...ticketCommands,
  ...rolePanelCommands,
  ...verificationCommands,
  ...warnCommands
];

const ownerIds = new Set();
let primaryOwnerId = null;

// Bot owners = Discord application owner / team members + optional OWNER_IDS env.
async function loadOwners(client) {
  try {
    const app = await client.application.fetch();
    const owner = app.owner;
    if (owner && owner.members) {
      primaryOwnerId = owner.ownerId || null;
      for (const member of owner.members.values()) {
        const id = member.user ? member.user.id : member.id;
        if (id) ownerIds.add(id);
      }
    } else if (owner) {
      primaryOwnerId = owner.id;
      ownerIds.add(owner.id);
    }
  } catch (err) {
    console.error('[bot] impossible de charger le proprietaire:', err.message);
  }

  for (const id of (process.env.OWNER_IDS || '').split(',').map((s) => s.trim()).filter(Boolean)) {
    ownerIds.add(id);
  }
  if (!primaryOwnerId && ownerIds.size) {
    primaryOwnerId = [...ownerIds][0];
  }
  console.log(`[bot] owners=${[...ownerIds].join(',') || '(aucun)'}`);
}

function isOwner(userId) {
  return ownerIds.has(userId);
}

async function denyIfNotOwner(interaction) {
  if (isOwner(interaction.user.id)) {
    return false;
  }
  await interaction.reply({
    content: '⛔ Reserved for the bot owner.',
    flags: MessageFlags.Ephemeral
  });
  return true;
}

async function registerCommands(token, clientId, guildId) {
  const rest = new REST({ version: '10' }).setToken(token);
  if (guildId) {
    await rest.put(Routes.applicationGuildCommands(clientId, guildId), { body: commands });
  } else {
    await rest.put(Routes.applicationCommands(clientId), { body: commands });
  }
}

async function createAndReply(interaction, options) {
  await interaction.deferReply({ flags: MessageFlags.Ephemeral });
  try {
    const result = await createLicense(options);
    await interaction.editReply({ embeds: [buildKeyEmbed(result)] });
  } catch (err) {
    await interaction.editReply({ content: `❌ Failed: ${err.message}` });
  }
}

async function handleLicenceCheck(interaction) {
  await interaction.deferReply({ flags: MessageFlags.Ephemeral });
  const key = interaction.fields.getTextInputValue('key');
  const license = await findLicenseByKey(key);
  if (!license) {
    await interaction.editReply({ content: "❌ Key not found. Make sure you copied it correctly." });
    return;
  }
  // Register the member under their Discord identity on the license.
  await bindDiscordUser(
    license,
    interaction.user.id,
    interaction.user.tag || interaction.user.username,
    key.trim()
  );
  // Grant the customer role when the license is currently valid.
  const valid = license.status === 'active' && !license.isExpired();
  if (valid && interaction.guild) {
    await grantCustomerRole(interaction.guild, interaction.user.id);
  }
  await interaction.editReply(buildMemberLicenseView(license));
}

async function resolveRequestChannel(interaction) {
  const raw = (process.env.RESET_CHANNEL_ID || '').trim();
  if (raw) {
    try {
      const channel = await interaction.client.channels.fetch(raw);
      if (channel && channel.isTextBased()) {
        return channel;
      }
      console.error(`[bot] RESET_CHANNEL_ID=${raw}: pas un salon textuel (type=${channel ? channel.type : 'null'}).`);
    } catch (err) {
      console.error(`[bot] RESET_CHANNEL_ID=${raw} inaccessible: ${err.message}`);
    }
  }

  // Fallback: the channel where the member clicked the button.
  if (interaction.channel && interaction.channel.isTextBased()) {
    return interaction.channel;
  }
  if (interaction.channelId) {
    try {
      const channel = await interaction.client.channels.fetch(interaction.channelId);
      if (channel && channel.isTextBased()) {
        return channel;
      }
    } catch (err) {
      console.error(`[bot] salon courant inaccessible: ${err.message}`);
    }
  }
  return null;
}

async function handleResetRequest(interaction, licenseId) {
  await interaction.deferReply({ flags: MessageFlags.Ephemeral });
  const license = await getLicenseById(licenseId);
  if (!license) {
    await interaction.editReply({ content: '❌ License not found.' });
    return;
  }

  const request = buildResetRequest(license, interaction.user, primaryOwnerId);
  const channel = await resolveRequestChannel(interaction);

  if (channel) {
    try {
      await channel.send({
        ...request,
        allowedMentions: { users: primaryOwnerId ? [primaryOwnerId] : [] }
      });
      await interaction.editReply({ content: '✅ Reset request sent to the owner. You will be notified.' });
      return;
    } catch (err) {
      console.error(`[bot] envoi demande reset echoue: ${err.message}`);
    }
  }

  // Last resort: DM the owner directly.
  if (primaryOwnerId) {
    try {
      const owner = await interaction.client.users.fetch(primaryOwnerId);
      await owner.send(request);
      await interaction.editReply({ content: '✅ Request sent to the owner via direct message.' });
      return;
    } catch (err) {
      console.error(`[bot] DM owner echoue: ${err.message}`);
    }
  }

  await interaction.editReply({
    content: "❌ Could not send the request. Make sure RESET_CHANNEL_ID is a **text channel** and that the bot has the **View Channel** + **Send Messages** permissions there."
  });
}

async function handleResetDecision(interaction, approve, licenseId, memberId) {
  if (await denyIfNotOwner(interaction)) {
    return;
  }
  await interaction.deferUpdate();

  let content;
  if (approve) {
    await resetLicenseHwid(licenseId);
    content = `✅ HWID reset **approved** by <@${interaction.user.id}> — <@${memberId}> can now reactivate on a new PC.`;
  } else {
    content = `⛔ HWID reset **denied** by <@${interaction.user.id}> for <@${memberId}>.`;
  }

  await interaction.message.edit({
    content,
    embeds: interaction.message.embeds,
    components: [],
    allowedMentions: { users: [memberId] }
  });
}

function attachHandlers(client) {
  client.on(Events.InteractionCreate, async (interaction) => {
    try {
      if (interaction.isChatInputCommand()) {
        if (interaction.commandName === 'licences') {
          if (await denyIfNotOwner(interaction)) return;
          await interaction.reply({ ...buildPanel(), flags: MessageFlags.Ephemeral });
        } else if (interaction.commandName === 'check') {
          // If the member already verified a license, show it directly.
          const existing = await findLicenseByDiscordUser(interaction.user.id);
          if (existing) {
            await interaction.reply({ ...buildMemberLicenseView(existing), flags: MessageFlags.Ephemeral });
          } else {
            await interaction.showModal(buildKeyCheckModal());
          }
        } else if (moderationCommandNames.has(interaction.commandName)) {
          await handleModerationCommand(interaction);
        } else if (levelingCommandNames.has(interaction.commandName)) {
          await handleLevelingCommand(interaction);
        } else if (configCommandNames.has(interaction.commandName)) {
          if (await denyIfNotOwner(interaction)) return;
          await handleConfigCommand(interaction);
        } else if (licenseCommandNames.has(interaction.commandName)) {
          if (await denyIfNotOwner(interaction)) return;
          await handleLicenseCommand(interaction);
        } else if (ticketCommandNames.has(interaction.commandName)) {
          if (interaction.commandName === 'ticketpanel' && await denyIfNotOwner(interaction)) return;
          await handleTicketCommand(interaction);
        } else if (rolePanelCommandNames.has(interaction.commandName)) {
          if (await denyIfNotOwner(interaction)) return;
          await handleRolePanelCommand(interaction);
        } else if (verificationCommandNames.has(interaction.commandName)) {
          if (await denyIfNotOwner(interaction)) return;
          await handleSetupVerification(interaction);
        } else if (warnCommandNames.has(interaction.commandName)) {
          await handleWarnCommand(interaction);
        }
        return;
      }

      if (interaction.isButton()) {
        const parts = interaction.customId.split(':');
        const ns = parts[0];

        if (ns === 'lic') {
          if (await denyIfNotOwner(interaction)) return;
          if (parts[1] === 'custom') {
            await interaction.showModal(buildCustomModal());
          } else if (parts[1] === 'preset') {
            const options = parts[2] === 'lifetime'
              ? { tier: 'lifetime' }
              : { tier: 'premium', daysValid: Number(parts[2]) };
            await createAndReply(interaction, options);
          }
        } else if (ns === 'chk' && parts[1] === 'new') {
          await interaction.showModal(buildKeyCheckModal());
        } else if (ns === 'chk' && parts[1] === 'reset') {
          await handleResetRequest(interaction, parts[2]);
        } else if (ns === 'reset') {
          await handleResetDecision(interaction, parts[1] === 'ok', parts[2], parts[3]);
        } else if (ns === 'ticket') {
          await handleTicketButton(interaction, parts[1]);
        } else if (ns === 'verify') {
          await handleVerifyButton(interaction);
        } else if (ns === 'rrapp') {
          if (await denyIfNotOwner(interaction)) return;
          await handleApprovalButton(interaction, parts[1] === 'ok', parts[2], parts[3]);
        }
        return;
      }

      if (interaction.isModalSubmit()) {
        if (interaction.customId === 'lic:modal') {
          if (await denyIfNotOwner(interaction)) return;
          const days = interaction.fields.getTextInputValue('days').trim();
          const tier = (interaction.fields.getTextInputValue('tier').trim() || 'premium').toLowerCase();
          const acts = interaction.fields.getTextInputValue('acts').trim();
          const notes = interaction.fields.getTextInputValue('notes').trim();
          await createAndReply(interaction, { tier, daysValid: days || undefined, maxActivations: acts || 1, notes });
        } else if (interaction.customId === 'chk:modal') {
          await handleLicenceCheck(interaction);
        }
      }
    } catch (err) {
      console.error('[bot] interaction error:', err.message);
      if (interaction.isRepliable() && !interaction.replied && !interaction.deferred) {
        await interaction
          .reply({ content: `❌ Error: ${err.message}`, flags: MessageFlags.Ephemeral })
          .catch(() => {});
      }
    }
  });
}

// Starts the Discord bot in the same process as the auth server.
// No-op (returns null) when Discord credentials are not configured.
async function startBot() {
  const token = process.env.DISCORD_TOKEN;
  const clientId = process.env.DISCORD_CLIENT_ID;
  const guildId = process.env.DISCORD_GUILD_ID || '';

  if (!token || !clientId) {
    console.log('[bot] DISCORD_TOKEN/DISCORD_CLIENT_ID absent - bot desactive.');
    return null;
  }

  const client = new Client({
    intents: [
      GatewayIntentBits.Guilds,
      GatewayIntentBits.GuildMessages,
      GatewayIntentBits.GuildMembers,
      GatewayIntentBits.GuildVoiceStates,
      GatewayIntentBits.GuildMessageReactions,
      // Optional privileged intent: enable ONLY if MessageContent is turned on
      // in the Developer Portal, otherwise the login is rejected. Without it,
      // automod still does rate-based anti-spam (content checks are skipped).
      ...(process.env.ENABLE_MESSAGE_CONTENT === '1' ? [GatewayIntentBits.MessageContent] : [])
    ],
    // Needed so reactions on messages posted before the current session still
    // emit add/remove events (uncached messages arrive as partials).
    partials: [Partials.Message, Partials.Reaction, Partials.User]
  });
  attachHandlers(client);
  attachLeveling(client);
  attachWelcome(client);
  attachAutomod(client);
  attachReactionRoles(client, { getOwnerId: () => primaryOwnerId });

  client.once(Events.ClientReady, async (c) => {
    // Force an explicit online presence so the bot never shows as offline/invisible.
    c.user.setPresence({
      status: 'online',
      activities: [{ name: '/check', type: ActivityType.Listening }]
    });
    console.log(`[bot] connecte et en ligne: ${c.user.tag}`);
    await loadOwners(c);
    startExpiryScheduler(c);
  });

  client.on(Events.Error, (err) => console.error('[bot] gateway error:', err.message));
  client.on(Events.Warn, (msg) => console.warn('[bot] warn:', msg));
  client.on(Events.ShardDisconnect, (event) => console.error(`[bot] shard deconnecte (code ${event.code}) - token/intents ?`));

  await registerCommands(token, clientId, guildId);
  try {
    await client.login(token);
  } catch (err) {
    console.error('[bot] login echoue (token invalide ?):', err.message);
    throw err;
  }
  return client;
}

module.exports = { startBot };
