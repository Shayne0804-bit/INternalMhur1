const {
  Client,
  GatewayIntentBits,
  Events,
  REST,
  Routes,
  SlashCommandBuilder,
  MessageFlags
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

const commands = [
  new SlashCommandBuilder()
    .setName('licences')
    .setDescription('Panneau de creation de licences (proprietaire)')
    .toJSON(),
  new SlashCommandBuilder()
    .setName('macle')
    .setDescription("Verifie ta licence et sa date d'expiration")
    .toJSON()
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
    content: '⛔ Reserve au proprietaire du bot.',
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
    await interaction.editReply({ content: `❌ Echec: ${err.message}` });
  }
}

async function handleLicenceCheck(interaction) {
  await interaction.deferReply({ flags: MessageFlags.Ephemeral });
  const key = interaction.fields.getTextInputValue('key');
  const license = await findLicenseByKey(key);
  if (!license) {
    await interaction.editReply({ content: "❌ Cle introuvable. Verifie que tu l'as copiee correctement." });
    return;
  }
  // Register the member under their Discord identity on the license.
  await bindDiscordUser(license, interaction.user.id, interaction.user.tag || interaction.user.username);
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
    await interaction.editReply({ content: '❌ Licence introuvable.' });
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
      await interaction.editReply({ content: '✅ Demande de reset envoyee au proprietaire. Tu seras notifie.' });
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
      await interaction.editReply({ content: '✅ Demande envoyee au proprietaire en message prive.' });
      return;
    } catch (err) {
      console.error(`[bot] DM owner echoue: ${err.message}`);
    }
  }

  await interaction.editReply({
    content: "❌ Impossible d'envoyer la demande. Verifie que RESET_CHANNEL_ID est bien un **salon textuel** et que le bot y a les permissions **Voir le salon** + **Envoyer des messages**."
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
    content = `✅ Reset HWID **approuve** par <@${interaction.user.id}> — <@${memberId}> peut reactiver sur un nouveau PC.`;
  } else {
    content = `⛔ Reset HWID **refuse** par <@${interaction.user.id}> pour <@${memberId}>.`;
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
        } else if (interaction.commandName === 'macle') {
          // If the member already verified a license, show it directly.
          const existing = await findLicenseByDiscordUser(interaction.user.id);
          if (existing) {
            await interaction.reply({ ...buildMemberLicenseView(existing), flags: MessageFlags.Ephemeral });
          } else {
            await interaction.showModal(buildKeyCheckModal());
          }
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
          .reply({ content: `❌ Erreur: ${err.message}`, flags: MessageFlags.Ephemeral })
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

  const client = new Client({ intents: [GatewayIntentBits.Guilds] });
  attachHandlers(client);
  client.once(Events.ClientReady, async (c) => {
    console.log(`[bot] connecte: ${c.user.tag}`);
    await loadOwners(c);
  });

  await registerCommands(token, clientId, guildId);
  await client.login(token);
  return client;
}

module.exports = { startBot };
