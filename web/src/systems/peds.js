// Pedestrians: wander the sidewalks, react to violence, call the police.
// Cops on foot are pedestrians too, driven by the police system.

import * as THREE from 'three';
import { Humanoid, randomOutfit, COP_OUTFIT } from '../entities/humanoid.js';
import { T } from '../world/grid.js';
import { rand, wrapAngle, dist, clamp, dampAngle } from '../util.js';

let nextId = 1;

export class Ped {
  constructor(game, x, z, opts = {}) {
    this.id = nextId++;
    this.game = game;
    this.cop = !!opts.cop;
    this.outfit = opts.outfit || (this.cop ? COP_OUTFIT : randomOutfit());
    this.h = new Humanoid(this.outfit);
    this.x = x; this.z = z;
    this.heading = rand() * Math.PI * 2;
    this.speed = 0;
    this.health = this.cop ? 120 : 60;
    this.state = 'wander';
    this.timer = rand.range(2, 6);
    this.threat = null;
    this.dead = false;
    this.deadTime = 0;
    this.inVehicle = null;
    this.radius = 0.35;
    this.shootCd = rand.range(0.5, 1.5);
    this.punchT = 0;
    this.persistent = !!opts.persistent;
    this.name = opts.name || null;
    this.h.gun.visible = this.cop;
    game.scene.add(this.h.root);
    this.sync();
  }

  sync() {
    const r = this.h.root;
    r.visible = !this.inVehicle;
    r.position.set(this.x, 0, this.z);
    r.rotation.y = this.heading;
  }

  remove() {
    this.game.scene.remove(this.h.root);
    this.removed = true;
  }

  damage(amount, from = null, kind = 'bullet') {
    if (this.dead) return;
    this.health -= amount;
    const g = this.game;
    if (from === 'player') {
      g.police.crime(this.cop ? 3 : 1, this.x, this.z, this.cop ? 'cop' : kind);
    }
    if (this.health <= 0) this.die(from);
    else if (!this.cop) {
      this.state = 'flee';
      this.threat = { x: g.player.x, z: g.player.z };
      this.timer = 12;
    }
  }

  die(from) {
    this.dead = true;
    this.health = 0;
    this.state = 'dead';
    this.speed = 0;
    this.deadTime = 0;
    this.h.gun.visible = false;
    if (from === 'player') {
      const g = this.game;
      g.stats.kills++;
      if (this.cop) g.stats.copKills++;
      g.police.crime(this.cop ? 4 : 2, this.x, this.z, this.cop ? 'copkill' : 'murder');
      if (rand() < (this.cop ? 0.7 : 0.45)) g.pickups.dropCash(this.x, this.z, this.cop ? 0 : rand.int(10, 80), this.cop ? 'pistol' : null);
    }
    this.game.events.emit('pedDied', this);
  }

  // Simple movement with grid collision; returns false if blocked.
  move(dt, allowRoad = true) {
    const g = this.game.grid;
    const nx = this.x + Math.sin(this.heading) * this.speed * dt;
    const nz = this.z + Math.cos(this.heading) * this.speed * dt;
    const t = g.type(nx, nz);
    const lookT = g.type(nx + Math.sin(this.heading) * 0.6, nz + Math.cos(this.heading) * 0.6);
    if (g.constructor.blocks(t) || g.constructor.blocks(lookT) || (!allowRoad && (lookT === T.ROAD))) return false;
    this.x = nx; this.z = nz;
    return true;
  }

  // Pick a free heading close to `pref` (sidewalk-only unless allowRoad).
  steerFree(pref, allowRoad, look = 2.5) {
    const g = this.game.grid;
    let best = pref, bestScore = -Infinity;
    for (let k = 0; k < 12; k++) {
      const a = pref + (k % 2 ? 1 : -1) * Math.ceil(k / 2) * (Math.PI / 6);
      const dx = Math.sin(a), dz = Math.cos(a);
      let ok = true;
      for (let s = 0.7; s <= look; s += 0.6) {
        const t = g.type(this.x + dx * s, this.z + dz * s);
        if (g.constructor.blocks(t) || (!allowRoad && t === T.ROAD)) { ok = false; break; }
      }
      if (ok) {
        const score = -Math.abs(wrapAngle(a - pref));
        if (score > bestScore) { bestScore = score; best = a; }
      }
    }
    return bestScore === -Infinity ? pref + Math.PI : best;
  }

