// Traffic: AI drivers that keep to the right of the carriageway, pick a
// branch at junctions, brake for cars and people, and panic under fire.
// Parked cars line the wider streets, ready to be stolen.

import { Vehicle } from '../entities/vehicle.js';
import { Ped } from './peds.js';
import { T } from '../world/grid.js';
import { rand, wrapAngle, clamp, dist } from '../util.js';

const TRAFFIC_TYPES = ['citadine', 'citadine', 'berline', 'berline', 'berline', 'utilitaire', 'taxi', 'sportive', 'scooter'];

// Reactive road driver shared by traffic, police and mission vehicles.
export class Driver {
  constructor(vehicle, opts = {}) {
    this.v = vehicle;
    this.cruise = opts.cruise ?? rand.range(9, 12.5);
    this.mode = opts.mode || 'cruise'; // cruise | panic | flee | follow | stop
    this.turnBias = 0;
    this.biasT = 0;
    this.stuckT = 0;
    this.reverseT = 0;
    this.hornCd = 0;
    this.path = null;
    this.pathT = 0;
    this.target = null;
  }

  // Desired heading from feelers on the road mask.
  roadHeading(grid, roadOnly = true) {
    const v = this.v;
    const onRoad = (t) => (roadOnly ? t !== T.ROAD : grid.constructor.blocks(t));
    let best = v.heading, bestScore = -Infinity;
    const right = this.mode === 'cruise' || this.mode === 'follow';
    for (let k = -6; k <= 6; k++) {
      const off = k * 0.2;
      const a = v.heading + off;
      const dx = Math.sin(a), dz = Math.cos(a);
      const d = grid.rayDistance(v.x, v.z, dx, dz, 28, 1, onRoad);
      let score = d - Math.abs(off) * 6 + this.turnBias * off * 10;
      if (d >= 27 && Math.abs(off) > 0.9) score += this.turnBias !== 0 && Math.sign(off) === Math.sign(this.turnBias) ? 12 : 0;
      if (score > bestScore) { bestScore = score; best = a; }
    }
    if (right) {
      // Keep about 2.4 m from the right edge.
      const rx = -Math.cos(v.heading), rz = Math.sin(v.heading);
      const rd = grid.rayDistance(v.x, v.z, rx, rz, 10, 0.5, onRoad);
      const ld = grid.rayDistance(v.x, v.z, -rx, -rz, 10, 0.5, onRoad);
      if (rd + ld > 5) best -= clamp((rd - 2.4) * 0.07, -0.2, 0.2);
    }
    return best;
  }

  // Follow a path of waypoints (police pursuit, missions).
  pathHeading() {
    const v = this.v;
    if (!this.path || this.path.length === 0) return null;
    while (this.path.length > 1 && dist(v.x, v.z, this.path[0].x, this.path[0].z) < 6) this.path.shift();
    const p = this.path[0];
    return Math.atan2(p.x - v.x, p.z - v.z);
  }

