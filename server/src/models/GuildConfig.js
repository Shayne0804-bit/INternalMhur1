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
    },
    // Maps a level threshold to the created role id, e.g. { "5": "12345" }.
    // Lets the bot reuse auto-created level roles instead of remaking them.
    levelRoles: {
      type: Map,
      of: String,
      default: {}
    }
  },
  {
    timestamps: true
  }
);

module.exports = mongoose.model('GuildConfig', guildConfigSchema);
