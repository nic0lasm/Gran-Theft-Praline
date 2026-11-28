// Pickups (hidden pralines, cash, weapons, mission items), place markers and
// the shops: gunsmith, bakery, safehouse, spray shop, chop garage.

import * as THREE from 'three';
import { T } from '../world/grid.js';
import { WEAPONS } from './weapons.js';
import { makeRng, dist, rand, formatMoney } from '../util.js';

const PRALINE_COUNT = 20;

function pralineMesh(gold = false) {
  const g = new THREE.Group();
  const mat = new THREE.MeshStandardMaterial({
    color: gold ? 0xffd34d : 0xe0559a, roughness: gold ? 0.25 : 0.6, metalness: gold ? 0.8 : 0,
    emissive: gold ? 0x8a6a10 : 0x6a1040, emissiveIntensity: 0.6, flatShading: true,
  });
  const core = new THREE.Mesh(new THREE.IcosahedronGeometry(0.28, 1), mat);
  g.add(core);
  for (let i = 0; i < 10; i++) {
    const b = new THREE.Mesh(new THREE.IcosahedronGeometry(0.08, 0), mat);
    const a = i * 2.4, e = (i / 10) * Math.PI - Math.PI / 2;
    b.position.set(Math.cos(a) * Math.cos(e) * 0.28, Math.sin(e) * 0.28, Math.sin(a) * Math.cos(e) * 0.28);
    g.add(b);
  }
  return g;
}

function itemMesh(kind) {
  switch (kind) {
    case 'praline': return pralineMesh(false);
    case 'golden': { const m = pralineMesh(true); m.scale.setScalar(1.6); return m; }
    case 'cash': {
      const m = new THREE.Mesh(new THREE.BoxGeometry(0.45, 0.18, 0.25), new THREE.MeshStandardMaterial({ color: 0x3d8a3d, emissive: 0x1d5a1d, emissiveIntensity: 0.6 }));
      return m;
    }
    case 'crate': {
      const g = new THREE.Group();
      const box = new THREE.Mesh(new THREE.BoxGeometry(0.9, 0.6, 0.6), new THREE.MeshStandardMaterial({ color: 0x9a6a3a, roughness: 0.9 }));
      const lbl = new THREE.Mesh(new THREE.BoxGeometry(0.5, 0.25, 0.62), new THREE.MeshStandardMaterial({ color: 0xe0559a, emissive: 0x6a1040, emissiveIntensity: 0.5 }));
      g.add(box, lbl);
      return g;
    }
    case 'briefcase': {
      return new THREE.Mesh(new THREE.BoxGeometry(0.5, 0.38, 0.14), new THREE.MeshStandardMaterial({ color: 0x3a2618, emissive: 0x2a1608, emissiveIntensity: 0.5 }));
    }
    case 'bag': {
      return new THREE.Mesh(new THREE.IcosahedronGeometry(0.3, 1), new THREE.MeshStandardMaterial({ color: 0x8a8070, emissive: 0x2a2a10, emissiveIntensity: 0.5 }));
    }
    case 'health': {
      const g = new THREE.Group();
      g.add(new THREE.Mesh(new THREE.BoxGeometry(0.4, 0.4, 0.15), new THREE.MeshStandardMaterial({ color: 0xffffff, emissive: 0x444444 })));
      const red = new THREE.MeshStandardMaterial({ color: 0xd01010, emissive: 0x801010 });
      g.add(new THREE.Mesh(new THREE.BoxGeometry(0.28, 0.08, 0.17), red), new THREE.Mesh(new THREE.BoxGeometry(0.08, 0.28, 0.17), red));
      return g;
    }
    case 'armor': return new THREE.Mesh(new THREE.BoxGeometry(0.45, 0.5, 0.18), new THREE.MeshStandardMaterial({ color: 0x2a4a8a, emissive: 0x10204a, emissiveIntensity: 0.6 }));
    default: { // weapon
      return new THREE.Mesh(new THREE.BoxGeometry(0.12, 0.18, 0.55), new THREE.MeshStandardMaterial({ color: 0x222222, emissive: 0x444444, emissiveIntensity: 0.4 }));
    }
  }
}

