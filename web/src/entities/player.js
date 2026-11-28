// Nicolito, the player: on foot (walk, sprint, jump, aim, punch, shoot),
// stealing and driving cars, traboules, and the third-person camera.

import * as THREE from 'three';
import { Humanoid, PLAYER_OUTFIT } from './humanoid.js';
import { Grid, T } from '../world/grid.js';
import { WEAPONS, WEAPON_ORDER } from '../systems/weapons.js';
import { clamp, dampAngle, wrapAngle, dist, lerp } from '../util.js';

export class Player {
  constructor(game, x, z) {
    this.game = game;
    this.h = new Humanoid(PLAYER_OUTFIT);
    game.scene.add(this.h.root);
    this.x = x; this.z = z; this.y = 0; this.vy = 0;
    this.heading = 0;
    this.speed = 0;
    this.vx = 0; this.vz = 0;
    this.radius = 0.35;
    this.health = 100;
    this.armor = 0;
    this.stamina = 1;
    this.vehicle = null;
    this.lastVehicle = null;
    this.weapon = 'fists';
    this.ammo = {}; // id -> {clip, reserve}
    this.dead = false;
    this.punchT = 0;
    this.shotT = 0;
    this.hidden = false;
    this.traboule = 0;
    this.hurtFlash = 0;
  }

  giveWeapon(id, rounds) {
    const w = WEAPONS[id];
    if (!this.ammo[id]) this.ammo[id] = { clip: 0, reserve: 0 };
    this.ammo[id].reserve += rounds;
    if (this.ammo[id].clip === 0) {
      const take = Math.min(w.clip, this.ammo[id].reserve);
      this.ammo[id].clip += take; this.ammo[id].reserve -= take;
    }
    if (this.weapon === 'fists') this.weapon = id;
  }

  hasWeapon(id) { return id === 'fists' || !!this.ammo[id]; }

  cycleWeapon(dir) {
    const owned = WEAPON_ORDER.filter((id) => this.hasWeapon(id));
    let i = owned.indexOf(this.weapon);
    i = (i + dir + owned.length) % owned.length;
    this.weapon = owned[i];
    this.game.weapons.reloadT = 0;
  }

  hurt(amount) {
    if (this.dead || this.game.god) return;
    if (this.armor > 0) {
      const a = Math.min(this.armor, amount * 0.7);
      this.armor -= a;
      amount -= a;
    }
    this.health -= amount;
    this.hurtFlash = 0.35;
    if (this.health <= 0) { this.health = 0; this.game.wasted(); }
  }

  muzzlePoint() {
    const fx = Math.sin(this.heading), fz = Math.cos(this.heading);
    const rx = -Math.cos(this.heading), rz = Math.sin(this.heading);
    return new THREE.Vector3(this.x + fx * 0.75 + rx * 0.3, this.y + 1.45, this.z + fz * 0.75 + rz * 0.3);
  }

  // Circle vs grid cells and vehicles.
  collide() {
    const g = this.game.grid, r = this.radius;
    for (let iter = 0; iter < 2; iter++) {
      const ix0 = Math.floor(this.x - r - g.minX), ix1 = Math.floor(this.x + r - g.minX);
      const iz0 = Math.floor(this.z - r - g.minZ), iz1 = Math.floor(this.z + r - g.minZ);
      for (let iz = iz0; iz <= iz1; iz++) {
        for (let ix = ix0; ix <= ix1; ix++) {
          const t = g.typeI(ix, iz);
          if (!Grid.blocks(t)) continue;
          if (t === T.OBST && this.y > 1.1) continue; // jumped over a low obstacle
          const cx0 = g.minX + ix, cz0 = g.minZ + iz;
          const nx = clamp(this.x, cx0, cx0 + 1), nz = clamp(this.z, cz0, cz0 + 1);
          let dx = this.x - nx, dz = this.z - nz;
          const d2 = dx * dx + dz * dz;
          if (d2 >= r * r) continue;
          if (d2 < 1e-9) {
            // Centre inside the cell: push out through the nearest side.
            const l = this.x - cx0, rr = cx0 + 1 - this.x, u = this.z - cz0, dd = cz0 + 1 - this.z;
            const m = Math.min(l, rr, u, dd);
            if (m === l) this.x = cx0 - r; else if (m === rr) this.x = cx0 + 1 + r;
            else if (m === u) this.z = cz0 - r; else this.z = cz0 + 1 + r;
            continue;
          }
          const d = Math.sqrt(d2);
          this.x += (dx / d) * (r - d);
          this.z += (dz / d) * (r - d);
        }
      }
    }
    for (const v of this.game.vehicles) {
      if (Math.abs(v.x - this.x) > 4 || Math.abs(v.z - this.z) > 4) continue;
      if (!v.contains(this.x, this.z, r)) continue;
      if (this.y > v.def.hgt - 0.2) continue; // standing on the roof
      // Push out along the shallowest local axis.
      const c = Math.cos(-v.heading), s = Math.sin(-v.heading);
      const lx = this.x - v.x, lz = this.z - v.z;
      let px = lx * c + lz * s, pz = -lx * s + lz * c;
      const ox = v.halfW + r - Math.abs(px), oz = v.halfL + r - Math.abs(pz);
      if (ox < oz) px += Math.sign(px || 1) * ox; else pz += Math.sign(pz || 1) * oz;
      this.x = v.x + px * c - pz * s;
      this.z = v.z + px * s + pz * c;
      // Hit by a moving car.
      if (v.speed > 4 && v !== this.vehicle) {
        this.hurt(v.speed * 2.2);
        this.x += v.vx * 0.06; this.z += v.vz * 0.06;
        this.game.audio.punch();
      }
    }
  }

