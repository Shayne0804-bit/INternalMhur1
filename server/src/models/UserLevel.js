const mongoose = require('mongoose');

// Per-guild, per-user activity stats used by the leveling system.
// One document = one member's progression on one server.
const userLevelSchema = new mongoose.Schema(
  {
    guildId: {
      type: String,
      required: true,
      index: true
    },
    userId: {
      type: String,
      required: true,
      index: true
    },
    username: {
      type: String,
      default: null
    },
    xp: {
      type: Number,
      default: 0,
      index: true
    },
    level: {
      type: Number,
      default: 0
    },
    messageCount: {
      type: Number,
      default: 0
    },
    lastMessageAt: {
      type: Date,
      default: null
    }
  },
  {
    timestamps: true
  }
);

// A member is unique per guild.
userLevelSchema.index({ guildId: 1, userId: 1 }, { unique: true });
// Leaderboard query: highest xp first, scoped to a guild.
userLevelSchema.index({ guildId: 1, xp: -1 });

module.exports = mongoose.model('UserLevel', userLevelSchema);