function markerMesh(color) {
  const g = new THREE.Group();
  const mat = new THREE.MeshBasicMaterial({ color, transparent: true, opacity: 0.35, depthWrite: false, side: THREE.DoubleSide });
  const cyl = new THREE.Mesh(new THREE.CylinderGeometry(1.4, 1.4, 1.2, 24, 1, true), mat);
  cyl.position.y = 0.6;
  const ring = new THREE.Mesh(new THREE.RingGeometry(1.2, 1.45, 32), new THREE.MeshBasicMaterial({ color, transparent: true, opacity: 0.8, side: THREE.DoubleSide }));
  ring.rotation.x = -Math.PI / 2;
  ring.position.y = 0.05;
  const arrow = new THREE.Mesh(new THREE.ConeGeometry(0.35, 0.7, 4), new THREE.MeshBasicMaterial({ color }));
  arrow.rotation.x = Math.PI;
  arrow.position.y = 3;
  g.add(cyl, ring, arrow);
  g.userData.arrow = arrow;
  return g;
}

export const SHOP_ICONS = {
  armurerie: { color: 0xffc107, label: 'Armurerie' },
  boulangerie: { color: 0xffc107, label: 'Boulangerie' },
  planque: { color: 0x4caf50, label: 'Planque' },
  peinture: { color: 0x29b6f6, label: 'Peinture Express' },
  garage: { color: 0xff9800, label: 'Garage (revente)' },
};

export class PickupSystem {
  constructor(game) {
    this.game = game;
    this.items = [];
    this.markers = [];
    this.pralines = [];
  }

  init(collected = []) {
    const g = this.game, grid = g.grid, rng = makeRng(2026);
    // Hidden pralines across the city.
    const spots = [];
    for (let tries = 0; tries < 20000 && spots.length < PRALINE_COUNT; tries++) {
      const x = rng.range(grid.minX + 30, grid.minX + grid.w - 30);
      const z = rng.range(grid.minZ + 30, grid.minZ + grid.h - 30);
      const t = grid.type(x, z);
      if (t !== T.OPEN && t !== T.PLAZA && t !== T.GRASS) continue;
      if (spots.some((s) => dist(s.x, s.z, x, z) < 45)) continue;
      // Favour nooks: courtyards and narrow passages near walls.
      if (grid.distB(x, z) > 3 && rng() < 0.7) continue;
      spots.push({ x, z });
    }
    spots.forEach((s, i) => {
      if (collected.includes(i)) return;
      const it = this.spawn('praline', s.x, s.z, { index: i });
      this.pralines.push(it);
    });
    // Shop markers.
    for (const [id, info] of Object.entries(SHOP_ICONS)) {
      const poi = g.world.pois[id];
      if (!poi) continue;
      const m = markerMesh(info.color);
      m.position.set(poi.x, 0, poi.z);
      g.scene.add(m);
      this.markers.push({ id, mesh: m, poi });
    }
    // A few health and armour pickups.
    const extras = [['health', 'boulangerie', 4], ['armor', 'planque', -4], ['health', 'hopital', 3]];
    for (const [kind, poiId, off] of extras) {
      const p = g.world.pois[poiId];
      if (!p) continue;
      const q = grid.findNearest(p.x + off, p.z + off, (t) => t === T.OPEN, 20);
      if (q) this.spawn(kind, q.x, q.z, { respawn: 120 });
    }
  }

  spawn(kind, x, z, data = {}) {
    const mesh = itemMesh(kind);
    mesh.position.set(x, 0.8, z);
    mesh.traverse((o) => { if (o.isMesh) o.castShadow = true; });
    this.game.scene.add(mesh);
    const it = { kind, x, z, mesh, data, t: rand() * 6, active: true };
    this.items.push(it);
    return it;
  }

  remove(it) {
    this.game.scene.remove(it.mesh);
    it.active = false;
    const i = this.items.indexOf(it);
    if (i >= 0) this.items.splice(i, 1);
  }