  nearestVehicle(maxD = 3.2) {
    let best = null, bd = maxD;
    for (const v of this.game.vehicles) {
      if (v.dead) continue;
      const d = dist(v.x, v.z, this.x, this.z) - Math.min(v.halfW, v.halfL) - 0.3;
      if (d < bd && v.contains(this.x, this.z, 1.6)) { bd = d; best = v; }
    }
    return best;
  }

  enterVehicle(v) {
    const g = this.game;
    if (v.locked) { g.hud.toast('Ce véhicule est verrouillé.'); return; }
    if (v.def.armored && !v.dead) { g.hud.toast('Le fourgon blindé est verrouillé de l’intérieur.'); return; }
    if (v.driver && v.driver !== 'player') {
      const d = v.driver;
      // Carjacking: the driver is thrown out.
      d.inVehicle = null;
      const p = v.doorPoint(1);
      d.x = p.x; d.z = p.z;
      if (d.cop) { d.state = 'chase'; g.police.cops.push(d); }
      else { d.state = 'flee'; d.threat = { x: this.x, z: this.z }; d.timer = 12; }
      g.police.crime(v.police ? 3 : 1, this.x, this.z, 'carjack');
      g.stats.carjacks++;
    } else if (v.parked || (!v.driver && !v.playerOwned)) {
      if (v.parked) g.police.crime(1, this.x, this.z, 'theft');
    }
    if (v.crew) {
      for (const c of v.crew) {
        if (c.inVehicle === v) { c.inVehicle = null; const p = v.doorPoint(-1); c.x = p.x; c.z = p.z; c.state = 'chase'; g.police.cops.push(c); }
      }
      v.crew = [];
    }
    v.parked = false;
    v.driver = 'player';
    v.ai = null;
    v.playerOwned = true;
    if (v.police) { const i = g.police.units.indexOf(v); if (i >= 0) g.police.units.splice(i, 1); v.police = false; v.siren = false; }
    this.vehicle = v;
    this.lastVehicle = v;
    this.h.root.visible = false;
    g.audio.door();
    g.stats.cars++;
    g.events.emit('enterVehicle', v);
  }

  exitVehicle(force = false) {
    const v = this.vehicle;
    if (!v) return;
    if (!force && v.speed > 9) return;
    const g = this.game;
    for (const side of [1, -1]) {
      const p = v.doorPoint(side);
      if (!g.grid.blocked(p.x, p.z) || side === -1) {
        this.x = p.x; this.z = p.z;
        break;
      }
    }
    if (g.grid.blocked(this.x, this.z)) {
      const f = g.grid.findNearest(this.x, this.z, (t) => !Grid.blocks(t), 10);
      if (f) { this.x = f.x; this.z = f.z; }
    }
    this.heading = v.heading;
    v.driver = null;
    this.vehicle = null;
    this.h.root.visible = true;
    g.audio.door();
    g.events.emit('exitVehicle', v);
  }

  // Through a traboule door: fade out, cross the building, fade in.
  useTraboule(t, fromA) {
    const g = this.game;
    const to = fromA ? t.b : t.a;
    g.hud.fade(0.5, () => {
      this.x = to.x + to.nx * 1.0;
      this.z = to.z + to.nz * 1.0;
      this.heading = Math.atan2(to.nx, to.nz);
      g.cam.yaw = this.heading + Math.PI;
      this.traboule = 6; // police lose track for a while
      g.hud.toast('Traboule : vous ressortez de l’autre côté de l’immeuble.');
      g.stats.traboules++;
    });
  }

  nearTraboule() {
    for (const t of this.game.world.traboules) {
      if (dist(this.x, this.z, t.a.x, t.a.z) < 2.2) return { t, fromA: true };
      if (dist(this.x, this.z, t.b.x, t.b.z) < 2.2) return { t, fromA: false };
    }
    return null;
  }

