const mongoose = require('mongoose');

// Per-guild bot configuration. One document per server. Holds channel/role
// wiring for welcome, tickets, mod-logs, auto-roles, customer role, etc.
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
    },
    // Role granted to members with a valid license.
    customerRoleId: {
      type: String,
      default: null
    },
    // Role auto-granted to every new member on join.
    memberRoleId: {
      type: String,
      default: null
    },
    // Staff role added to ticket channels (besides admins).
    staffRoleId: {
      type: String,
      default: null
    },
    // Channel where moderation actions are logged.
    modlogChannelId: {
      type: String,
      default: null
    },
    // Category under which ticket channels are created.
    ticketCategoryId: {
      type: String,
      default: null
    },
    // Channels excluded from XP gain.
    noXpChannelIds: {
      type: [String],
      default: []
    }
  },
  {
    timestamps: true
  }
);

module.exports = mongoose.model('GuildConfig', guildConfigSchema);
