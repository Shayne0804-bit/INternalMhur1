const {
  Client,
  GatewayIntentBits,
  Events,
  REST,
  Routes,
  SlashCommandBuilder,
  PermissionFlagsBits,
  MessageFlags
} = require('discord.js');

const { createLicense } = require('../services/licenseService');
const { buildPanel, buildCustomModal, buildKeyEmbed } = require('./ui');

const commands = [
  new SlashCommandBuilder()
    .setName('licences')
    .setDescription('Ouvre le panneau de creation de licences')
    .setDefaultMemberPermissions(PermissionFlagsBits.Administrator)
    .toJSON()
];

async function registerCommands(token, clientId, guildId) {
  const rest = new REST({ version: '10' }).setToken(token);
  if (guildId) {
    // Guild-scoped commands appear instantly (best for a single server).
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

function attachHandlers(client) {
  client.on(Events.InteractionCreate, async (interaction) => {
    try {
      if (interaction.isChatInputCommand() && interaction.commandName === 'licences') {
        await interaction.reply({ ...buildPanel(), flags: MessageFlags.Ephemeral });
        return;
      }

      if (interaction.isButton() && interaction.customId.startsWith('lic:')) {
        const [, action, value] = interaction.customId.split(':');
        if (action === 'custom') {
          await interaction.showModal(buildCustomModal());
        } else if (action === 'preset') {
          const options = value === 'lifetime'
            ? { tier: 'lifetime' }
            : { tier: 'premium', daysValid: Number(value) };
          await createAndReply(interaction, options);
        }
        return;
      }

      if (interaction.isModalSubmit() && interaction.customId === 'lic:modal') {
        const days = interaction.fields.getTextInputValue('days').trim();
        const tier = (interaction.fields.getTextInputValue('tier').trim() || 'premium').toLowerCase();
        const acts = interaction.fields.getTextInputValue('acts').trim();
        const notes = interaction.fields.getTextInputValue('notes').trim();

        await createAndReply(interaction, {
          tier,
          daysValid: days || undefined,
          maxActivations: acts || 1,
          notes
        });
      }
    } catch (err) {
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
  client.once(Events.ClientReady, (c) => console.log(`[bot] connecte: ${c.user.tag}`));

  await registerCommands(token, clientId, guildId);
  await client.login(token);
  return client;
}

module.exports = { startBot };
