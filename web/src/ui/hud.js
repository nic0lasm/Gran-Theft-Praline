// HUD: money, stars, weapon, health/armour, minimap with GPS, objective,
// subtitles, big messages, shops, full map, menus.

import { WEAPONS } from '../systems/weapons.js';
import { SHOP_ICONS, PRALINE_COUNT } from '../systems/pickups.js';
import { MISSIONS } from '../systems/missions.js';
import { formatMoney, dist } from '../util.js';

const $ = (id) => document.getElementById(id);

export class Hud {
  constructor(game) {
    this.game = game;
    this.el = {
      root: $('hud'), money: $('money'), stars: $('stars'), weapon: $('weapon'), ammo: $('ammo'),
      health: $('health-bar'), armor: $('armor-bar'), zone: $('zone'), clock: $('clock'), help: $('help'),
      objective: $('objective'), subtitle: $('subtitle'), big: $('big'), bigTitle: $('big-title'), bigSub: $('big-sub'),
      toast: $('toast'), crosshair: $('crosshair'), speed: $('speed'), fade: $('fade'), hurt: $('hurt'),
      progress: $('progress'), progressBar: $('progress-bar'), progressLabel: $('progress-label'), timer: $('timer'),
      shop: $('shop'), fullmap: $('fullmap'), mapCanvas: $('map-canvas'), arrest: $('arrest'),
    };
    this.mini = $('minimap');
    this.mctx = this.mini.getContext('2d');
    this.helpText = '';
    this.helpSet = false;
    this.toasts = [];
    this.bigT = 0;
    this.route = null;
    this.routeT = 0;
    this.shopOpen = false;
    this.mapOpen = false;
    this.lastMoney = -1;
  }

  help(t) { this.helpText = t; this.helpSet = true; }
  objective(t) { this.el.objective.textContent = t || ''; this.el.objective.style.display = t ? 'block' : 'none'; }
  subtitle(who, text) {
    const s = this.el.subtitle;
    if (!who) { s.style.display = 'none'; return; }
    s.innerHTML = `<b>${who} :</b> ${text}`;
    s.style.display = 'block';
  }
  toast(t) {
    this.toasts.push({ t, life: 3.5 });
    if (this.toasts.length > 4) this.toasts.shift();
  }
  big(title, sub = '', dur = 3) {
    this.el.bigTitle.textContent = title;
    this.el.bigSub.textContent = sub;
    this.el.big.style.display = 'flex';
    this.bigT = dur;
  }
  progress(label, k) {
    if (label === null) { this.el.progress.style.display = 'none'; return; }
    this.el.progress.style.display = 'block';
    this.el.progressLabel.textContent = label;
    this.el.progressBar.style.width = `${Math.max(0, Math.min(1, k)) * 100}%`;
  }
  timer(sec) {
    if (sec === null) { this.el.timer.style.display = 'none'; return; }
    this.el.timer.style.display = 'block';
    const s = Math.max(0, Math.ceil(sec));
    this.el.timer.textContent = `${Math.floor(s / 60)}:${String(s % 60).padStart(2, '0')}`;
    this.el.timer.classList.toggle('urgent', s < 30);
  }
  fade(dur, mid) {
    const f = this.el.fade;
    f.style.transition = `opacity ${dur / 2}s`;
    f.style.opacity = 1;
    setTimeout(() => { mid && mid(); f.style.opacity = 0; }, (dur / 2) * 1000);
  }

  shop(title, desc, items) {
    const g = this.game;
    this.shopOpen = true;
    g.setPaused(true, 'shop');
    const el = this.el.shop;
    const render = () => {
      el.innerHTML = `<h2>${title}</h2><p class="desc">${desc}</p><p class="cash">Votre argent : <b>${formatMoney(g.money)}</b></p>`;
      const list = document.createElement('div');
      list.className = 'items';
      items.forEach((it) => {
        const b = document.createElement('button');
        const price = it.price > 0 ? formatMoney(it.price) : it.price < 0 ? `+${formatMoney(-it.price)}` : 'gratuit';
        b.innerHTML = `<span>${it.label}</span><span class="price">${price}</span>`;
        b.onclick = () => { if (it.action() !== false) render(); };
        list.appendChild(b);
      });
      el.appendChild(list);
      const close = document.createElement('button');
      close.className = 'close';
      close.textContent = 'Fermer (Échap)';
      close.onclick = () => this.closeShop();
      el.appendChild(close);
    };
    render();
    el.style.display = 'block';
  }
  closeShop() {
    this.el.shop.style.display = 'none';
    this.shopOpen = false;
    this.game.setPaused(false);
  }

