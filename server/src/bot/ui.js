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
    .setTitle('🔑 License management')
    .setDescription([
      'Generate a premium license key.',
      '',
      '• **Quick durations** — one click creates a premium key (1 activation).',
      '• **Custom** — choose the tier, exact duration, activations and a note.'
    ].join('\n'))
    .setFooter({ text: 'The generated key is only shown to you (ephemeral message).' });

  const presets = new ActionRowBuilder().addComponents(
    new ButtonBuilder().setCustomId('lic:preset:7').setLabel('7 days').setEmoji('📅').setStyle(ButtonStyle.Secondary),
    new ButtonBuilder().setCustomId('lic:preset:30').setLabel('30 days').setEmoji('📅').setStyle(ButtonStyle.Secondary),
    new ButtonBuilder().setCustomId('lic:preset:90').setLabel('90 days').setEmoji('📅').setStyle(ButtonStyle.Secondary),
    new ButtonBuilder().setCustomId('lic:preset:lifetime').setLabel('Lifetime').setEmoji('♾️').setStyle(ButtonStyle.Success)
  );

  const custom = new ActionRowBuilder().addComponents(
    new ButtonBuilder().setCustomId('lic:custom').setLabel('Custom').setEmoji('⚙️').setStyle(ButtonStyle.Primary)
  );

  return { embeds: [embed], components: [presets, custom] };
}

function buildCustomModal() {
  const modal = new ModalBuilder().setCustomId('lic:modal').setTitle('Create a license');

  const inputs = [
    new TextInputBuilder()
      .setCustomId('days')
      .setLabel('Duration in days (empty = lifetime)')
      .setStyle(TextInputStyle.Short)
      .setRequired(false)
      .setPlaceholder('30'),
    new TextInputBuilder()
      .setCustomId('tier')
      .setLabel('Tier: premium / trial / lifetime')
      .setStyle(TextInputStyle.Short)
      .setRequired(false)
      .setValue('premium'),
    new TextInputBuilder()
      .setCustomId('acts')
      .setLabel('Max activations (1-10)')
      .setStyle(TextInputStyle.Short)
      .setRequired(false)
      .setValue('1'),
    new TextInputBuilder()
      .setCustomId('notes')
      .setLabel('Note (optional)')
      .setStyle(TextInputStyle.Short)
      .setRequired(false)
  ];

  modal.addComponents(...inputs.map((input) => new ActionRowBuilder().addComponents(input)));
  return modal;
}

function buildKeyEmbed({ key, license }) {
  const expiration = license.expiresAt
    ? `<t:${Math.floor(new Date(license.expiresAt).getTime() / 1000)}:F>`
    : 'Lifetime';

  return new EmbedBuilder()
    .setColor(0x25ff85)
    .setTitle('✅ License created')
    .addFields(
      { name: 'Key', value: '```' + key + '```' },
      { name: 'Tier', value: String(license.tier), inline: true },
      { name: 'Activations', value: String(license.maxActivations), inline: true },
      { name: 'Expiration', value: expiration, inline: true }
    )
    .setFooter({ text: `ID: ${license._id}` });
}

function buildKeyCheckModal() {
  const modal = new ModalBuilder().setCustomId('chk:modal').setTitle('Check my license');
  const key = new TextInputBuilder()
    .setCustomId('key')
    .setLabel('Your license key')
    .setStyle(TextInputStyle.Short)
    .setRequired(true)
    .setPlaceholder('RUGIR-XXXXX-XXXXX-XXXXX-XXXXX-XXXXX');
  modal.addComponents(new ActionRowBuilder().addComponents(key));
  return modal;
}

function licenseStatusLabel(license) {
  if (license.status === 'revoked') return '⛔ Revoked';
  if (license.status === 'expired' || (license.expiresAt && new Date(license.expiresAt) <= new Date())) {
    return '⏳ Expired';
  }
  return '✅ Active';
}

// Member-facing view of their own license, with a "reset HWID" button.
function buildMemberLicenseView(license) {
  let expiration = 'Lifetime';
  if (license.expiresAt) {
    const unix = Math.floor(new Date(license.expiresAt).getTime() / 1000);
    expiration = `<t:${unix}:F> (<t:${unix}:R>)`;
  }

  const embed = new EmbedBuilder()
    .setColor(ACCENT)
    .setTitle('📄 Your license')
    .addFields(
      { name: 'Key', value: '```' + (license.keyPlain || 'unavailable') + '```' },
      { name: 'Status', value: licenseStatusLabel(license), inline: true },
      { name: 'Tier', value: String(license.tier), inline: true },
      { name: 'Activations', value: `${license.hwidHashes.length}/${license.maxActivations}`, inline: true },
      { name: 'Expiration', value: expiration }
    )
    .setFooter({ text: 'Changing PC? Request an HWID reset below.' });

  const row = new ActionRowBuilder().addComponents(
    new ButtonBuilder()
      .setCustomId(`chk:reset:${license._id}`)
      .setLabel('Reset HWID')
      .setEmoji('🔄')
      .setStyle(ButtonStyle.Danger),
    new ButtonBuilder()
      .setCustomId('chk:new')
      .setLabel('New license')
      .setEmoji('➕')
      .setStyle(ButtonStyle.Secondary)
  );

  return { embeds: [embed], components: [row] };
}

// Owner-facing reset request (pings the owner, approve / deny buttons).
function buildResetRequest(license, member, ownerId) {
  const embed = new EmbedBuilder()
    .setColor(0xffb800)
    .setTitle('🔄 HWID reset request')
    .setDescription(`${member} is requesting an HWID reset for their license.`)
    .addFields(
      { name: 'User', value: `${member} (\`${member.id}\`)` },
      { name: 'License', value: `\`${license._id}\``, inline: true },
      { name: 'Tier', value: String(license.tier), inline: true },
      { name: 'Linked activations', value: `${license.hwidHashes.length}/${license.maxActivations}`, inline: true }
    );

  const row = new ActionRowBuilder().addComponents(
    new ButtonBuilder()
      .setCustomId(`reset:ok:${license._id}:${member.id}`)
      .setLabel('Approve')
      .setEmoji('✅')
      .setStyle(ButtonStyle.Success),
    new ButtonBuilder()
      .setCustomId(`reset:no:${license._id}:${member.id}`)
      .setLabel('Deny')
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
