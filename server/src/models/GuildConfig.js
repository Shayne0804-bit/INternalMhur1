const mongoose = require('mongoose');

// Per-guild bot configuration (welcome channel, etc.). One document per server.
const guildConfigSchema = new mongoose.Schema(
  {
    guildId: {
      type: String,
      required: true,
      unique: true,
      index: true
    },
    welcomeChannelId: {
      type: String,
      default: null
    }
  },
  {
    timestamps: true
  }
);

module.exports = mongoose.model('GuildConfig', guildConfigSchema);