  credits(g) {
    this.big('FIN', 'Lyon a retrouvé ses pralines. Merci d’avoir joué à Gran Theft Praline !', 8);
    setTimeout(() => this.toast('Le jeu continue en ville libre : pralines cachées, boutiques, police…'), 8000);
    void g;
  }

  // ---- Full map -------------------------------------------------------
  toggleMap() {
    const g = this.game;
    this.mapOpen = !this.mapOpen;
    this.el.fullmap.style.display = this.mapOpen ? 'flex' : 'none';
    g.setPaused(this.mapOpen, 'map');
    if (this.mapOpen) this.drawFullMap();
  }

  drawFullMap() {
    const g = this.game, w = g.world, grid = g.grid;
    const c = this.el.mapCanvas;
    const scale = Math.min((innerHeight * 0.82) / grid.h, (innerWidth * 0.62) / grid.w);
    c.width = grid.w * scale; c.height = grid.h * scale;
    const ctx = c.getContext('2d');
    ctx.imageSmoothingEnabled = false;
    ctx.drawImage(w.mapImage, 0, 0, c.width, c.height);
    const P = (x, z) => [(x - grid.minX) * scale, (z - grid.minZ) * scale];
    ctx.font = `bold ${Math.max(10, 13 * scale)}px system-ui, sans-serif`;
    ctx.textAlign = 'center';
    const label = (x, z, t, color) => {
      const [px, py] = P(x, z);
      ctx.fillStyle = 'rgba(0,0,0,0.6)';
      ctx.fillText(t, px + 1, py + 1);
      ctx.fillStyle = color;
      ctx.fillText(t, px, py);
    };
    label(10, -60, 'Rue de la République', '#fff');
    label(0, (w.plaza.z0 + w.plaza.z1) / 2 - 20, 'Place Bellecour', '#fff');
    label(318, -200, 'Rhône', '#cfe8ff');
    label(-318, -200, 'Saône', '#cfe8ff');
    for (const [id, poi] of Object.entries(w.pois)) {
      const [px, py] = P(poi.x, poi.z);
      ctx.fillStyle = poi.color;
      ctx.beginPath(); ctx.arc(px, py, 6, 0, Math.PI * 2); ctx.fill();
      ctx.strokeStyle = '#fff'; ctx.lineWidth = 2; ctx.stroke();
      if (['tatie', 'armurerie', 'planque', 'peinture', 'garage', 'boulangerie', 'hopital', 'commissariat'].includes(id)) label(poi.x, poi.z - 9, poi.name, '#fff');
    }
    for (const t of w.traboules) {
      for (const d of [t.a, t.b]) {
        const [px, py] = P(d.x, d.z);
        ctx.fillStyle = '#8fd3ff';
        ctx.fillRect(px - 3, py - 3, 6, 6);
      }
    }
    if (g.missions.blip) {
      const [px, py] = P(g.missions.blip.x, g.missions.blip.z);
      ctx.fillStyle = '#ff4fa3';
      ctx.beginPath(); ctx.arc(px, py, 9, 0, Math.PI * 2); ctx.fill();
    }
    const pl = g.player;
    const [px, py] = P(pl.x, pl.z);
    ctx.save();
    ctx.translate(px, py);
    ctx.rotate(-pl.heading + Math.PI);
    ctx.fillStyle = '#fff';
    ctx.beginPath(); ctx.moveTo(0, -10); ctx.lineTo(7, 8); ctx.lineTo(0, 4); ctx.lineTo(-7, 8); ctx.fill();
    ctx.restore();
    $('map-legend').innerHTML = `
      <h3>Carte de la Presqu'île</h3>
      <p>Données : IGN BD TOPO (rue de la République), le reste est procédural.</p>
      <ul>
        <li><i style="background:#c2185b"></i>Tatie Rose (missions)</li>
        <li><i style="background:#2e7d32"></i>Planque (sauvegarde)</li>
        <li><i style="background:#37474f"></i>Armurerie</li>
        <li><i style="background:#8d6e63"></i>Boulangerie (soins)</li>
        <li><i style="background:#1565c0"></i>Peinture Express (efface la recherche)</li>
        <li><i style="background:#ef6c00"></i>Garage (revente de voitures)</li>
        <li><i style="background:#8fd3ff;border-radius:0"></i>Traboules</li>
        <li><i style="background:#ff4fa3"></i>Objectif</li>
      </ul>
      <p>Missions : ${Math.min(g.missions.index, MISSIONS.length)} / ${MISSIONS.length}<br>
      Pralines cachées : ${g.save.pralines.length} / ${PRALINE_COUNT}</p>
      <p class="dim">M ou Échap pour fermer</p>`;
  }

