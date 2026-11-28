// Procedural canvas textures: Lyon stone facades, shopfronts, zinc roofs,
// and the night emissive masks that light some windows.

import * as THREE from 'three';
import { makeRng } from '../util.js';

function canvas(w, h) {
  const c = document.createElement('canvas');
  c.width = w; c.height = h;
  return c;
}

function toTexture(c, repeat = true, srgb = true) {
  const t = new THREE.CanvasTexture(c);
  if (repeat) t.wrapS = t.wrapT = THREE.RepeatWrapping;
  t.anisotropy = 8;
  if (srgb) t.colorSpace = THREE.SRGBColorSpace;
  t.needsUpdate = true;
  return t;
}

function noise(ctx, w, h, rng, amount, alpha = 0.06) {
  for (let i = 0; i < amount; i++) {
    const v = rng() < 0.5 ? 0 : 255;
    ctx.fillStyle = `rgba(${v},${v},${v},${alpha * rng()})`;
    ctx.fillRect(rng() * w, rng() * h, 1 + rng() * 3, 1 + rng() * 3);
  }
}

// Upper floors. The texture covers BAYS bays x FLOORS floors.
export const BAY_W = 3.3;
export const FLOOR_H = 3.2;
export const BAYS = 4;
export const FLOORS = 4;
export const GROUND_H = 4.2;
export const SHOP_BAYS = 12;

// style 0: dressed stone with wrought-iron balconies (rue de la République)
// style 1: rendered wall with louvred shutters (side streets)
export function makeFacade(style, seed) {
  const rng = makeRng(seed);
  const PX = 128; // pixels per bay
  const W = PX * BAYS, H = PX * FLOORS;
  const c = canvas(W, H), e = canvas(W, H);
  const ctx = c.getContext('2d'), ex = e.getContext('2d');
  // Base wall (white: tinted per building by vertex colours).
  ctx.fillStyle = style === 0 ? '#f4efe6' : '#f6eee2';
  ctx.fillRect(0, 0, W, H);
  noise(ctx, W, H, rng, 9000, 0.07);
  ex.fillStyle = '#000';
  ex.fillRect(0, 0, W, H);
  const fh = PX; // floor height in px (FLOOR_H m)
  for (let f = 0; f < FLOORS; f++) {
    const y0 = H - (f + 1) * fh; // canvas y top of this floor (v=0 at bottom)
    if (style === 0) {
      // Stone courses.
      ctx.strokeStyle = 'rgba(120,100,80,0.18)';
      ctx.lineWidth = 1;
      for (let k = 1; k < 5; k++) {
        ctx.beginPath(); ctx.moveTo(0, y0 + (k * fh) / 5); ctx.lineTo(W, y0 + (k * fh) / 5); ctx.stroke();
      }
      // Cornice band.
      ctx.fillStyle = 'rgba(90,75,60,0.25)';
      ctx.fillRect(0, y0 + fh - 6, W, 6);
      ctx.fillStyle = 'rgba(255,255,255,0.35)';
      ctx.fillRect(0, y0 + fh - 8, W, 2);
    }
    for (let b = 0; b < BAYS; b++) {
      const x0 = b * PX;
      const ww = PX * 0.42, wh = fh * 0.62;
      const wx = x0 + (PX - ww) / 2, wy = y0 + fh * 0.16;
      // Window surround.
      ctx.fillStyle = style === 0 ? 'rgba(110,95,75,0.35)' : 'rgba(255,255,255,0.7)';
      ctx.fillRect(wx - 5, wy - 6, ww + 10, wh + 9);
      // Glass.
      const g = ctx.createLinearGradient(wx, wy, wx + ww, wy + wh);
      g.addColorStop(0, '#2b3440');
      g.addColorStop(0.5, '#4c5a68');
      g.addColorStop(1, '#1f262e');
      ctx.fillStyle = g;
      ctx.fillRect(wx, wy, ww, wh);
      // Mullions.
      ctx.fillStyle = '#e9e4da';
      ctx.fillRect(wx + ww / 2 - 1.5, wy, 3, wh);
      ctx.fillRect(wx, wy + wh * 0.32, ww, 3);
      // Curtains, sometimes.
      if (rng() < 0.4) {
        ctx.fillStyle = 'rgba(230,225,210,0.55)';
        ctx.fillRect(wx + 2, wy + 2, ww * 0.22, wh - 4);
        ctx.fillRect(wx + ww * 0.76, wy + 2, ww * 0.22, wh - 4);
      }
      if (style === 1) {
        // Shutters.
        const sc = ['#6f7f6a', '#7d8f9c', '#8a6f55', '#a7a59a'][Math.floor(rng() * 4)];
        const open = rng() < 0.7;
        ctx.fillStyle = sc;
        if (open) {
          ctx.fillRect(wx - 5 - ww / 2, wy, ww / 2 - 2, wh);
          ctx.fillRect(wx + ww + 7, wy, ww / 2 - 2, wh);
        } else {
          ctx.fillRect(wx, wy, ww, wh);
        }
        ctx.strokeStyle = 'rgba(0,0,0,0.25)';
        for (let k = 0; k < wh; k += 5) {
          if (open) {
            ctx.beginPath(); ctx.moveTo(wx - 5 - ww / 2, wy + k); ctx.lineTo(wx - 7, wy + k); ctx.stroke();
            ctx.beginPath(); ctx.moveTo(wx + ww + 7, wy + k); ctx.lineTo(wx + ww + 5 + ww / 2, wy + k); ctx.stroke();
          } else {
            ctx.beginPath(); ctx.moveTo(wx, wy + k); ctx.lineTo(wx + ww, wy + k); ctx.stroke();
          }
        }
      }
      // Wrought-iron balcony on some floors.
      const balcony = style === 0 ? (f === 0 || f === 3 || rng() < 0.25) : rng() < 0.15;
      if (balcony) {
        const by = wy + wh - fh * 0.2;
        ctx.fillStyle = 'rgba(60,55,50,0.35)';
        ctx.fillRect(x0 + 6, wy + wh + 1, PX - 12, 5);
        ctx.strokeStyle = '#2a2a2a';
        ctx.lineWidth = 2;
        ctx.strokeRect(x0 + 8, by, PX - 16, fh * 0.2);
        ctx.lineWidth = 1.2;
        for (let k = x0 + 12; k < x0 + PX - 10; k += 6) {
          ctx.beginPath(); ctx.moveTo(k, by); ctx.lineTo(k, by + fh * 0.2); ctx.stroke();
        }
      }
      // Night lighting: ~35% of the windows.
      if (rng() < 0.35) {
        const warm = ['#ffcf7a', '#ffd99a', '#ffe7c0', '#c8d8ff'][Math.floor(rng() * 4)];
        ex.fillStyle = warm;
        ex.globalAlpha = 0.6 + rng() * 0.4;
        ex.fillRect(wx, wy, ww, wh);
        ex.globalAlpha = 1;
        ex.fillStyle = '#000';
        ex.fillRect(wx + ww / 2 - 1.5, wy, 3, wh);
      }
    }
  }
  return { map: toTexture(c), emissive: toTexture(e) };
}

