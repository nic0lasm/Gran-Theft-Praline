// Weapons: fists, pistol, SMG, shotgun. Hitscan against the city, people
// and vehicles. Fictional names, no real brands.

import * as THREE from 'three';
import { T } from '../world/grid.js';
import { rand, dist } from '../util.js';

export const WEAPONS = {
  fists: { id: 'fists', name: 'Poings', melee: true, dmg: 18, rate: 0.45, range: 1.6 },
  pistol: { id: 'pistol', name: 'Pistolet « Gone 9 »', dmg: 34, rate: 0.26, clip: 12, spread: 0.012, range: 90, sound: 'pistol', price: 400, ammoPrice: 60, ammoPack: 24 },
  smg: { id: 'smg', name: 'Pistolet-mitrailleur « Ficelle »', dmg: 17, rate: 0.085, clip: 32, spread: 0.035, range: 70, sound: 'smg', auto: true, price: 1600, ammoPrice: 120, ammoPack: 96 },
  shotgun: { id: 'shotgun', name: 'Fusil à pompe « Bouchon »', dmg: 13, pellets: 8, rate: 0.85, clip: 6, spread: 0.09, range: 35, sound: 'shotgun', price: 2200, ammoPrice: 100, ammoPack: 18 },
};
export const WEAPON_ORDER = ['fists', 'pistol', 'smg', 'shotgun'];


export class WeaponSystem {
  constructor(game) {
    this.game = game;
    this.cd = 0;
    this.reloadT = 0;
  }

  // Raycast in 3D against the world (buildings are tall solids on the grid).
  rayWorld(o, d, maxD) {
    const g = this.game.grid;
    const step = 0.3;
    for (let s = 0; s <= maxD; s += step) {
      const x = o.x + d.x * s, y = o.y + d.y * s, z = o.z + d.z * s;
      if (y <= 0) return { dist: s, point: new THREE.Vector3(x, 0, z), kind: 'ground' };
      const t = g.type(x, z);
      if (t === T.BUILD) return { dist: s, point: new THREE.Vector3(x - d.x * step, y, z - d.z * step), kind: 'wall' };
      if (t === T.OBST && y < 3) return { dist: s, point: new THREE.Vector3(x, y, z), kind: 'wall' };
    }
    return null;
  }

  // Ray vs a standing person (vertical capsule ~ cylinder r 0.38, h 1.9; lying: low box).
  rayPerson(o, d, p, maxD) {
    const ox = o.x - p.x, oz = o.z - p.z;
    const a = d.x * d.x + d.z * d.z;
    const b = 2 * (ox * d.x + oz * d.z);
    const r = p.dead ? 0.9 : 0.38;
    const c = ox * ox + oz * oz - r * r;
    const disc = b * b - 4 * a * c;
    if (disc < 0 || a < 1e-9) return -1;
    const sq = Math.sqrt(disc);
    for (const t of [(-b - sq) / (2 * a), (-b + sq) / (2 * a)]) {
      if (t < 0 || t > maxD) continue;
      const y = o.y + d.y * t;
      if (y >= 0 && y <= (p.dead ? 0.4 : 1.95)) return t;
    }
    return -1;
  }