  drive(dt, game) {
    const v = this.v, grid = game.grid;
    if (v.dead) { v.physics(dt, { throttle: 0, steer: 0, handbrake: true }, grid); return; }
    this.biasT -= dt;
    if (this.biasT <= 0) { this.biasT = rand.range(2, 6); this.turnBias = rand.pick([-1, 0, 0, 1]); }
    this.hornCd -= dt;
    let desired;
    let target = this.cruise;
    if (this.mode === 'stop') { v.physics(dt, { throttle: 0, steer: 0, handbrake: true }, grid); return; }
    if (this.mode === 'follow' && this.target) {
      this.pathT -= dt;
      const tx = this.target.x, tz = this.target.z;
      const dT = dist(v.x, v.z, tx, tz);
      if (this.pathT <= 0) {
        this.pathT = dT > 60 ? 1.5 : 0.8;
        this.path = grid.findPath(v.x, v.z, tx, tz, 25000, 1.3);
      }
      if (dT < 30 && grid.clearPath(v.x, v.z, tx, tz, 1.1)) desired = Math.atan2(tx - v.x, tz - v.z);
      else desired = this.pathHeading() ?? this.roadHeading(grid, false);
      target = this.cruise;
      const turn = Math.abs(wrapAngle(desired - v.heading));
      if (turn > 0.8) target = Math.min(target, 9);
    } else {
      desired = this.roadHeading(grid, true);
      if (this.mode === 'panic' || this.mode === 'flee') target = this.cruise * 1.7;
      // Off the road (pushed onto a sidewalk): head back to the nearest road.
      if (grid.type(v.x, v.z) !== T.ROAD) {
        const r = grid.findNearest(v.x, v.z, (t) => t === T.ROAD, 12);
        if (r) desired = Math.atan2(r.x - v.x, r.z - v.z);
      }
      const turn = Math.abs(wrapAngle(desired - v.heading));
      target *= clamp(1.2 - turn * 1.3, 0.35, 1);
      // Obstacles ahead.
      const fx = Math.sin(v.heading), fz = Math.cos(v.heading);
      const free = grid.rayDistance(v.x, v.z, fx, fz, 20, 1);
      if (free < 10) target = Math.min(target, free * 0.6);
      if (this.mode !== 'panic' && this.mode !== 'flee') {
        let blockD = Infinity, blocker = null;
        const check = (x, z, w, obj) => {
          const dx = x - v.x, dz = z - v.z;
          const along = dx * fx + dz * fz;
          if (along < 0 || along > 16) return;
          const lat = Math.abs(dx * fz - dz * fx);
          if (lat < w && along < blockD) { blockD = along; blocker = obj; }
        };
        for (const o of game.vehicles) if (o !== v) check(o.x, o.z, 1.9, o);
        for (const p of game.peds.list) if (!p.dead && !p.inVehicle) check(p.x, p.z, 1.4, p);
        if (!game.player.vehicle) check(game.player.x, game.player.z, 1.4, game.player);
        if (blocker) {
          target = Math.min(target, Math.max(0, (blockD - v.halfL - 3) * 0.8));
          if ((blocker === game.player || blocker === game.player.vehicle) && blockD < 8 && this.hornCd <= 0 && v.speed < 1) {
            this.hornCd = rand.range(3, 6);
            if (dist(v.x, v.z, game.player.x, game.player.z) < 30) game.audio.horn();
          }
        }
      }
    }
    let steer = clamp(wrapAngle(desired - v.heading) * 2.2, -1, 1);
    let throttle;
    const vF = v.forwardSpeed;
    if (vF < target - 0.5) throttle = 1;
    else if (vF > target + 1.5) throttle = -0.7;
    else throttle = 0.15;
    // Stuck: reverse a moment.
    if (this.reverseT > 0) {
      this.reverseT -= dt;
      throttle = -1; steer = -steer;
    } else if (target > 2) {
      // No progress for a while (wall, tree, parked car): back up.
      this.stuckT += dt;
      if (!this.anchor) this.anchor = { x: v.x, z: v.z };
      if (this.stuckT > 1.8) {
        const moved = dist(v.x, v.z, this.anchor.x, this.anchor.z);
        this.anchor = { x: v.x, z: v.z };
        this.stuckT = 0;
        if (moved < 2) { this.reverseT = 1.3; this.turnBias = rand.pick([-1, 1]); this.stuckCount = (this.stuckCount || 0) + 1; }
        else this.stuckCount = 0;
      }
    } else { this.stuckT = 0; this.anchor = null; }
    v.physics(dt, { throttle, steer, handbrake: false }, grid);
  }
}

export class TrafficSystem {
  constructor(game) {
    this.game = game;
    this.target = 22;
    this.spawnCd = 0;
    this.parkedDone = false;
  }

  // Heading along the road at a point: the direction with the longest run.
  roadDir(x, z) {
    const g = this.game.grid;
    let best = 0, bd = -1;
    for (let k = 0; k < 16; k++) {
      const a = (k / 16) * Math.PI * 2;
      const d = g.rayDistance(x, z, Math.sin(a), Math.cos(a), 40, 1, (t) => t !== T.ROAD);
      if (d > bd) { bd = d; best = a; }
    }
    return best;
  }

  spawnTraffic(x, z, type = null) {
    const g = this.game;
    const h = this.roadDir(x, z);
    const v = new Vehicle(type || rand.pick(TRAFFIC_TYPES), x, z, h);
    // Shift onto the right half of the road.
    const rx = -Math.cos(h), rz = Math.sin(h);
    const rd = g.grid.rayDistance(x, z, rx, rz, 8, 0.5, (t) => t !== T.ROAD);
    if (rd > 2.5) { v.x += rx * (rd - 2.2); v.z += rz * (rd - 2.2); }
    if (v.samplePoints().some((p) => g.grid.blocked(p.x, p.z))) return null;
    if (g.vehicles.some((o) => dist(o.x, o.z, v.x, v.z) < 7)) return null;
    v.ai = new Driver(v);
    const driver = new Ped(g, v.x, v.z);
    driver.inVehicle = v;
    v.driver = driver;
    g.peds.list.push(driver);
    g.addVehicle(v);
    const sp = v.def.maxSpeed > 0 ? v.ai.cruise * 0.8 : 0;
    v.vx = Math.sin(h) * sp; v.vz = Math.cos(h) * sp;
    v.sync();
    return v;
  }

