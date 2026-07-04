const mongoose = require('mongoose');

const authLogSchema = new mongoose.Schema(
  {
    licenseId: {
      type: mongoose.Schema.Types.ObjectId,
      ref: 'License',
      default: null,
      index: true
    },
    keyHash: {
      type: String,
      default: null,
      index: true
    },
    hwidHash: {
      type: String,
      default: null,
      index: true
    },
    ok: {
      type: Boolean,
      required: true,
      index: true
    },
    reason: {
      type: String,
      required: true
    },
    version: {
      type: String,
      default: null
    },
    ip: {
      type: String,
      default: null
    },
    userAgent: {
      type: String,
      default: null
    },
    createdAt: {
      type: Date,
      default: Date.now,
      expires: 60 * 60 * 24 * 30
    }
  },
  {
    versionKey: false
  }
);

module.exports = mongoose.model('AuthLog', authLogSchema);
