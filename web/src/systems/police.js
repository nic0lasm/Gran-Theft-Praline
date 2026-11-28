// Wanted level (1–5 stars), witnesses, pursuit units, cops on foot,
// helicopter, evasion, arrest.

import * as THREE from 'three';
import { Vehicle } from '../entities/vehicle.js';
import { Ped } from './peds.js';
import { Driver } from './traffic.js';
import { T } from '../world/grid.js';
import { rand, dist, clamp, wrapAngle, dampAngle } from '../util.js';

const UNITS = [0, 1, 2, 3, 4, 5];
const COP_ACC = [0, 0, 0.18, 0.26, 0.34, 0.42];

export class PoliceSystem {
  constructor(game) {
    this.game = game;
    this.wanted = 0;
    this.seen = false;
    this.lastSeen = null;
    this.evade = 0;
    this.units = []; // vehicles
    this.cops = []; // peds on foot
    this.spawnCd = 0;
    this.arrest = 0;
    this.heli = null;
    this.sirenLevel = 0;
    this.crimeCd = 0;
  }

  get evadeNeeded() { return 9 + this.wanted * 3; }

  setWanted(level, x, z) {
    level = clamp(Math.round(level), 0, 5);
    if (level > this.wanted) {
      this.game.audio.star();
      this.lastSeen = { x: x ?? this.game.player.x, z: z ?? this.game.player.z };
      this.evade = 0;
    }
    this.wanted = level;
    if (level === 0) this.clear();
  }

  clear() {
    this.wanted = 0;
    this.evade = 0;
    this.arrest = 0;
    for (const v of this.units) if (v.ai) { v.ai.mode = 'cruise'; v.siren = false; v.leaving = true; }
    for (const c of this.cops) c.leaving = true;
  }

  // A crime happened. Police nearby see it directly; otherwise a witness calls.
  crime(severity, x, z, kind = '') {
    const g = this.game;
    g.traffic.panic(x, z, 35);
    if (kind === 'shot') {
      // Gunfire alone: panic around, a witness may call it in.
      g.peds.witness(1, x, z);
    }
    const copSees = this.cops.some((c) => !c.dead && dist(c.x, c.z, x, z) < 45 && g.grid.lineOfSight(c.x, c.z, x, z)) ||
      this.units.some((v) => !v.dead && dist(v.x, v.z, x, z) < 50 && g.grid.lineOfSight(v.x, v.z, x, z));
    let target = severity;
    if (this.wanted > 0 && severity >= 2) target = Math.max(this.wanted + (severity >= 3 ? 1 : 0), severity);
    if (copSees || (this.wanted > 0 && this.seen)) {
      if (target > this.wanted) this.setWanted(target, x, z);
      else if (this.wanted > 0 && severity >= 3 && this.crimeCd <= 0) { this.crimeCd = 6; this.setWanted(Math.min(5, this.wanted + 1), x, z); }
      this.lastSeen = { x, z };
      this.evade = 0;
    } else if (kind !== 'shot') {
      g.peds.witness(target, x, z);
    }
  }

  // A witness got through to the police.
  report(severity, x, z) {
    if (severity > this.wanted) this.setWanted(severity, x, z);
    else { this.lastSeen = { x, z }; }
  }

  spawnUnit() {
    const g = this.game, pl = g.player;
    const grid = g.grid;
    for (let tries = 0; tries < 25; tries++) {
      const a = rand() * Math.PI * 2, r = rand.range(75, 140);
      const x = pl.x + Math.sin(a) * r, z = pl.z + Math.cos(a) * r;
      if (grid.type(x, z) !== T.ROAD || grid.distE(x, z) < 2) continue;
      if (g.vehicles.some((o) => dist(o.x, o.z, x, z) < 8)) continue;
      const h = Math.atan2(pl.x - x, pl.z - z);
      const v = new Vehicle('police', x, z, h);
      if (v.samplePoints().some((p) => grid.blocked(p.x, p.z))) continue;
      v.police = true;
      v.siren = true;
      v.ai = new Driver(v, { mode: 'follow', cruise: 18 + this.wanted * 4 });
      v.ai.target = this.lastSeen || { x: pl.x, z: pl.z };
      v.crew = [];
      for (let k = 0; k < 2; k++) {
        const c = new Ped(g, x, z, { cop: true });
        c.inVehicle = v;
        c.unit = v;
        g.peds.list.push(c);
        v.crew.push(c);
      }
      v.driver = v.crew[0];
      g.addVehicle(v);
      this.units.push(v);
      return v;
    }
    return null;
  }

