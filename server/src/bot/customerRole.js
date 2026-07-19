const { getConfig } = require('../services/guildConfigService');

// Grant the configured customer role to a member on a guild.
// Safe no-op if no role configured, role missing, or above the bot.
async function grantCustomerRole(guild, userId) {
  try {
    const config = await getConfig(guild.id);
    if (!config.customerRoleId) return false;
    const role = guild.roles.cache.get(config.customerRoleId)
      || await guild.roles.fetch(config.customerRoleId).catch(() => null);
    if (!role) return false;
    const me = guild.members.me;
    if (!me || role.position >= me.roles.highest.position) return false;
    const member = await guild.members.fetch(userId).catch(() => null);
    if (!member) return false;
    if (!member.roles.cache.has(role.id)) {
      await member.roles.add(role, 'Valid license verified');
    }
    return true;
  } catch (err) {
    console.error('[bot] grantCustomerRole:', err.message);
    return false;
  }
}

// Remove the customer role (on expiry/revocation).
async function removeCustomerRole(guild, userId) {
  try {
    const config = await getConfig(guild.id);
    if (!config.customerRoleId) return false;
    const member = await guild.members.fetch(userId).catch(() => null);
    if (!member || !member.roles.cache.has(config.customerRoleId)) return false;
    await member.roles.remove(config.customerRoleId, 'License expired/revoked');
    return true;
  } catch (err) {
    console.error('[bot] removeCustomerRole:', err.message);
    return false;
  }
}

module.exports = { grantCustomerRole, removeCustomerRole };