  dropCash(x, z, amount, weapon) {
    if (amount > 0) this.spawn('cash', x + rand.range(-0.5, 0.5), z + rand.range(-0.5, 0.5), { amount, ttl: 45 });
    if (weapon) this.spawn(weapon, x + rand.range(-0.6, 0.6), z + rand.range(-0.6, 0.6), { weapon, rounds: 12, ttl: 45 });
  }

  collect(it) {
    const g = this.game, pl = g.player;
    switch (it.kind) {
      case 'praline': {
        g.save.pralines.push(it.data.index);
        g.addMoney(100);
        const n = g.save.pralines.length;
        g.hud.toast(`Praline cachée trouvée : ${n} / ${PRALINE_COUNT}  (+100 €)`);
        if (n === PRALINE_COUNT) {
          g.addMoney(5000);
          pl.armor = 100;
          pl.giveWeapon('smg', 128);
          g.hud.big('TOUTES LES PRALINES !', 'Bonus : 5 000 €, gilet pare-balles et pistolet-mitrailleur');
          g.audio.pass();
        }
        g.persist();
        break;
      }
      case 'cash': g.addMoney(it.data.amount); g.audio.cash(); break;
      case 'health': if (pl.health >= 100) return false; pl.health = Math.min(100, pl.health + 50); g.hud.toast('+50 santé'); break;
      case 'armor': if (pl.armor >= 100) return false; pl.armor = 100; g.hud.toast('Gilet pare-balles'); break;
      case 'pistol': case 'smg': case 'shotgun':
        pl.giveWeapon(it.data.weapon || it.kind, it.data.rounds || 12);
        g.hud.toast(`${WEAPONS[it.data.weapon || it.kind].name} récupéré`);
        break;
      default: break; // mission items handled by missions
    }
    g.audio.pickup();
    g.events.emit('pickup', it);
    if (it.data.respawn) {
      const { kind, x, z, data } = it;
      setTimeout(() => this.spawn(kind, x, z, data), data.respawn * 1000);
    }
    return true;
  }

  // Shops: E next to the marker.
  nearShop() {
    const pl = this.game.player;
    for (const m of this.markers) {
      const r = m.id === 'peinture' || m.id === 'garage' ? 4.5 : 2.2;
      if (dist(pl.x, pl.z, m.poi.x, m.poi.z) < r) return m;
    }
    return null;
  }