  dismount(v) {
    if (!v.crew) return;
    v.ai.mode = 'stop';
    v.dismounted = true;
    let side = 1;
    for (const c of v.crew) {
      if (c.dead) continue;
      c.inVehicle = null;
      const p = v.doorPoint(side);
      side = -side;
      if (!this.game.grid.blocked(p.x, p.z)) { c.x = p.x; c.z = p.z; }
      c.state = 'chase';
      this.cops.push(c);
    }
    v.crew = [];
    v.driver = null;
  }

  remount(v) {
    // Cops climb back in and drive on.
    v.crew = this.cops.filter((c) => c.unit === v && !c.dead && dist(c.x, c.z, v.x, v.z) < 4);
    for (const c of v.crew) { c.inVehicle = v; this.cops.splice(this.cops.indexOf(c), 1); }
    if (v.crew.length && !v.dead) { v.driver = v.crew[0]; v.ai.mode = 'follow'; v.dismounted = false; }
  }

  // Cop on foot.
  copUpdate(c, dt) {
    const g = this.game, pl = g.player;
    c.anim = 'walk';
    const tx = pl.x, tz = pl.z;
    const d = dist(c.x, c.z, tx, tz);
    const los = d < 60 && g.grid.lineOfSight(c.x, c.z, tx, tz);
    if (c.leaving || this.wanted === 0) {
      // Go back to the car or wander off.
      c.speed = 1.4;
      if (c.unit && !c.unit.dead && !c.unit.driver) {
        const dv = dist(c.x, c.z, c.unit.x, c.unit.z);
        c.heading = dampAngle(c.heading, c.steerFree(Math.atan2(c.unit.x - c.x, c.unit.z - c.z), true), 6, dt);
        if (dv < 3) { c.leaving = false; this.remount(c.unit); return; }
      }
      c.move(dt, true);
      return;
    }
    // Back in the car when the player drives away.
    if (pl.vehicle && d > 25 && c.unit && !c.unit.dead && !c.unit.driver && dist(c.x, c.z, c.unit.x, c.unit.z) < 25) {
      c.speed = 5.5; c.anim = 'run';
      c.heading = dampAngle(c.heading, c.steerFree(Math.atan2(c.unit.x - c.x, c.unit.z - c.z), true), 8, dt);
      c.move(dt, true);
      if (dist(c.x, c.z, c.unit.x, c.unit.z) < 3) this.remount(c.unit);
      return;
    }
    const shoot = this.wanted >= 2 && los && d < 38;
    const want = this.wanted >= 2 ? (los ? 9 : 1.5) : 1.2;
    const face = Math.atan2(tx - c.x, tz - c.z);
    if (d > want) {
      c.speed = d > 12 ? 5.6 : 3.5;
      c.anim = shoot && d < 20 ? 'aim' : 'run';
      const h = c.steerFree(face, true, 2);
      c.heading = dampAngle(c.heading, h, 8, dt);
      if (!c.move(dt, true)) c.heading = c.steerFree(c.heading + 1, true);
    } else {
      c.speed = 0;
      c.heading = dampAngle(c.heading, face, 10, dt);
      c.anim = shoot ? 'aim' : 'idle';
    }
    if (shoot) {
      c.shootCd -= dt;
      if (c.shootCd <= 0) {
        c.shootCd = rand.range(0.7, 1.4);
        c.heading = face;
        g.weapons.npcShoot(c, pl, COP_ACC[this.wanted] * (pl.vehicle ? 0.7 : 1) * clamp(1.4 - d / 40, 0.3, 1.2));
      }
    }
    // Arrest when close.
    if (d < 1.9 && this.wanted <= 2 && (!pl.vehicle || pl.vehicle.speed < 1)) {
      this.arrest += dt;
    }
  }