  update(dt) {
    if (this.inVehicle) {
      this.x = this.inVehicle.x; this.z = this.inVehicle.z;
      this.sync();
      return;
    }
    const g = this.game;
    let anim = 'walk';
    if (this.dead) {
      this.deadTime += dt;
      this.h.animate(dt, 0, 'dead');
      this.sync();
      return;
    }
    // Cars hitting pedestrians.
    for (const v of g.vehicles) {
      if (v.speed < 3.5 || !v.contains(this.x, this.z, 0.3)) continue;
      const dmg = v.speed * 9;
      const fromPlayer = v === g.player.vehicle;
      this.x += v.vx * 0.08; this.z += v.vz * 0.08;
      g.audio.punch();
      g.effects.blood(new THREE.Vector3(this.x, 1, this.z));
      this.damage(dmg, fromPlayer ? 'player' : null, 'runover');
      v.vx *= 0.92; v.vz *= 0.92;
      if (this.dead) { this.sync(); return; }
    }
    if (this.cop) {
      g.police.copUpdate(this, dt);
      anim = this.anim || 'walk';
    } else {
      this.civilianUpdate(dt);
      anim = this.anim;
    }
    this.punchT = Math.max(0, this.punchT - dt);
    this.h.animate(dt, this.speed, anim, this.punchT > 0 ? 1 - this.punchT / 0.3 : 0);
    this.sync();
  }

  civilianUpdate(dt) {
    const g = this.game;
    this.timer -= dt;
    this.anim = 'walk';
    switch (this.state) {
      case 'wander': {
        this.speed = this.walkSpeed || (this.walkSpeed = rand.range(1.1, 1.6));
        if (this.timer <= 0) { this.heading += rand.range(-1, 1); this.timer = rand.range(3, 9); }
        if (!this.move(dt, false)) this.heading = this.steerFree(this.heading + rand.range(-0.5, 0.5), false);
        // Avoid the player a little.
        const dp = dist(this.x, this.z, g.player.x, g.player.z);
        if (dp < 1.2 && !g.player.vehicle) this.heading = Math.atan2(this.x - g.player.x, this.z - g.player.z);
        break;
      }
      case 'flee': {
        this.speed = 5.2;
        this.anim = 'run';
        const away = Math.atan2(this.x - this.threat.x, this.z - this.threat.z);
        this.heading = dampAngle(this.heading, this.steerFree(away, true, 2), 8, dt);
        if (!this.move(dt, true)) this.heading = this.steerFree(this.heading + Math.PI / 2, true);
        if (this.timer <= 0) { this.state = 'wander'; this.timer = 4; }
        break;
      }
      case 'phone': {
        // Calling the police after witnessing a crime.
        this.speed = 0;
        this.anim = 'phone';
        if (this.timer <= 0) {
          g.police.report(this.crimeSeverity || 1, this.crimeX, this.crimeZ);
          this.state = 'flee';
          this.threat = { x: this.crimeX, z: this.crimeZ };
          this.timer = 10;
        }
        break;
      }
      case 'hands': {
        this.speed = 0;
        this.anim = 'hands';
        if (this.timer <= 0) { this.state = 'flee'; this.threat = { x: g.player.x, z: g.player.z }; this.timer = 8; }
        break;
      }
      case 'cower': {
        this.speed = 0;
        this.anim = 'hands';
        if (this.timer <= 0) { this.state = 'wander'; }
        break;
      }
      case 'idle': {
        this.speed = 0;
        this.anim = 'idle';
        break;
      }
    }
  }
}

export class PedSystem {
  constructor(game) {
    this.game = game;
    this.list = [];
    this.target = 46;
    this.spawnCd = 0;
  }

  spawnCiv(x, z, opts) {
    const p = new Ped(this.game, x, z, opts);
    this.list.push(p);
    return p;
  }