  // First thing hit along the ray. `ignore` = shooter entity.
  trace(o, d, maxD, ignore) {
    const g = this.game;
    let best = this.rayWorld(o, d, maxD);
    let bestD = best ? best.dist : maxD;
    let hit = best ? { ...best, target: null } : null;
    for (const p of g.peds.list) {
      if (p === ignore || p.inVehicle) continue;
      if (Math.abs(p.x - o.x) > bestD + 1 && Math.abs(p.z - o.z) > bestD + 1) continue;
      const t = this.rayPerson(o, d, p, bestD);
      if (t >= 0 && t < bestD) { bestD = t; hit = { dist: t, point: o.clone().addScaledVector(d, t), kind: 'ped', target: p }; }
    }
    const pl = g.player;
    if (ignore !== pl && !pl.vehicle && !pl.dead) {
      const t = this.rayPerson(o, d, pl, bestD);
      if (t >= 0 && t < bestD) { bestD = t; hit = { dist: t, point: o.clone().addScaledVector(d, t), kind: 'player', target: pl }; }
    }
    for (const v of g.vehicles) {
      if (ignore && (ignore === v || ignore.inVehicle === v || (ignore === pl && pl.vehicle === v))) continue;
      if (dist(v.x, v.z, o.x, o.z) > bestD + 6) continue;
      const t = v.rayHit(o, d, bestD);
      if (t >= 0 && t < bestD) { bestD = t; hit = { dist: t, point: o.clone().addScaledVector(d, t), kind: 'vehicle', target: v }; }
    }
    return hit;
  }

  applyHit(hit, dmg, from) {
    const g = this.game;
    if (!hit) return;
    if (hit.kind === 'ped') {
      g.effects.blood(hit.point);
      const head = hit.point.y > 1.55;
      hit.target.damage(dmg * (head ? 2.5 : 1), from);
    } else if (hit.kind === 'player') {
      g.effects.blood(hit.point);
      g.player.hurt(dmg);
    } else if (hit.kind === 'vehicle') {
      g.effects.sparks(hit.point, 0xffe0a0, 4);
      const v = hit.target;
      if (from === 'player') v.lastHitBy = 'player';
      v.damage(dmg * 1.2);
      if (from === 'player') {
        if (v.police) g.police.crime(3, v.x, v.z, 'cop');
        else if (v.driver && !v.driver.dead) {
          // Drivers under fire flee, sometimes on foot.
          if (v.ai) { v.ai.mode = 'panic'; v.ai.cruise = 16; }
        }
        g.events.emit('vehicleShot', v);
      }
    } else {
      g.effects.sparks(hit.point, 0xd8cdb5, 4);
    }
  }

  // Player shooting from the camera crosshair.
  playerFire(dt) {
    const g = this.game, pl = g.player, input = g.input;
    this.cd -= dt;
    if (this.reloadT > 0) {
      this.reloadT -= dt;
      if (this.reloadT <= 0) this.finishReload();
      return;
    }
    const w = WEAPONS[pl.weapon];
    const wantFire = w.auto ? input.firing() : (input.clicked & 1) !== 0;
    if (input.hit('KeyR') && !w.melee) this.reload();
    if (!wantFire || this.cd > 0) return;
    this.cd = w.rate;
    if (w.melee) { this.punch(); return; }
    const inv = pl.ammo[w.id];
    if (!inv || inv.clip <= 0) {
      if (inv && inv.reserve > 0) this.reload();
      else { g.audio.click(); this.cd = 0.3; }
      return;
    }
    inv.clip--;
    pl.shotT = 0.15;
    // Aim ray from the camera, then from the muzzle to that point.
    const cam = g.camera;
    const camDir = new THREE.Vector3();
    cam.getWorldDirection(camDir);
    const muzzle = pl.muzzlePoint();
    const aimHit = this.trace(cam.position.clone().addScaledVector(camDir, 2.5), camDir, w.range, pl);
    const aimPoint = aimHit ? aimHit.point : cam.position.clone().addScaledVector(camDir, w.range);
    const spreadK = (g.input.aiming() ? 1 : 2.2) * (pl.speed > 3 ? 1.6 : 1);
    const pellets = w.pellets || 1;
    for (let i = 0; i < pellets; i++) {
      const dir = aimPoint.clone().sub(muzzle).normalize();
      dir.x += rand.range(-1, 1) * w.spread * spreadK;
      dir.y += rand.range(-1, 1) * w.spread * spreadK;
      dir.z += rand.range(-1, 1) * w.spread * spreadK;
      dir.normalize();
      const hit = this.trace(muzzle, dir, w.range, pl);
      const end = hit ? hit.point : muzzle.clone().addScaledVector(dir, w.range);
      g.effects.tracer(muzzle, end);
      this.applyHit(hit, w.dmg, 'player');
    }
    g.effects.muzzle(muzzle);
    g.audio.gunshot(w.sound);
    g.stats.shots++;
    // Shots are a crime when someone hears them.
    g.police.crime(1, pl.x, pl.z, 'shot');
    if (inv.clip <= 0 && inv.reserve > 0) this.reload();
  }

