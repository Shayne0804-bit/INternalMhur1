const mongoose = require('mongoose');

const licenseSchema = new mongoose.Schema(
  {
    keyHash: {
      type: String,
      required: true,
      unique: true,
      index: true
    },
    tier: {
      type: String,
      enum: ['trial', 'premium', 'lifetime'],
      default: 'premium',
      index: true
    },
    status: {
      type: String,
      enum: ['active', 'revoked', 'expired'],
      default: 'active',
      index: true
    },
    hwidHashes: {
      type: [String],
      default: []
    },
    maxActivations: {
      type: Number,
      default: 1,
      min: 1,
      max: 10
    },
    expiresAt: {
      type: Date,
      default: null,
      index: true
    },
    notes: {
      type: String,
      default: ''
    },
    lastSeenAt: {
      type: Date,
      default: null
    },
    lastVersion: {
      type: String,
      default: null
    },
    lastIp: {
      type: String,
      default: null
    },
    discordUserId: {
      type: String,
      default: null,
      index: true
    },
    discordUsername: {
      type: String,
      default: null
    },
    keyPlain: {
      type: String,
      default: null
    }
  },
  {
    timestamps: true
  }
);

licenseSchema.methods.isExpired = function isExpired(now = new Date()) {
  return Boolean(this.expiresAt && this.expiresAt <= now);
};

licenseSchema.methods.toAdminJSON = function toAdminJSON() {
  return {
    id: this._id.toString(),
    tier: this.tier,
    status: this.status,
    maxActivations: this.maxActivations,
    activationCount: this.hwidHashes.length,
    expiresAt: this.expiresAt,
    notes: this.notes,
    lastSeenAt: this.lastSeenAt,
    lastVersion: this.lastVersion,
    lastIp: this.lastIp,
    createdAt: this.createdAt,
    updatedAt: this.updatedAt
  };
};

module.exports = mongoose.model('License', licenseSchema);