  // ---- Minimap ----------------------------------------------------------
  drawMinimap(dt) {
    const g = this.game, pl = g.player, grid = g.grid;
    const ctx = this.mctx, W = this.mini.width, H = this.mini.height;
    const zoom = pl.vehicle ? 1.1 + Math.min(pl.vehicle.speed, 40) * 0.012 : 1.0; // metres per pixel
    const yaw = g.cam.yaw;
    ctx.save();
    ctx.clearRect(0, 0, W, H);
    ctx.beginPath(); ctx.arc(W / 2, H / 2, W / 2 - 2, 0, Math.PI * 2); ctx.clip();
    ctx.fillStyle = '#3f5f78';
    ctx.fillRect(0, 0, W, H);
    ctx.translate(W / 2, H / 2);
    // Up on the minimap = camera forward.
    ctx.rotate(yaw);
    ctx.scale(1 / zoom, 1 / zoom);
    ctx.translate(-(pl.x - grid.minX), -(pl.z - grid.minZ));
    ctx.imageSmoothingEnabled = false;
    ctx.drawImage(g.world.mapImage, 0, 0);
    const P = (x, z) => [x - grid.minX, z - grid.minZ];
    // GPS route.
    this.routeT -= dt;
    const b = g.missions.blip;
    if (b && this.routeT <= 0) {
      this.routeT = 1.2;
      this.route = dist(pl.x, pl.z, b.x, b.z) > 15 ? grid.findPath(pl.x, pl.z, b.x, b.z, 30000, 1.0) : null;
    }
    if (!b) this.route = null;
    if (this.route) {
      ctx.strokeStyle = '#ff4fa3';
      ctx.lineWidth = 4 * zoom;
      ctx.lineJoin = 'round';
      ctx.beginPath();
      ctx.moveTo(...P(pl.x, pl.z));
      for (const p of this.route) ctx.lineTo(...P(p.x, p.z));
      ctx.stroke();
    }
    const dot = (x, z, r, color, stroke = '#fff') => {
      const [px, py] = P(x, z);
      ctx.beginPath(); ctx.arc(px, py, r * zoom, 0, Math.PI * 2);
      ctx.fillStyle = color; ctx.fill();
      if (stroke) { ctx.lineWidth = 1.5 * zoom; ctx.strokeStyle = stroke; ctx.stroke(); }
    };
    for (const [id, poi] of Object.entries(g.world.pois)) {
      if (id === 'tatie' && (g.missions.active || g.missions.index >= MISSIONS.length)) continue;
      if (SHOP_ICONS[id] || id === 'tatie' || id === 'hopital' || id === 'commissariat') dot(poi.x, poi.z, 5, poi.color);
    }
    for (const t of g.world.traboules) {
      for (const d of [t.a, t.b]) {
        const [px, py] = P(d.x, d.z);
        ctx.fillStyle = '#8fd3ff';
        ctx.fillRect(px - 2.5 * zoom, py - 2.5 * zoom, 5 * zoom, 5 * zoom);
      }
    }
    const flash = Math.floor(g.time * 4) % 2;
    for (const v of g.police.units) if (!v.dead) dot(v.x, v.z, 4, flash ? '#ff3030' : '#3060ff', null);
    for (const c of g.police.cops) if (!c.dead) dot(c.x, c.z, 2.5, flash ? '#3060ff' : '#ff3030', null);
    if (g.police.heli) dot(g.police.heli.x, g.police.heli.z, 5, '#ffffff', '#3060ff');
    ctx.restore();
    // Objective blip, clamped to the edge.
    if (b) {
      const dx = b.x - pl.x, dz = b.z - pl.z;
      const a = yaw;
      let sx = (dx * Math.cos(a) - dz * Math.sin(a)) / zoom;
      let sy = (dx * Math.sin(a) + dz * Math.cos(a)) / zoom;
      const r = Math.hypot(sx, sy), maxR = W / 2 - 10;
      if (r > maxR) { sx *= maxR / r; sy *= maxR / r; }
      ctx.beginPath(); ctx.arc(W / 2 + sx, H / 2 + sy, 7, 0, Math.PI * 2);
      ctx.fillStyle = '#ff4fa3'; ctx.fill(); ctx.strokeStyle = '#fff'; ctx.lineWidth = 2; ctx.stroke();
    }
    // Player arrow.
    ctx.save();
    ctx.translate(W / 2, H / 2);
    ctx.rotate(-(pl.heading - yaw + Math.PI));
    ctx.fillStyle = '#fff';
    ctx.strokeStyle = '#000';
    ctx.lineWidth = 1;
    ctx.beginPath(); ctx.moveTo(0, -9); ctx.lineTo(6, 7); ctx.lineTo(0, 3); ctx.lineTo(-6, 7); ctx.closePath(); ctx.fill(); ctx.stroke();
    ctx.restore();
    // North marker.
    const nx = W / 2 + Math.sin(yaw) * (W / 2 - 12), ny = H / 2 - Math.cos(yaw) * (H / 2 - 12);
    ctx.fillStyle = '#fff';
    ctx.font = 'bold 12px system-ui';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
    ctx.fillText('N', nx, ny);
  }

