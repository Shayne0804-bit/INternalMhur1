const { PermissionFlagsBits } = require('discord.js');
const { getConfig } = require('../services/guildConfigService');

// Fixed level-role tiers (English). Auto-created on demand, replacement style:
// a member holds only the highest tier they have reached.
// Ordered ascending by level.
const TIERS = [
  { level: 5, name: 'Active', color: 0x57f287 },
  { level: 10, name: 'Regular', color: 0x3498db },
  { level: 20, name: 'Veteran', color: 0x9b59b6 },
  { level: 30, name: 'Elite', color: 0xe67e22 },
  { level: 50, name: 'Legend', color: 0xf1c40f }
];

const TIER_LEVELS = TIERS.map((t) => t.level);

// Highest tier whose level requirement is met by `level` (or null below tier 1).
function tierForLevel(level) {
  let match = null;
  for (const tier of TIERS) {
    if (level >= tier.level) match = tier;
  }
  return match;
}

// Resolve the role id for a tier: persisted id -> existing role by name ->
// create it. Returns a Role, or null if creation is impossible. Persists the id.
async function resolveTierRole(guild, tier, config) {
  const key = String(tier.level);
  const storedId = config.levelRoles.get(key);

  if (storedId) {
    const cached = guild.roles.cache.get(storedId)
      || await guild.roles.fetch(storedId).catch(() => null);
    if (cached) return cached;
    config.levelRoles.delete(key); // deleted role -> recreate below
  }

  // Reuse an existing role literally named like the tier.
  const existing = guild.roles.cache.find((r) => r.name === tier.name);
  if (existing) {
    config.levelRoles.set(key, existing.id);
    await config.save();
    return existing;
  }

  const me = guild.members.me;
  if (!me || !me.permissions.has(PermissionFlagsBits.ManageRoles)) {
    console.error(`[bot] levelRoles: pas la permission ManageRoles sur ${guild.name}.`);
    return null;
  }

  try {
    const role = await guild.roles.create({
      name: tier.name,
      color: tier.color,
      hoist: true,
      reason: `Auto-created level role (level ${tier.level})`
    });
    config.levelRoles.set(key, role.id);
    await config.save();
    return role;
  } catch (err) {
    console.error(`[bot] levelRoles: creation du role ${tier.name} echouee: ${err.message}`);
    return null;
  }
}

// Sync a member's level roles for their current level (replacement style):
// grant the role for their highest reached tier, remove every other tier role.
async function syncMemberLevelRole(member, level) {
  const target = tierForLevel(level);
  if (!target) return; // below the first tier — nothing to grant yet

  const guild = member.guild;
  const config = await getConfig(guild.id);

  const targetRole = await resolveTierRole(guild, target, config);
  if (!targetRole) return;

  // A role above the bot's highest role cannot be assigned.
  const me = guild.members.me;
  if (targetRole.position >= me.roles.highest.position) {
    console.error(`[bot] levelRoles: ${targetRole.name} est au-dessus du bot, assignation impossible.`);
    return;
  }

  // Collect the ids of all known tier roles for this guild.
  const tierRoleIds = new Set();
  for (const lvl of TIER_LEVELS) {
    const id = config.levelRoles.get(String(lvl));
    if (id) tierRoleIds.add(id);
  }

  // Remove every tier role the member holds except the target one.
  const toRemove = member.roles.cache.filter((r) => tierRoleIds.has(r.id) && r.id !== targetRole.id);
  for (const role of toRemove.values()) {
    await member.roles.remove(role, 'Level role replacement').catch(() => {});
  }

  if (!member.roles.cache.has(targetRole.id)) {
    await member.roles.add(targetRole, `Reached level ${target.level}`).catch(() => {});
  }
}

module.exports = { TIERS, syncMemberLevelRole };
