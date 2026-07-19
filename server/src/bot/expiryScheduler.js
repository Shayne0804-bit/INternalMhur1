const {
  sweepExpiredLicenses,
  findLicensesExpiringWithin
} = require('../services/licenseService');
const { removeCustomerRole } = require('./customerRole');

const CHECK_INTERVAL_MS = 60 * 60 * 1000; // hourly

// DM a member that their license is about to expire.
async function sendReminder(client, license, daysLeft) {
  if (!license.discordUserId) return;
  const user = await client.users.fetch(license.discordUserId).catch(() => null);
  if (!user) return;
  const when = license.expiresAt
    ? `<t:${Math.floor(new Date(license.expiresAt).getTime() / 1000)}:R>`
    : 'soon';
  await user.send(
    `⏳ **Your license expires ${when}** (in about ${daysLeft} day${daysLeft > 1 ? 's' : ''}).\n` +
    `Renew it to keep your access. Use \`/check\` to see the details.`
  ).catch(() => {});
}

// One full pass: send reminders, then expire past-due licenses and strip roles.
async function runExpiryPass(client) {
  try {
    // 3-day reminders.
    for (const license of await findLicensesExpiringWithin(3, 'reminded3d')) {
      await sendReminder(client, license, 3);
      license.reminded3d = true;
      await license.save();
    }
    // 1-day reminders.
    for (const license of await findLicensesExpiringWithin(1, 'reminded1d')) {
      await sendReminder(client, license, 1);
      license.reminded1d = true;
      await license.save();
    }

    // Expire past-due licenses and remove the customer role everywhere.
    const expired = await sweepExpiredLicenses();
    for (const license of expired) {
      if (!license.discordUserId) continue;
      for (const guild of client.guilds.cache.values()) {
        await removeCustomerRole(guild, license.discordUserId);
      }
    }
    if (expired.length) {
      console.log(`[bot] expiry: ${expired.length} license(s) expired.`);
    }
  } catch (err) {
    console.error('[bot] expiry pass error:', err.message);
  }
}

// Start the hourly scheduler. Runs one pass shortly after startup.
function startExpiryScheduler(client) {
  setTimeout(() => runExpiryPass(client), 30 * 1000);
  setInterval(() => runExpiryPass(client), CHECK_INTERVAL_MS);
}

module.exports = { startExpiryScheduler };