  update(dt) {
    const g = this.game, pl = g.player, el = this.el;
    if (g.money !== this.lastMoney) { el.money.textContent = formatMoney(g.money); this.lastMoney = g.money; }
    // Stars.
    const w = g.police.wanted;
    const blink = w > 0 && !g.police.seen && Math.floor(g.time * 3) % 2 === 0;
    let s = '';
    for (let i = 0; i < 5; i++) s += `<span class="${i < w ? (blink ? 'on blink' : 'on') : ''}">★</span>`;
    el.stars.innerHTML = s;
    const wd = WEAPONS[pl.weapon];
    el.weapon.textContent = wd.name;
    const inv = pl.ammo[pl.weapon];
    el.ammo.textContent = wd.melee ? '' : inv ? `${inv.clip} / ${inv.reserve}${g.weapons.reloadT > 0 ? ' · recharge…' : ''}` : '';
    el.health.style.width = `${Math.max(0, pl.health)}%`;
    el.armor.style.width = `${Math.max(0, pl.armor)}%`;
    el.armor.parentElement.style.display = pl.armor > 0 ? 'block' : 'none';
    el.zone.textContent = g.world.zoneName(pl.x, pl.z);
    el.clock.textContent = g.sky.label();
    // Help text (set by systems each frame).
    el.help.style.display = this.helpSet && this.helpText ? 'block' : 'none';
    el.help.textContent = this.helpText;
    this.helpSet = false;
    // Toasts.
    for (const t of this.toasts) t.life -= dt;
    this.toasts = this.toasts.filter((t) => t.life > 0);
    el.toast.innerHTML = this.toasts.map((t) => `<div style="opacity:${Math.min(1, t.life)}">${t.t}</div>`).join('');
    if (this.bigT > 0) { this.bigT -= dt; if (this.bigT <= 0) el.big.style.display = 'none'; }
    el.crosshair.style.display = !pl.vehicle && !pl.dead && (g.input.aiming() || g.input.firing()) && pl.weapon !== 'fists' ? 'block' : 'none';
    el.crosshair.classList.toggle('target', !!(pl.aimTarget && (pl.aimTarget.kind === 'ped' || pl.aimTarget.kind === 'vehicle')));
    if (pl.vehicle) {
      el.speed.style.display = 'block';
      el.speed.innerHTML = `${Math.round(pl.vehicle.speed * 3.6)} <small>km/h</small><div class="vh"><div style="width:${(100 * pl.vehicle.health) / pl.vehicle.maxHealth}%"></div></div>`;
    } else el.speed.style.display = 'none';
    el.hurt.style.opacity = Math.min(0.6, pl.hurtFlash * 2 + (pl.health < 25 ? 0.25 + Math.sin(g.time * 5) * 0.1 : 0));
    el.arrest.style.display = g.police.arrest > 0.1 ? 'block' : 'none';
    this.drawMinimap(dt);
  }
}
