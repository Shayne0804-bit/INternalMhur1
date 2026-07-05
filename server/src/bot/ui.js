const {
  EmbedBuilder,
  ActionRowBuilder,
  ButtonBuilder,
  ButtonStyle,
  ModalBuilder,
  TextInputBuilder,
  TextInputStyle
} = require('discord.js');

const ACCENT = 0x9d4bff;

function buildPanel() {
  const embed = new EmbedBuilder()
    .setColor(ACCENT)
    .setTitle('🔑 Gestion des licences')
    .setDescription([
      'Génère une clé de licence premium.',
      '',
      '• **Durées rapides** — un clic crée une clé premium (1 activation).',
      '• **Personnalisé** — choisis le tier, la durée exacte, les activations et une note.'
    ].join('\n'))
    .setFooter({ text: "La clé générée ne s'affiche qu'à toi (message éphémère)." });

  const presets = new ActionRowBuilder().addComponents(
    new ButtonBuilder().setCustomId('lic:preset:7').setLabel('7 jours').setEmoji('📅').setStyle(ButtonStyle.Secondary),
    new ButtonBuilder().setCustomId('lic:preset:30').setLabel('30 jours').setEmoji('📅').setStyle(ButtonStyle.Secondary),
    new ButtonBuilder().setCustomId('lic:preset:90').setLabel('90 jours').setEmoji('📅').setStyle(ButtonStyle.Secondary),
    new ButtonBuilder().setCustomId('lic:preset:lifetime').setLabel('À vie').setEmoji('♾️').setStyle(ButtonStyle.Success)
  );

  const custom = new ActionRowBuilder().addComponents(
    new ButtonBuilder().setCustomId('lic:custom').setLabel('Personnalisé').setEmoji('⚙️').setStyle(ButtonStyle.Primary)
  );

  return { embeds: [embed], components: [presets, custom] };
}

function buildCustomModal() {
  const modal = new ModalBuilder().setCustomId('lic:modal').setTitle('Créer une licence');

  const inputs = [
    new TextInputBuilder()
      .setCustomId('days')
      .setLabel('Durée en jours (vide = à vie)')
      .setStyle(TextInputStyle.Short)
      .setRequired(false)
      .setPlaceholder('30'),
    new TextInputBuilder()
      .setCustomId('tier')
      .setLabel('Tier : premium / trial / lifetime')
      .setStyle(TextInputStyle.Short)
      .setRequired(false)
      .setValue('premium'),
    new TextInputBuilder()
      .setCustomId('acts')
      .setLabel('Activations max (1-10)')
      .setStyle(TextInputStyle.Short)
      .setRequired(false)
      .setValue('1'),
    new TextInputBuilder()
      .setCustomId('notes')
      .setLabel('Note (optionnel)')
      .setStyle(TextInputStyle.Short)
      .setRequired(false)
  ];

  modal.addComponents(...inputs.map((input) => new ActionRowBuilder().addComponents(input)));
  return modal;
}

function buildKeyEmbed({ key, license }) {
  const expiration = license.expiresAt
    ? `<t:${Math.floor(new Date(license.expiresAt).getTime() / 1000)}:F>`
    : 'À vie';

  return new EmbedBuilder()
    .setColor(0x25ff85)
    .setTitle('✅ Licence créée')
    .addFields(
      { name: 'Clé', value: '```' + key + '```' },
      { name: 'Tier', value: String(license.tier), inline: true },
      { name: 'Activations', value: String(license.maxActivations), inline: true },
      { name: 'Expiration', value: expiration, inline: true }
    )
    .setFooter({ text: `ID: ${license._id}` });
}

function buildKeyCheckModal() {
  const modal = new ModalBuilder().setCustomId('chk:modal').setTitle('Verifier ma licence');
  const key = new TextInputBuilder()
    .setCustomId('key')
    .setLabel('Ta cle de licence')
    .setStyle(TextInputStyle.Short)
    .setRequired(true)
    .setPlaceholder('RUGIR-XXXXX-XXXXX-XXXXX-XXXXX-XXXXX');
  modal.addComponents(new ActionRowBuilder().addComponents(key));
  return modal;
}

function licenseStatusLabel(license) {
  if (license.status === 'revoked') return '⛔ Revoquee';
  if (license.status === 'expired' || (license.expiresAt && new Date(license.expiresAt) <= new Date())) {
    return '⏳ Expiree';
  }
  return '✅ Active';
}

// Member-facing view of their own license, with a "reset HWID" button.
function buildMemberLicenseView(license) {
  let expiration = 'À vie';
  if (license.expiresAt) {
    const unix = Math.floor(new Date(license.expiresAt).getTime() / 1000);
    expiration = `<t:${unix}:F> (<t:${unix}:R>)`;
  }

  const embed = new EmbedBuilder()
    .setColor(ACCENT)
    .setTitle('📄 Ta licence')
    .addFields(
      { name: 'Statut', value: licenseStatusLabel(license), inline: true },
      { name: 'Tier', value: String(license.tier), inline: true },
      { name: 'Activations', value: `${license.hwidHashes.length}/${license.maxActivations}`, inline: true },
      { name: 'Expiration', value: expiration }
    )
    .setFooter({ text: 'Changement de PC ? Demande un reset HWID ci-dessous.' });

  const row = new ActionRowBuilder().addComponents(
    new ButtonBuilder()
      .setCustomId(`chk:reset:${license._id}`)
      .setLabel('Reinitialiser HWID')
      .setEmoji('🔄')
      .setStyle(ButtonStyle.Danger),
    new ButtonBuilder()
      .setCustomId('chk:new')
      .setLabel('Nouvelle licence')
      .setEmoji('➕')
      .setStyle(ButtonStyle.Secondary)
  );

  return { embeds: [embed], components: [row] };
}

// Owner-facing reset request (pings the owner, approve / deny buttons).
function buildResetRequest(license, member, ownerId) {
  const embed = new EmbedBuilder()
    .setColor(0xffb800)
    .setTitle('🔄 Demande de reset HWID')
    .setDescription(`${member} demande la reinitialisation du HWID de sa licence.`)
    .addFields(
      { name: 'Utilisateur', value: `${member} (\`${member.id}\`)` },
      { name: 'Licence', value: `\`${license._id}\``, inline: true },
      { name: 'Tier', value: String(license.tier), inline: true },
      { name: 'Activations liees', value: `${license.hwidHashes.length}/${license.maxActivations}`, inline: true }
    );

  const row = new ActionRowBuilder().addComponents(
    new ButtonBuilder()
      .setCustomId(`reset:ok:${license._id}:${member.id}`)
      .setLabel('Approuver')
      .setEmoji('✅')
      .setStyle(ButtonStyle.Success),
    new ButtonBuilder()
      .setCustomId(`reset:no:${license._id}:${member.id}`)
      .setLabel('Refuser')
      .setEmoji('⛔')
      .setStyle(ButtonStyle.Danger)
  );

  return { content: ownerId ? `<@${ownerId}>` : undefined, embeds: [embed], components: [row] };
}

module.exports = {
  buildPanel,
  buildCustomModal,
  buildKeyEmbed,
  buildKeyCheckModal,
  buildMemberLicenseView,
  buildResetRequest
};
