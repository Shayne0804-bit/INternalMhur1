const path = require('path');
const fs = require('fs');
const { createCanvas, GlobalFonts, loadImage } = require('@napi-rs/canvas');

const WIDTH = 900;
const HEIGHT = 260;
const ACCENT = '#9d4bff';
const BG_PATH = path.join(__dirname, '..', '..', 'assets', 'rank-bg.png');

// Rounded-rectangle path helper.
function roundRect(ctx, x, y, w, h, r) {
  ctx.beginPath();
  ctx.moveTo(x + r, y);
  ctx.arcTo(x + w, y, x + w, y + h, r);
  ctx.arcTo(x + w, y + h, x, y + h, r);
  ctx.arcTo(x, y + h, x, y, r);
  ctx.arcTo(x, y, x + w, y, r);
  ctx.closePath();
}

function abbreviate(n) {
  if (n >= 1_000_000) return `${(n / 1_000_000).toFixed(1)}M`;
  if (n >= 1_000) return `${(n / 1_000).toFixed(1)}k`;
  return String(n);
}

// Draws the background: custom image if assets/rank-bg.png exists, else a
// diagonal gradient. Returns nothing; paints straight onto ctx.
async function drawBackground(ctx) {
  if (fs.existsSync(BG_PATH)) {
    try {
      const bg = await loadImage(BG_PATH);
      ctx.drawImage(bg, 0, 0, WIDTH, HEIGHT);
      // Darken slightly so text stays readable over any image.
      ctx.fillStyle = 'rgba(0,0,0,0.45)';
      ctx.fillRect(0, 0, WIDTH, HEIGHT);
      return;
    } catch (err) {
      console.error('[bot] rankCard bg load failed, using gradient:', err.message);
    }
  }
  const grad = ctx.createLinearGradient(0, 0, WIDTH, HEIGHT);
  grad.addColorStop(0, '#1a1030');
  grad.addColorStop(1, '#2a1a4a');
  ctx.fillStyle = grad;
  ctx.fillRect(0, 0, WIDTH, HEIGHT);
}

// data: { username, avatarURL, level, rank, totalRanked, current, needed, totalXp }
async function renderRankCard(data) {
  const canvas = createCanvas(WIDTH, HEIGHT);
  const ctx = canvas.getContext('2d');

  await drawBackground(ctx);

  // Semi-transparent panel behind content.
  ctx.fillStyle = 'rgba(0,0,0,0.35)';
  roundRect(ctx, 20, 20, WIDTH - 40, HEIGHT - 40, 24);
  ctx.fill();

  // --- Avatar (circular) ---
  const avSize = 160;
  const avX = 50;
  const avY = (HEIGHT - avSize) / 2;
  try {
    const avatar = await loadImage(data.avatarURL);
    ctx.save();
    ctx.beginPath();
    ctx.arc(avX + avSize / 2, avY + avSize / 2, avSize / 2, 0, Math.PI * 2);
    ctx.closePath();
    ctx.clip();
    ctx.drawImage(avatar, avX, avY, avSize, avSize);
    ctx.restore();
  } catch {
    ctx.fillStyle = ACCENT;
    ctx.beginPath();
    ctx.arc(avX + avSize / 2, avY + avSize / 2, avSize / 2, 0, Math.PI * 2);
    ctx.fill();
  }
  // Accent ring around the avatar.
  ctx.strokeStyle = ACCENT;
  ctx.lineWidth = 6;
  ctx.beginPath();
  ctx.arc(avX + avSize / 2, avY + avSize / 2, avSize / 2, 0, Math.PI * 2);
  ctx.stroke();

  const contentX = avX + avSize + 40;

  // --- Username ---
  ctx.fillStyle = '#ffffff';
  ctx.font = 'bold 42px sans-serif';
  const name = data.username.length > 20 ? `${data.username.slice(0, 19)}…` : data.username;
  ctx.fillText(name, contentX, 90);

  // --- Level & rank (right-aligned) ---
  ctx.textAlign = 'right';
  ctx.fillStyle = ACCENT;
  ctx.font = 'bold 40px sans-serif';
  ctx.fillText(`LVL ${data.level}`, WIDTH - 50, 80);
  ctx.fillStyle = '#cfcfe0';
  ctx.font = 'bold 26px sans-serif';
  ctx.fillText(`#${data.rank} / ${data.totalRanked}`, WIDTH - 50, 115);
  ctx.textAlign = 'left';

  // --- XP progress bar ---
  const barX = contentX;
  const barY = 150;
  const barW = WIDTH - contentX - 50;
  const barH = 34;
  const ratio = data.needed > 0 ? Math.max(0, Math.min(1, data.current / data.needed)) : 0;

  ctx.fillStyle = 'rgba(255,255,255,0.15)';
  roundRect(ctx, barX, barY, barW, barH, barH / 2);
  ctx.fill();

  if (ratio > 0) {
    ctx.fillStyle = ACCENT;
    roundRect(ctx, barX, barY, Math.max(barH, barW * ratio), barH, barH / 2);
    ctx.fill();
  }

  // XP text under the bar.
  ctx.fillStyle = '#e8e8f0';
  ctx.font = '22px sans-serif';
  ctx.fillText(`${abbreviate(data.current)} / ${abbreviate(data.needed)} XP`, barX, barY + barH + 34);

  ctx.textAlign = 'right';
  ctx.fillStyle = '#9a9ab0';
  ctx.fillText(`Total: ${abbreviate(data.totalXp)} XP`, WIDTH - 50, barY + barH + 34);
  ctx.textAlign = 'left';

  return canvas.toBuffer('image/png');
}

module.exports = { renderRankCard };