  updateHeli(dt) {
    const g = this.game, pl = g.player;
    if (this.wanted >= 4 && !this.heli) {
      const grp = new THREE.Group();
      const mat = new THREE.MeshStandardMaterial({ color: 0x1d2d5c, roughness: 0.5, metalness: 0.3 });
      const body = new THREE.Mesh(new THREE.BoxGeometry(2.2, 2, 5), mat);
      const tail = new THREE.Mesh(new THREE.BoxGeometry(0.4, 0.5, 5), mat);
      tail.position.set(0, 0.4, -4.5);
      const rotor = new THREE.Mesh(new THREE.BoxGeometry(11, 0.08, 0.4), new THREE.MeshStandardMaterial({ color: 0x111111 }));
      rotor.position.y = 1.3;
      const glass = new THREE.Mesh(new THREE.BoxGeometry(2, 1.2, 1.4), new THREE.MeshStandardMaterial({ color: 0x223344, metalness: 0.6, roughness: 0.1 }));
      glass.position.set(0, 0.2, 2.6);
      grp.add(body, tail, rotor, glass);
      grp.traverse((o) => { if (o.isMesh) o.castShadow = true; });
      const spot = new THREE.SpotLight(0xffffff, 0, 120, 0.25, 0.5, 1);
      spot.position.set(0, -1, 0);
      grp.add(spot);
      g.scene.add(spot.target);
      this.heli = { grp, rotor, spot, x: pl.x + 120, z: pl.z + 120, y: 45, health: 600 };
      g.scene.add(grp);
    }
    if (!this.heli) return;
    const h = this.heli;
    const leaving = this.wanted < 4;
    const tx = leaving ? pl.x + 400 : (this.lastSeen?.x ?? pl.x) + Math.sin(g.time * 0.3) * 18;
    const tz = leaving ? pl.z + 400 : (this.lastSeen?.z ?? pl.z) + Math.cos(g.time * 0.3) * 18;
    const dx = tx - h.x, dz = tz - h.z, d = Math.hypot(dx, dz);
    const sp = Math.min(d, 22) * dt;
    if (d > 0.1) { h.x += (dx / d) * sp; h.z += (dz / d) * sp; }
    h.grp.position.set(h.x, h.y + Math.sin(g.time) * 0.5, h.z);
    h.grp.rotation.y = Math.atan2(dx, dz);
    h.rotor.rotation.y += dt * 40;
    h.spot.intensity = g.sky.night * 160;
    h.spot.target.position.set(pl.x, 0, pl.z);
    if (leaving && dist(h.x, h.z, pl.x, pl.z) > 300) {
      g.scene.remove(h.grp); g.scene.remove(h.spot.target); this.heli = null;
      return;
    }
    // Marksman at 5 stars.
    if (this.wanted >= 5 && this.seen && dist(h.x, h.z, pl.x, pl.z) < 70) {
      h.cd = (h.cd ?? 2) - dt;
      if (h.cd <= 0) {
        h.cd = rand.range(1.5, 3);
        g.weapons.npcShoot({ x: h.x, z: h.z, y: h.y, heli: true }, pl, 0.2);
      }
    }
  }

