const mongoose = require('mongoose');

// A posted reaction-role panel: one message whose emoji reactions grant roles.
const reactionRolePanelSchema = new mongoose.Schema(
  {
    guildId: {
      type: String,
      required: true,
      index: true
    },
    channelId: {
      type: String,
      required: true
    },
    messageId: {
      type: String,
      required: true,
      unique: true,
      index: true
    },
    // emojiKey = custom emoji id, or the unicode emoji char.
    mappings: {
      type: [{ emojiKey: String, roleId: String }],
      default: []
    }
  },
  {
    timestamps: true
  }
);

module.exports = mongoose.model('ReactionRolePanel', reactionRolePanelSchema);
