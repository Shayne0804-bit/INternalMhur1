const mongoose = require('mongoose');

// A moderation warning issued to a member on a guild.
const warningSchema = new mongoose.Schema(
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
    moderatorId: {
      type: String,
      required: true
    },
    reason: {
      type: String,
      default: 'No reason'
    }
  },
  {
    timestamps: true
  }
);

warningSchema.index({ guildId: 1, userId: 1 });

module.exports = mongoose.model('Warning', warningSchema);