  // Parked cars along wide streets, across the whole map (once).
  placeParked() {
    const g = this.game, grid = g.grid;
    const rngSpots = [];
    for (let iz = 4; iz < grid.h - 4; iz += 3) {
      for (let ix = 4; ix < grid.w - 4; ix += 3) {
        const i = iz * grid.w + ix;
        if (grid.cells[i] !== T.ROAD) continue;
        const de = grid.distEdge[i];
        if (de < 1.2 || de > 1.8) continue;
        rngSpots.push({ x: grid.cx(ix), z: grid.cz(iz) });
      }
    }
    let placed = 0;
    for (let k = 0; k < 400 && placed < 34; k++) {
      const s = rand.pick(rngSpots);
      // Road must be wide here (centre at least 5 m from the edge nearby).
      let wide = false;
      for (let r = 2; r < 9; r++) if (grid.distE(s.x + r, s.z) > 5 || grid.distE(s.x - r, s.z) > 5 || grid.distE(s.x, s.z + r) > 5 || grid.distE(s.x, s.z - r) > 5) wide = true;
      if (!wide) continue;
      if (g.vehicles.some((o) => dist(o.x, o.z, s.x, s.z) < 9)) continue;
      // Along the curb: perpendicular to the edge-distance gradient.
      const gx = grid.distE(s.x + 1, s.z) - grid.distE(s.x - 1, s.z);
      const gz = grid.distE(s.x, s.z + 1) - grid.distE(s.x, s.z - 1);
      const h = Math.atan2(gz, -gx) + (rand() < 0.5 ? 0 : Math.PI);
      const v = new Vehicle(rand.pick(['citadine', 'citadine', 'berline', 'utilitaire', 'sportive', 'scooter']), s.x, s.z, h);
      if (v.samplePoints().some((p) => grid.blocked(p.x, p.z))) continue;
      v.parked = true;
      g.addVehicle(v);
      placed++;
    }
  }

  update(dt) {
    const g = this.game, pl = g.player;
    if (!this.parkedDone) { this.placeParked(); this.parkedDone = true; }
    const px = pl.x, pz = pl.z;
    const night = g.sky.night;
    const target = Math.round(this.target * (1 - night * 0.4));
    let count = 0;
    for (const v of g.vehicles) if (v.ai && !v.police && !v.mission && v.ai.mode !== 'stop') count++;
    this.spawnCd -= dt;
    if (count < target && this.spawnCd <= 0) {
      this.spawnCd = 0.25;
      for (let tries = 0; tries < 10; tries++) {
        const a = rand() * Math.PI * 2, r = rand.range(60, 150);
        const x = px + Math.sin(a) * r, z = pz + Math.cos(a) * r;
        if (g.grid.type(x, z) !== T.ROAD || g.grid.distE(x, z) < 2) continue;
        const cam = g.camera.position;
        if (r < 100 && g.grid.lineOfSight(cam.x, cam.z, x, z)) continue;
        if (this.spawnTraffic(x, z)) break;
      }
    }
    for (const v of g.vehicles) {
      if (!v.ai || v.police || v.isPolice) continue;
      if (v.driver && v.driver.dead) { v.ai.mode = 'stop'; }
      if (!v.driver) continue;
      const d = dist(v.x, v.z, px, pz);
      if (d > 170 && !v.mission) { g.removeVehicle(v); continue; }
      if (d > 130 && !v.mission) continue;
      v.ai.drive(dt, g);
    }
    // Parked and abandoned cars far away disappear (not the player's last car).
    for (const v of g.vehicles) {
      if (v.driver || v.mission || v === g.player.lastVehicle || v.police) continue;
      if (!v.parked && dist(v.x, v.z, px, pz) > 200) g.removeVehicle(v);
    }
  }

  // Shots fired: nearby drivers floor it.
  panic(x, z, r = 40) {
    for (const v of this.game.vehicles) {
      if (!v.ai || v.police || !v.driver || v.mission) continue;
      if (dist(v.x, v.z, x, z) < r) { v.ai.mode = 'panic'; v.ai.cruise = Math.max(v.ai.cruise, 14); }
    }
  }
}