  update(dt) {
    const g = this.game, pl = g.player;
    this.crimeCd -= dt;
    // Line of sight to the player.
    this.seen = false;
    if (this.wanted > 0 && !pl.hidden) {
      for (const c of this.cops) {
        if (!c.dead && dist(c.x, c.z, pl.x, pl.z) < 45 && g.grid.lineOfSight(c.x, c.z, pl.x, pl.z)) { this.seen = true; break; }
      }
      if (!this.seen) {
        for (const v of this.units) {
          if (!v.dead && v.driver && dist(v.x, v.z, pl.x, pl.z) < 55 && g.grid.lineOfSight(v.x, v.z, pl.x, pl.z)) { this.seen = true; break; }
        }
      }
      if (!this.seen && this.heli && dist(this.heli.x, this.heli.z, pl.x, pl.z) < 75 && g.grid.type(pl.x, pl.z) !== T.BUILD) this.seen = true;
    }
    if (this.wanted > 0) {
      if (this.seen) { this.lastSeen = { x: pl.x, z: pl.z }; this.evade = 0; }
      else {
        this.evade += dt * (pl.traboule > 0 ? 2.5 : 1);
        if (this.evade > this.evadeNeeded) {
          this.setWanted(0);
          g.hud.toast('Vous avez semé la police.');
          g.events.emit('wantedLost');
        }
      }
    }
    // Spawn / manage units.
    const want = UNITS[this.wanted];
    const active = this.units.filter((v) => !v.dead && !v.leaving && (v.driver || v.dismounted)).length;
    this.spawnCd -= dt;
    if (active < want && this.spawnCd <= 0) { this.spawnCd = 3.5; this.spawnUnit(); }
    for (let i = this.units.length - 1; i >= 0; i--) {
      const v = this.units[i];
      const d = dist(v.x, v.z, pl.x, pl.z);
      if (v.removed || ((v.leaving || v.dead || !v.driver) && d > 120 && v !== pl.vehicle)) {
        if (!v.removed) g.removeVehicle(v);
        for (const c of v.crew || []) c.remove();
        this.units.splice(i, 1);
        continue;
      }
      if (v === pl.vehicle) { this.units.splice(i, 1); v.police = false; v.ai = null; v.siren = false; continue; }
      if (!v.driver || v.dead) continue;
      if (v.leaving) { v.ai.mode = 'cruise'; v.ai.drive(dt, g); continue; }
      v.ai.mode = 'follow';
      v.ai.cruise = 16 + this.wanted * 4;
      v.ai.target = this.seen ? { x: pl.x, z: pl.z } : this.lastSeen || { x: pl.x, z: pl.z };
      // Close and the player is on foot or stopped: get out.
      const pv = pl.vehicle;
      const stuck = (v.ai.stuckCount || 0) >= 2 && d < 45;
      if ((d < 22 && (!pv || pv.speed < 2)) || (d < 8 && v.speed < 1) || (stuck && (!pv || pv.speed < 4))) this.dismount(v);
      else v.ai.drive(dt, g);
      // Ramming the player.
    }
    for (let i = this.cops.length - 1; i >= 0; i--) {
      const c = this.cops[i];
      if (c.removed) { this.cops.splice(i, 1); continue; }
      const d = dist(c.x, c.z, pl.x, pl.z);
      if ((c.dead && c.deadTime > 30) || (d > 110 && (c.leaving || this.wanted === 0))) { c.remove(); this.cops.splice(i, 1); }
    }
    if (!this.cops.some((c) => !c.dead && dist(c.x, c.z, pl.x, pl.z) < 2.2)) this.arrest = Math.max(0, this.arrest - dt * 2);
    if (this.arrest > 1.6 && !pl.dead) { this.arrest = 0; g.busted(); }
    this.updateHeli(dt);
    // Siren volume: nearest unit with siren on.
    let near = Infinity;
    for (const v of this.units) if (v.siren && !v.dead && v.driver) near = Math.min(near, dist(v.x, v.z, pl.x, pl.z));
    this.sirenLevel = near < 150 ? 1 - near / 150 : 0;
    g.audio.siren(this.sirenLevel, g.time);
    void wrapAngle;
  }

  reset() {
    this.clear();
    const g = this.game;
    for (const v of this.units) g.removeVehicle(v);
    for (const v of this.units) for (const c of v.crew || []) c.remove();
    for (const c of this.cops) c.remove();
    this.units = []; this.cops = [];
    if (this.heli) { g.scene.remove(this.heli.grp); this.heli = null; }
  }
}