  openShop(id) {
    const g = this.game, pl = g.player, hud = g.hud;
    const money = () => g.money;
    const buy = (price, fn) => () => {
      if (money() < price) { hud.toast('Pas assez d’argent.'); g.audio.fail(); return false; }
      g.addMoney(-price);
      fn();
      g.audio.cash();
      return true;
    };
    if (id === 'armurerie') {
      const items = [];
      for (const w of [WEAPONS.pistol, WEAPONS.smg, WEAPONS.shotgun]) {
        if (!pl.hasWeapon(w.id)) items.push({ label: w.name, price: w.price, action: buy(w.price, () => pl.giveWeapon(w.id, w.clip * 2)) });
        else items.push({ label: `Munitions : ${w.name} (+${w.ammoPack})`, price: w.ammoPrice, action: buy(w.ammoPrice, () => { pl.ammo[w.id].reserve += w.ammoPack; }) });
      }
      items.push({ label: 'Gilet pare-balles', price: 350, action: buy(350, () => { pl.armor = 100; }) });
      hud.shop("L'Arsenal du Rhône", 'Armes de marques fictives, paiement comptant, aucune question.', items);
    } else if (id === 'boulangerie') {
      hud.shop('Boulangerie du Bon Pain', 'Viennoiseries et douceurs lyonnaises.', [
        { label: 'Tarte à la praline (+60 santé)', price: 18, action: buy(18, () => { pl.health = Math.min(100, pl.health + 60); }) },
        { label: 'Brioche aux pralines (+30 santé)', price: 9, action: buy(9, () => { pl.health = Math.min(100, pl.health + 30); }) },
        { label: 'Coussin de Lyon (+10 santé)', price: 4, action: buy(4, () => { pl.health = Math.min(100, pl.health + 10); }) },
      ]);
    } else if (id === 'planque') {
      hud.shop('Planque', 'Votre appartement sous les toits, rue de la République.', [
        { label: 'Sauvegarder la partie', price: 0, action: () => { g.persist(); hud.toast('Partie sauvegardée.'); return true; } },
        { label: 'Dormir 6 heures (santé pleine, sauvegarde)', price: 0, action: () => { g.sky.time = (g.sky.time + 6) % 24; pl.health = 100; g.persist(); hud.toast('Bien dormi. Partie sauvegardée.'); return true; } },
      ]);
    } else if (id === 'peinture') {
      const v = pl.vehicle;
      if (!v) { hud.toast('Venez en voiture pour la repeindre.'); return; }
      const cost = g.police.wanted > 0 ? 300 : 120;
      if (g.police.wanted > 0 && g.police.seen) { hud.toast('Les policiers vous voient : semez-les d’abord !'); return; }
      if (money() < cost) { hud.toast(`Il faut ${formatMoney(cost)}.`); return; }
      g.addMoney(-cost);
      g.hud.fade(0.6, () => {
        const c = new THREE.Color().setHSL(rand(), 0.6, 0.45);
        v.bodyMat.color.copy(c);
        if (g.police.wanted > 0) { g.police.setWanted(0); g.hud.toast('Nouvelle couleur, nouvelle plaque : la police perd votre trace.'); }
        else g.hud.toast('Voiture repeinte.');
        v.health = Math.max(v.health, v.maxHealth * 0.8);
      });
    } else if (id === 'garage') {
      const v = pl.vehicle;
      if (!v) { hud.toast('Le garage rachète les voitures… venez avec une voiture.'); return; }
      const prices = { citadine: 300, berline: 600, sportive: 1600, utilitaire: 500, police: 900, taxi: 500, scooter: 150, fourgon: 2000 };
      const price = Math.round((prices[v.type] || 300) * (0.3 + 0.7 * v.health / v.maxHealth));
      hud.shop('Garage Gerland', 'On rachète, on ne pose pas de questions.', [
        { label: `Vendre ${v.def.name} (${Math.round((100 * v.health) / v.maxHealth)} %)`, price: -price, action: () => {
          if (v.mission) { hud.toast('Ce véhicule est attendu ailleurs.'); return false; }
          pl.exitVehicle(true);
          g.removeVehicle(v);
          g.addMoney(price);
          g.audio.cash();
          return true;
        } },
      ]);
    }
  }

  update(dt) {
    const g = this.game, pl = g.player;
    for (let i = this.items.length - 1; i >= 0; i--) {
      const it = this.items[i];
      it.t += dt;
      it.mesh.rotation.y += dt * 1.8;
      it.mesh.position.y = 0.8 + Math.sin(it.t * 2.5) * 0.15;
      if (it.data.ttl !== undefined) {
        it.data.ttl -= dt;
        if (it.data.ttl <= 0) { this.remove(it); continue; }
      }
      const d = dist(pl.x, pl.z, it.x, it.z);
      const reach = pl.vehicle ? 2.4 : 1.3;
      const allowCar = it.kind === 'praline' || it.kind === 'cash' || it.kind === 'health' || it.kind === 'armor';
      if (!pl.dead && d < reach && (allowCar || !pl.vehicle)) {
        if (this.collect(it) !== false) this.remove(it);
      }
    }
    for (const m of this.markers) {
      m.mesh.userData.arrow.position.y = 3 + Math.sin(g.time * 3) * 0.2;
      m.mesh.userData.arrow.rotation.y += dt * 2;
      m.mesh.visible = dist(pl.x, pl.z, m.poi.x, m.poi.z) < 160;
    }
    const shop = this.nearShop();
    if (shop && !pl.dead) {
      const label = SHOP_ICONS[shop.id].label;
      g.hud.help(`E : ${label}`);
      if (g.input.hit('KeyE')) this.openShop(shop.id);
    }
  }
}

export { markerMesh, PRALINE_COUNT };