  update(dt) {
    const g = this.game, input = g.input;
    this.hurtFlash = Math.max(0, this.hurtFlash - dt);
    this.traboule = Math.max(0, this.traboule - dt);
    this.hidden = this.traboule > 3;
    if (this.dead) {
      this.h.animate(dt, 0, 'dead');
      return;
    }
    if (input.wheel) this.cycleWeapon(input.wheel > 0 ? 1 : -1);
    for (let k = 1; k <= 4; k++) if (input.hit('Digit' + k)) { const id = WEAPON_ORDER[k - 1]; if (this.hasWeapon(id)) this.weapon = id; }
    if (this.vehicle) this.updateDriving(dt);
    else this.updateOnFoot(dt);
  }

  updateDriving(dt) {
    const g = this.game, input = g.input, v = this.vehicle;
    const controls = { throttle: input.throttle(), steer: input.steer(), handbrake: input.key('Space') };
    const impact = v.physics(dt, controls, g.grid);
    if (impact > 4) {
      v.damage((impact - 4) * 30);
      g.audio.crash(impact);
      g.cam.shake = Math.min(0.6, impact * 0.03);
      if (impact > 14) this.hurt((impact - 14) * 1.5);
    }
    this.x = v.x; this.z = v.z; this.heading = v.heading;
    this.speed = v.speed;
    if (input.hit('KeyF') || input.hit('Enter')) this.exitVehicle();
    if (input.hit('KeyH')) g.audio.horn();
    g.audio.engine(true, Math.min(1, Math.abs(v.forwardSpeed) / v.def.maxSpeed), Math.abs(controls.throttle));
    // Burning car: get out!
    if (v.burnTimer > 0 && v.burnTimer < 2.5 && !v.dead) g.hud.help('Le véhicule va exploser ! Sortez (F) !');
  }

  updateOnFoot(dt) {
    const g = this.game, input = g.input;
    g.audio.engine(false, 0, 0);
    const ax = input.axis();
    const aiming = input.aiming() && this.weapon !== 'fists';
    const camYaw = g.cam.yaw;
    // Camera-relative movement. Camera looks along (-sin yaw, -cos yaw).
    const fx = -Math.sin(camYaw), fz = -Math.cos(camYaw);
    const rx = -fz, rz = fx; // screen right
    let mx = fx * ax.y + rx * ax.x, mz = fz * ax.y + rz * ax.x;
    const ml = Math.hypot(mx, mz);
    let target = 0;
    const sprint = input.sprint() && this.stamina > 0.05 && !aiming;
    if (ml > 0.1) {
      mx /= ml; mz /= ml;
      target = aiming ? 2.2 : sprint ? 7.0 : 4.0;
      if (input.key('AltLeft') || input.key('KeyC')) target = 1.6;
    }
    this.stamina = clamp(this.stamina + (sprint && ml > 0.1 ? -dt * 0.12 : dt * 0.2), 0, 1);
    const accel = this.y > 0.01 ? 3 : 14;
    this.vx = lerp(this.vx, mx * target, Math.min(1, accel * dt));
    this.vz = lerp(this.vz, mz * target, Math.min(1, accel * dt));
    this.speed = Math.hypot(this.vx, this.vz);
    if (aiming || this.shotT > 0) {
      this.heading = dampAngle(this.heading, camYaw + Math.PI, 20, dt);
    } else if (this.speed > 0.3) {
      this.heading = dampAngle(this.heading, Math.atan2(this.vx, this.vz), 12, dt);
    }
    this.x += this.vx * dt;
    this.z += this.vz * dt;
    // Jump.
    if (input.hit('Space') && this.y <= 0.001) this.vy = 5.2;
    this.vy -= 15 * dt;
    this.y = Math.max(0, this.y + this.vy * dt);
    if (this.y <= 0) this.vy = 0;
    this.collide();
    this.shotT = Math.max(0, this.shotT - dt);
    this.punchT = Math.max(0, this.punchT - dt);
    // Weapons.
    g.weapons.playerFire(dt);
    // Threatening people.
    if (aiming) {
      const cam = g.camera;
      const d = new THREE.Vector3();
      cam.getWorldDirection(d);
      const hit = g.weapons.trace(cam.position.clone().addScaledVector(d, 2.5), d, 25, this);
      if (hit && hit.kind === 'ped') {
        g.peds.aimedAt(hit.target);
        g.events.emit('aimPed', hit.target);
      }
      this.aimTarget = hit;
    } else this.aimTarget = null;
    // Interactions.
    const v = this.nearestVehicle();
    const tr = this.nearTraboule();
    if (input.hit('KeyF') || input.hit('Enter')) {
      if (v) this.enterVehicle(v);
    } else if (input.hit('KeyE') && tr) {
      this.useTraboule(tr.t, tr.fromA);
    }
    if (v && !g.hud.helpLocked) g.hud.help(v.driver && v.driver !== 'player' ? 'F : sortir le conducteur et voler la voiture' : `F : monter dans ${v.def.name.toLowerCase().startsWith('f') ? 'le' : 'la'} ${v.def.name.toLowerCase()}`);
    else if (tr) g.hud.help('E : prendre la traboule');
    // Animation.
    let anim = this.speed > 4.5 ? 'run' : 'walk';
    const w = WEAPONS[this.weapon];
    this.h.gun.visible = !w.melee;
    if (aiming) anim = 'aim';
    if (this.punchT > 0) anim = 'punch';
    this.h.animate(dt, this.speed, anim, this.punchT > 0 ? 1 - this.punchT / 0.3 : 0);
    this.h.root.position.set(this.x, this.y, this.z);
    this.h.root.rotation.y = this.heading;
  }
}