  findSpawn(px, pz, minR, maxR, needHidden = true) {
    const g = this.game.grid, cam = this.game.camera;
    for (let tries = 0; tries < 30; tries++) {
      const a = rand() * Math.PI * 2, r = rand.range(minR, maxR);
      const x = px + Math.sin(a) * r, z = pz + Math.cos(a) * r;
      const t = g.type(x, z);
      if (t !== T.OPEN && t !== T.PLAZA) continue;
      const db = g.distB(x, z);
      if (t === T.OPEN && (db > 4 || db < 0.8)) continue;
      if (needHidden && r < 80) {
        // Not in plain sight: behind a building or outside the view cone.
        const fx = -Math.sin(this.game.cam.yaw), fz = -Math.cos(this.game.cam.yaw);
        const dx = x - cam.position.x, dz = z - cam.position.z;
        const inFront = (dx * fx + dz * fz) / Math.hypot(dx, dz) > 0.5;
        if (inFront && g.lineOfSight(cam.position.x, cam.position.z, x, z)) continue;
      }
      return { x, z };
    }
    return null;
  }

  // Witnesses: civilians who see a crime call the police after a delay.
  witness(severity, x, z) {
    const g = this.game.grid;
    let caller = null, best = Infinity;
    for (const p of this.list) {
      if (p.dead || p.cop || p.inVehicle) continue;
      const d = dist(p.x, p.z, x, z);
      if (d > 40) continue;
      if (!g.lineOfSight(p.x, p.z, x, z)) continue;
      // Everyone nearby panics.
      if (p.state === 'wander' || p.state === 'idle') {
        p.state = 'flee'; p.threat = { x, z }; p.timer = rand.range(6, 12);
      }
      if (d < best && p.state !== 'hands') { best = d; caller = p; }
    }
    if (caller && this.game.police.wanted < severity) {
      caller.state = 'phone';
      caller.timer = 2.5;
      caller.crimeSeverity = severity;
      caller.crimeX = x; caller.crimeZ = z;
      return caller;
    }
    return null;
  }

  // Threaten: pedestrians the player aims at raise their hands.
  aimedAt(p) {
    if (p.dead || p.cop) return;
    if (p.state !== 'hands') { p.state = 'hands'; p.timer = 5; }
    else p.timer = Math.max(p.timer, 2);
  }

  update(dt) {
    const g = this.game, pl = g.player;
    const px = pl.x, pz = pl.z;
    const night = g.sky.night;
    const target = Math.round(this.target * (1 - night * 0.55));
    this.spawnCd -= dt;
    const alive = this.list.filter((p) => !p.cop && !p.dead && !p.inVehicle).length;
    if (this.spawnCd <= 0 && alive < target) {
      this.spawnCd = 0.15;
      const s = this.findSpawn(px, pz, 35, 110);
      if (s) this.spawnCiv(s.x, s.z);
    }
    for (let i = this.list.length - 1; i >= 0; i--) {
      const p = this.list[i];
      if (p.removed) { this.list.splice(i, 1); continue; }
      const d = dist(p.x, p.z, px, pz);
      const tooFar = d > 150 && !p.persistent && !p.inVehicle;
      const oldBody = p.dead && p.deadTime > 40 && d > 30;
      if (tooFar || oldBody) { p.remove(); this.list.splice(i, 1); continue; }
      if (d > 120 && !p.cop && !p.persistent) continue; // freeze far ones
      p.update(dt);
    }
    // Keep pedestrians out of each other a bit (cheap pairwise on near ones).
    const near = this.list.filter((p) => !p.dead && !p.inVehicle && Math.abs(p.x - px) < 40 && Math.abs(p.z - pz) < 40);
    for (let i = 0; i < near.length; i++) {
      for (let j = i + 1; j < near.length; j++) {
        const a = near[i], b = near[j];
        const dx = b.x - a.x, dz = b.z - a.z, d2 = dx * dx + dz * dz;
        if (d2 > 0.49 || d2 < 1e-6) continue;
        const d = Math.sqrt(d2), push = (0.7 - d) / 2;
        const nx = dx / d, nz = dz / d;
        if (!g.grid.blocked(a.x - nx * push, a.z - nz * push)) { a.x -= nx * push; a.z -= nz * push; }
        if (!g.grid.blocked(b.x + nx * push, b.z + nz * push)) { b.x += nx * push; b.z += nz * push; }
      }
    }
    void clamp;
  }

  nearestTo(x, z, maxD, filter = () => true) {
    let best = null, bd = maxD;
    for (const p of this.list) {
      if (p.inVehicle || !filter(p)) continue;
      const d = dist(p.x, p.z, x, z);
      if (d < bd) { bd = d; best = p; }
    }
    return best;
  }
}