  reload() {
    const pl = this.game.player, w = WEAPONS[pl.weapon];
    const inv = pl.ammo[w.id];
    if (!inv || inv.reserve <= 0 || inv.clip >= w.clip || this.reloadT > 0) return;
    this.reloadT = w.id === 'shotgun' ? 1.6 : 1.1;
    this.game.audio.door();
  }

  finishReload() {
    const pl = this.game.player, w = WEAPONS[pl.weapon];
    const inv = pl.ammo[w.id];
    if (!inv) return;
    const need = w.clip - inv.clip;
    const take = Math.min(need, inv.reserve);
    inv.clip += take; inv.reserve -= take;
  }

  punch() {
    const g = this.game, pl = g.player;
    pl.punchT = 0.3;
    const fx = Math.sin(pl.heading), fz = Math.cos(pl.heading);
    let target = null, bd = 1.8;
    for (const p of g.peds.list) {
      if (p.dead || p.inVehicle) continue;
      const dx = p.x - pl.x, dz = p.z - pl.z;
      const d = Math.hypot(dx, dz);
      if (d < bd && (dx * fx + dz * fz) / (d || 1) > 0.4) { bd = d; target = p; }
    }
    if (target) {
      g.audio.punch();
      target.damage(WEAPONS.fists.dmg, 'player', 'assault');
      target.x += fx * 0.4; target.z += fz * 0.4;
      if (!target.dead && target.cop) target.state = 'chase';
    }
  }

  // NPC shooting at the player (or anything) with a hit chance.
  npcShoot(shooter, target, acc) {
    const g = this.game;
    const sy = shooter.y ?? 1.4;
    const o = new THREE.Vector3(shooter.x, sy, shooter.z);
    const tx = target.x, tz = target.z;
    const ty = target.vehicle ? 1.0 : 1.2;
    const dir = new THREE.Vector3(tx - o.x, ty - o.y, tz - o.z);
    const d = dir.length();
    dir.normalize();
    const hitChance = Math.max(0.05, acc);
    const miss = rand() > hitChance;
    if (miss) {
      dir.x += rand.range(-0.08, 0.08);
      dir.y += rand.range(-0.03, 0.06);
      dir.z += rand.range(-0.08, 0.08);
      dir.normalize();
    }
    const hit = this.trace(o, dir, d + 8, shooter.heli ? null : shooter);
    const end = hit ? hit.point : o.clone().addScaledVector(dir, d + 8);
    if (!shooter.heli) {
      const m = o.clone().addScaledVector(dir, 0.6);
      g.effects.muzzle(m);
      g.effects.tracer(m, end, 0xfff0c0);
    } else g.effects.tracer(o, end, 0xfff0c0);
    const dp = dist(shooter.x, shooter.z, g.player.x, g.player.z);
    g.audio.gunshot('pistol', Math.max(0.15, 1 - dp / 80));
    if (hit && hit.kind === 'vehicle' && hit.target === g.player.vehicle) {
      g.effects.sparks(hit.point);
      hit.target.damage(14);
      if (rand() < 0.25) g.player.hurt(6);
    } else if (hit && (hit.kind === 'player' || hit.kind === 'ped' || hit.kind === 'vehicle' || hit.kind === 'wall' || hit.kind === 'ground')) {
      if (hit.kind === 'ped' && hit.target.cop) return; // no friendly fire among cops
      this.applyHit(hit, 9, 'npc');
    }
  }
}
