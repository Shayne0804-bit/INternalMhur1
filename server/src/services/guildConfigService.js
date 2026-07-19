const GuildConfig = require('../models/GuildConfig');

// Load (or create) the persisted config row for a guild.
async function getConfig(guildId) {
  return GuildConfig.findOneAndUpdate(
    { guildId },
    { $setOnInsert: { guildId } },
    { new: true, upsert: true, setDefaultsOnInsert: true }
  );
}

// Set a single top-level field and persist.
async function setConfigField(guildId, field, value) {
  const config = await getConfig(guildId);
  config[field] = value;
  await config.save();
  return config;
}

module.exports = { getConfig, setConfigField };