const SHOP_COLORS = ['#7a1f2b', '#1f3d5a', '#2f5a3a', '#3a3a3a', '#8a5a1f', '#5a2f6a', '#c2185b', '#00695c'];

// Ground floor: shop windows, doors, awnings. Covers BAYS bays x GROUND_H m.
export function makeShopfront(seed) {
  const rng = makeRng(seed);
  const PX = 128;
  const W = PX * SHOP_BAYS, H = 160;
  const c = canvas(W, H), e = canvas(W, H);
  const ctx = c.getContext('2d'), ex = e.getContext('2d');
  ctx.fillStyle = '#ddd3c4';
  ctx.fillRect(0, 0, W, H);
  noise(ctx, W, H, rng, 3000, 0.08);
  ex.fillStyle = '#000';
  ex.fillRect(0, 0, W, H);
  // Plinth.
  ctx.fillStyle = '#9d978d';
  ctx.fillRect(0, H - 10, W, 10);
  // Entablature band.
  ctx.fillStyle = 'rgba(80,70,60,0.3)';
  ctx.fillRect(0, 0, W, 8);
  let b = 0;
  while (b < SHOP_BAYS) {
    const span = Math.min(SHOP_BAYS - b, rng() < 0.5 ? 2 : 1);
    const x0 = b * PX + 6, x1 = (b + span) * PX - 6;
    const col = SHOP_COLORS[Math.floor(rng() * SHOP_COLORS.length)];
    const kind = rng();
    if (kind < 0.12) {
      // Carriage door (porte cochère).
      ctx.fillStyle = '#3b2a1e';
      ctx.beginPath();
      ctx.moveTo(x0 + 10, H - 10);
      ctx.lineTo(x0 + 10, 50);
      ctx.quadraticCurveTo((x0 + x1) / 2, 18, x1 - 10, 50);
      ctx.lineTo(x1 - 10, H - 10);
      ctx.fill();
      ctx.strokeStyle = 'rgba(0,0,0,0.4)';
      ctx.beginPath(); ctx.moveTo((x0 + x1) / 2, 30); ctx.lineTo((x0 + x1) / 2, H - 10); ctx.stroke();
    } else if (kind < 0.22) {
      // Metal roller shutter (closed shop).
      ctx.fillStyle = '#8f9397';
      ctx.fillRect(x0, 36, x1 - x0, H - 46);
      ctx.strokeStyle = 'rgba(0,0,0,0.25)';
      for (let y = 40; y < H - 10; y += 4) { ctx.beginPath(); ctx.moveTo(x0, y); ctx.lineTo(x1, y); ctx.stroke(); }
      // Graffiti tag.
      ctx.strokeStyle = ['#e91e63', '#29b6f6', '#ffeb3b'][Math.floor(rng() * 3)];
      ctx.lineWidth = 3;
      ctx.beginPath();
      let gx = x0 + 15;
      ctx.moveTo(gx, 100);
      for (let k = 0; k < 6; k++) { gx += 12; ctx.lineTo(gx, 80 + rng() * 40); }
      ctx.stroke();
      ctx.lineWidth = 1;
    } else {
      // Shop: fascia, window, door.
      ctx.fillStyle = col;
      ctx.fillRect(x0, 14, x1 - x0, 26);
      ctx.fillStyle = 'rgba(255,255,255,0.85)';
      for (let k = 0; k < 5 + span * 3; k++) {
        ctx.fillRect(x0 + 14 + k * 12 + rng() * 3, 22 + rng() * 4, 7, 9);
      }
      const g = ctx.createLinearGradient(0, 44, 0, H - 12);
      g.addColorStop(0, '#26303a');
      g.addColorStop(1, '#55616d');
      ctx.fillStyle = g;
      ctx.fillRect(x0 + 4, 46, x1 - x0 - 8, H - 58);
      // Goods in the window.
      for (let k = 0; k < 10 * span; k++) {
        ctx.fillStyle = `hsl(${Math.floor(rng() * 360)},45%,${40 + rng() * 30}%)`;
        ctx.fillRect(x0 + 10 + rng() * (x1 - x0 - 30), H - 40 - rng() * 30, 6 + rng() * 10, 8 + rng() * 14);
      }
      ctx.fillStyle = col;
      ctx.fillRect(x0 + 4, 44, x1 - x0 - 8, 4);
      ctx.fillRect((x0 + x1) / 2 - 2, 46, 4, H - 58);
      // Awning.
      if (rng() < 0.5) {
        ctx.fillStyle = col;
        ctx.beginPath(); ctx.moveTo(x0, 40); ctx.lineTo(x1, 40); ctx.lineTo(x1 + 4, 58); ctx.lineTo(x0 - 4, 58); ctx.fill();
        ctx.fillStyle = 'rgba(255,255,255,0.5)';
        for (let x = x0; x < x1; x += 16) ctx.fillRect(x, 40, 8, 18);
      }
      // Lit at night.
      ex.fillStyle = rng() < 0.7 ? '#ffe2b0' : '#bfe0ff';
      ex.globalAlpha = 0.85;
      ex.fillRect(x0 + 4, 46, x1 - x0 - 8, H - 58);
      ex.fillStyle = '#fff';
      ex.globalAlpha = 0.5;
      ex.fillRect(x0, 14, x1 - x0, 26);
      ex.globalAlpha = 1;
    }
    b += span;
  }
  const map = toTexture(c), emissive = toTexture(e);
  map.wrapT = emissive.wrapT = THREE.ClampToEdgeWrapping;
  return { map, emissive };
}