// Third-person camera: orbit on foot, chase behind cars, collision with facades.
export class CameraRig {
  constructor(game, camera) {
    this.game = game;
    this.camera = camera;
    this.yaw = Math.PI;
    this.pitch = 0.18;
    this.dist = 4.4;
    this.lookTimer = 0;
    this.shake = 0;
    this.fov = 62;
    this.pos = new THREE.Vector3();
  }

  update(dt) {
    const g = this.game, pl = g.player, input = g.input;
    const look = input.look();
    if (Math.abs(look.dx) + Math.abs(look.dy) > 0.0001) this.lookTimer = 1.6;
    this.lookTimer -= dt;
    this.yaw -= look.dx;
    this.pitch = clamp(this.pitch + look.dy, -0.6, 1.2);
    let target, d, height, side = 0, fov = 62;
    if (pl.vehicle) {
      const v = pl.vehicle;
      const sp = v.speed;
      if (this.lookTimer <= 0 && sp > 2) {
        const back = Math.abs(v.forwardSpeed) > 1 && v.forwardSpeed < 0 ? v.heading : v.heading + Math.PI;
        this.yaw = dampAngle(this.yaw, back, 3, dt);
        this.pitch = lerp(this.pitch, 0.22, dt * 2);
      }
      d = 6.5 + v.def.len * 0.6 + Math.min(sp, 40) * 0.05;
      height = 1.3 + v.def.hgt * 0.5;
      target = new THREE.Vector3(v.x, height, v.z);
      fov = 64 + Math.min(sp, 50) * 0.25;
    } else {
      const aiming = input.aiming() && pl.weapon !== 'fists' && !pl.dead;
      d = aiming ? 2.1 : 4.2;
      side = aiming ? 0.75 : 0.35;
      height = 1.55;
      target = new THREE.Vector3(pl.x, pl.y + height, pl.z);
      fov = aiming ? 50 : 62;
      if (pl.dead) { d = 6; this.yaw += dt * 0.3; }
    }
    this.dist = lerp(this.dist, d, Math.min(1, dt * 6));
    this.fov = lerp(this.fov, fov, Math.min(1, dt * 5));
    const cp = Math.cos(this.pitch), sp = Math.sin(this.pitch);
    const dirX = Math.sin(this.yaw) * cp, dirZ = Math.cos(this.yaw) * cp;
    // Shoulder offset (to the right of the view direction).
    const rx = -Math.cos(this.yaw), rz = Math.sin(this.yaw);
    const ox = target.x - rx * side, oz = target.z - rz * side;
    // Pull in when a facade is between the player and the camera.
    let dd = this.dist;
    const hor = Math.hypot(dirX, dirZ) || 1;
    const free = g.grid.rayDistance(ox, oz, dirX / hor, dirZ / hor, dd * cp + 0.6, 0.25, (t) => t === T.BUILD);
    dd = Math.min(dd, Math.max(0.6, (free - 0.35) / Math.max(cp, 0.2)));
    const cam = this.camera;
    cam.position.set(ox + dirX * dd, Math.max(0.35, target.y + sp * dd), oz + dirZ * dd);
    if (this.shake > 0) {
      this.shake = Math.max(0, this.shake - dt * 1.5);
      cam.position.x += (Math.random() - 0.5) * this.shake;
      cam.position.y += (Math.random() - 0.5) * this.shake;
    }
    cam.lookAt(ox, target.y + (pl.vehicle ? 0.4 : 0.1), oz);
    if (Math.abs(cam.fov - this.fov) > 0.05) { cam.fov = this.fov; cam.updateProjectionMatrix(); }
    void wrapAngle;
  }
}