export function makeRoof(seed) {
  const rng = makeRng(seed);
  const c = canvas(256, 256);
  const ctx = c.getContext('2d');
  ctx.fillStyle = '#8a9096';
  ctx.fillRect(0, 0, 256, 256);
  ctx.strokeStyle = 'rgba(40,45,50,0.35)';
  for (let x = 0; x < 256; x += 16) { ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, 256); ctx.stroke(); }
  noise(ctx, 256, 256, rng, 4000, 0.12);
  // Chimney stacks seen from above.
  for (let k = 0; k < 3; k++) {
    ctx.fillStyle = '#a5594a';
    ctx.fillRect(rng() * 220, rng() * 220, 18, 30);
  }
  return toTexture(c);
}

// A city-wide ground texture painted from the occupancy grid.
export function makeGroundTexture(grid, T, pxPerM = 3) {
  const W = grid.w * pxPerM, H = grid.h * pxPerM;
  const c = canvas(W, H);
  const ctx = c.getContext('2d');
  const img = ctx.createImageData(W, H);
  const d = img.data;
  const rng = makeRng(42);
  const colors = {
    [T.OPEN]: [176, 170, 160],
    [T.ROAD]: [72, 74, 78],
    [T.BUILD]: [120, 116, 110],
    [T.WATER]: [60, 80, 80],
    [T.OBST]: [160, 155, 148],
    [T.PLAZA]: [176, 104, 82],
    [T.GRASS]: [86, 118, 62],
  };
  const gw = grid.w;
  for (let py = 0; py < H; py++) {
    const iz = Math.floor(py / pxPerM);
    const fz = (py % pxPerM) / pxPerM;
    for (let px = 0; px < W; px++) {
      const ix = Math.floor(px / pxPerM);
      const fx = (px % pxPerM) / pxPerM;
      const t = grid.cells[iz * gw + ix];
      let [r, g, b] = colors[t] || colors[0];
      const n = (rng() - 0.5) * (t === T.ROAD ? 14 : 12);
      r += n; g += n; b += n;
      if (t === T.ROAD) {
        // Curb: darker strip along the edge, white edge line inside.
        const de = grid.distEdge[iz * gw + ix];
        if (de <= 1.01) { r = g = b = 58; }
        else if (de <= 2.01 && fx > 0.2 && fx < 0.5) { /* nothing */ }
        // Centre dashes on wide carriageways: where the edge distance peaks.
        if (de >= 3) {
          const l = grid.distEdge[iz * gw + ix - 1] || 0, rr = grid.distEdge[iz * gw + ix + 1] || 0;
          const u = grid.distEdge[(iz - 1) * gw + ix] || 0, dn = grid.distEdge[(iz + 1) * gw + ix] || 0;
          const ridgeX = de >= l && de >= rr && (l < de || rr < de);
          const ridgeZ = de >= u && de >= dn && (u < de || dn < de);
          if ((ridgeX && Math.floor(iz / 3) % 2 === 0) || (ridgeZ && Math.floor(ix / 3) % 2 === 0)) {
            if (fx > 0.25 && fx < 0.75 || fz > 0.25 && fz < 0.75) { r = 205; g = 205; b = 196; }
          }
        }
      } else if (t === T.OPEN || t === T.OBST) {
        // Paving joints and a stone curb next to the road.
        if (fx < 0.12 || fz < 0.12) { r -= 14; g -= 14; b -= 14; }
        const nearRoad = grid.typeI(ix - 1, iz) === T.ROAD || grid.typeI(ix + 1, iz) === T.ROAD ||
          grid.typeI(ix, iz - 1) === T.ROAD || grid.typeI(ix, iz + 1) === T.ROAD;
        if (nearRoad) { r = 196; g = 192; b = 184; }
      } else if (t === T.PLAZA) {
        // Gravel.
        const k = (rng() - 0.5) * 30;
        r += k; g += k * 0.6; b += k * 0.5;
      } else if (t === T.GRASS) {
        const k = (rng() - 0.5) * 25;
        g += k;
      }
      const o = (py * W + px) * 4;
      d[o] = r; d[o + 1] = g; d[o + 2] = b; d[o + 3] = 255;
    }
  }
  ctx.putImageData(img, 0, 0);
  const t = toTexture(c, false);
  t.minFilter = THREE.LinearMipmapLinearFilter;
  return t;
}

// Text sign (shop names, traboule plates).
export function makeSignTexture(text, bg = '#7a1f2b', fg = '#fff', w = 512, h = 128, font = 'bold 64px Georgia, serif') {
  const c = canvas(w, h);
  const ctx = c.getContext('2d');
  ctx.fillStyle = bg;
  ctx.fillRect(0, 0, w, h);
  ctx.strokeStyle = 'rgba(255,255,255,0.6)';
  ctx.lineWidth = 6;
  ctx.strokeRect(8, 8, w - 16, h - 16);
  ctx.fillStyle = fg;
  ctx.font = font;
  ctx.textAlign = 'center';
  ctx.textBaseline = 'middle';
  let size = parseInt(font.match(/(\d+)px/)[1], 10);
  while (ctx.measureText(text).width > w - 40 && size > 12) {
    size -= 2;
    ctx.font = font.replace(/\d+px/, size + 'px');
  }
  ctx.fillText(text, w / 2, h / 2 + 4);
  return toTexture(c, false);
}
