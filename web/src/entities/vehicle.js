// Cars: low-poly models and an arcade driving model (mass transfer is faked
// with grip and steering that fade with speed; handbrake slides the rear).

import * as THREE from 'three';
import { clamp, rand, wrapAngle } from '../util.js';

export const VEHICLE_TYPES = {
  citadine: { name: 'Citadine', len: 3.7, wid: 1.7, hgt: 1.45, maxSpeed: 38, accel: 9, grip: 9, steer: 0.6, health: 700, cabin: [0.55, 0.15] },
  berline: { name: 'Berline', len: 4.6, wid: 1.85, hgt: 1.45, maxSpeed: 46, accel: 10, grip: 8.5, steer: 0.55, health: 900, cabin: [0.5, 0.05] },
  sportive: { name: 'Sportive', len: 4.4, wid: 1.9, hgt: 1.2, maxSpeed: 62, accel: 15, grip: 10, steer: 0.55, health: 750, cabin: [0.42, -0.05] },
  utilitaire: { name: 'Utilitaire', len: 5.1, wid: 2.0, hgt: 2.3, maxSpeed: 33, accel: 7, grip: 7, steer: 0.5, health: 1300, cabin: [0.8, 0.0], van: true },
  police: { name: 'Police', len: 4.6, wid: 1.85, hgt: 1.5, maxSpeed: 52, accel: 12.5, grip: 9.5, steer: 0.58, health: 1200, cabin: [0.5, 0.05], police: true },
  taxi: { name: 'Taxi', len: 4.6, wid: 1.85, hgt: 1.5, maxSpeed: 44, accel: 10, grip: 8.5, steer: 0.55, health: 900, cabin: [0.5, 0.05], taxi: true },
  fourgon: { name: 'Fourgon blindé', len: 5.6, wid: 2.2, hgt: 2.6, maxSpeed: 31, accel: 6.5, grip: 7, steer: 0.45, health: 3200, cabin: [0.85, 0.0], van: true, armored: true },
  scooter: { name: 'Scooter', len: 1.9, wid: 0.7, hgt: 1.2, maxSpeed: 30, accel: 9, grip: 11, steer: 0.7, health: 300, bike: true },
};

const PAINTS = [0xb71c1c, 0x1565c0, 0xeeeeee, 0x212121, 0x9e9e9e, 0x2e7d32, 0xf9a825, 0x6a1b9a, 0x37474f, 0xd84315, 0x0097a7, 0x5d4037, 0xc0c0c0, 0x880e4f];

const sharedGeo = {
  wheel: (() => { const g = new THREE.CylinderGeometry(0.34, 0.34, 0.24, 12); g.rotateZ(Math.PI / 2); return g; })(),
  smallWheel: (() => { const g = new THREE.CylinderGeometry(0.24, 0.24, 0.12, 10); g.rotateZ(Math.PI / 2); return g; })(),
};
const sharedMat = {
  tyre: new THREE.MeshStandardMaterial({ color: 0x151515, roughness: 0.9 }),
  glass: new THREE.MeshStandardMaterial({ color: 0x1b2633, roughness: 0.1, metalness: 0.6 }),
  dark: new THREE.MeshStandardMaterial({ color: 0x1a1a1a, roughness: 0.7 }),
  chrome: new THREE.MeshStandardMaterial({ color: 0xcccccc, roughness: 0.3, metalness: 0.8 }),
  burnt: new THREE.MeshStandardMaterial({ color: 0x1c1a18, roughness: 1 }),
  blue: new THREE.MeshStandardMaterial({ color: 0x1a3a8a, roughness: 0.5 }),
  red: new THREE.MeshStandardMaterial({ color: 0xb01020, roughness: 0.5 }),
  pink: new THREE.MeshStandardMaterial({ color: 0xe0559a, roughness: 0.5 }),
};

let nextId = 1;

export class Vehicle {
  constructor(type, x, z, heading, paint = null) {
    const def = VEHICLE_TYPES[type];
    this.id = nextId++;
    this.type = type;
    this.def = def;
    this.x = x; this.z = z; this.heading = heading;
    this.vx = 0; this.vz = 0;
    this.steerAngle = 0;
    this.health = def.health;
    this.maxHealth = def.health;
    this.driver = null; // 'player' | ped-like object | null
    this.ai = null;
    this.dead = false;
    this.burnTimer = 0;
    this.locked = false;
    this.siren = false;
    this.throttle = 0; this.brakeLight = 0;
    this.halfL = def.len / 2; this.halfW = def.wid / 2;
    this.radius = Math.max(def.len, def.wid) / 2;
    this.paint = paint ?? (def.police ? 0xf4f4f4 : def.taxi ? 0xf2f2f2 : def.armored ? 0x8a8f94 : rand.pick(PAINTS));
    this.buildModel();
    this.sync();
  }

  buildModel() {
    const d = this.def;
    const g = new THREE.Group();
    this.bodyMat = new THREE.MeshStandardMaterial({ color: this.paint, roughness: 0.35, metalness: 0.4 });
    const L = d.len, W = d.wid, H = d.hgt;
    const parts = [];
    if (d.bike) {
      const frame = new THREE.Mesh(new THREE.BoxGeometry(0.5, 0.5, 1.4), this.bodyMat);
      frame.position.y = 0.55;
      const seat = new THREE.Mesh(new THREE.BoxGeometry(0.4, 0.15, 0.7), sharedMat.dark);
      seat.position.set(0, 0.88, -0.2);
      const bar = new THREE.Mesh(new THREE.BoxGeometry(0.7, 0.08, 0.08), sharedMat.chrome);
      bar.position.set(0, 1.1, 0.6);
      const col = new THREE.Mesh(new THREE.BoxGeometry(0.12, 0.7, 0.12), this.bodyMat);
      col.position.set(0, 0.75, 0.62);
      parts.push(frame, seat, bar, col);
      this.wheels = [];
      for (const z of [0.7, -0.7]) {
        const w = new THREE.Mesh(sharedGeo.smallWheel, sharedMat.tyre);
        w.position.set(0, 0.24, z);
        g.add(w); this.wheels.push(w);
      }
      this.frontWheels = [this.wheels[0]];
    } else {
      const lowerH = H * 0.45;
      const body = new THREE.Mesh(new THREE.BoxGeometry(W, lowerH, L), this.bodyMat);
      body.position.y = 0.3 + lowerH / 2;
      parts.push(body);
      const [cabLen, cabOff] = d.cabin;
      const cabH = H - lowerH - 0.3;
      const cab = new THREE.Mesh(new THREE.BoxGeometry(W * 0.92, cabH, L * cabLen), d.van ? this.bodyMat : sharedMat.glass);
      cab.position.set(0, 0.3 + lowerH + cabH / 2, -cabOff * L);
      parts.push(cab);
      if (!d.van) {
        const roof = new THREE.Mesh(new THREE.BoxGeometry(W * 0.86, 0.06, L * cabLen * 0.8), this.bodyMat);
        roof.position.set(0, 0.3 + lowerH + cabH, -cabOff * L);
        parts.push(roof);
      } else {
        // Windscreen and side windows on the van's cab.
        const ws = new THREE.Mesh(new THREE.BoxGeometry(W * 0.86, cabH * 0.45, 0.05), sharedMat.glass);
        ws.position.set(0, 0.3 + lowerH + cabH * 0.68, -cabOff * L + L * cabLen / 2 + 0.01);
        parts.push(ws);
        const sw = new THREE.Mesh(new THREE.BoxGeometry(W * 0.94, cabH * 0.4, 0.9), sharedMat.glass);
        sw.position.set(0, 0.3 + lowerH + cabH * 0.68, -cabOff * L + L * cabLen / 2 - 0.6);
        parts.push(sw);
      }
      // Bumpers.
      for (const s of [1, -1]) {
        const b = new THREE.Mesh(new THREE.BoxGeometry(W * 1.02, 0.18, 0.12), sharedMat.dark);
        b.position.set(0, 0.38, s * (L / 2 + 0.02));
        parts.push(b);
      }
      // Lights.
      this.headMat = new THREE.MeshStandardMaterial({ color: 0xfffbe8, emissive: 0xfff1c0, emissiveIntensity: 0.2 });
      this.tailMat = new THREE.MeshStandardMaterial({ color: 0x550000, emissive: 0xff1010, emissiveIntensity: 0.3 });
      for (const s of [1, -1]) {
        const h = new THREE.Mesh(new THREE.BoxGeometry(0.35, 0.14, 0.05), this.headMat);
        h.position.set(s * (W / 2 - 0.3), 0.3 + lowerH * 0.7, L / 2 + 0.01);
        const t = new THREE.Mesh(new THREE.BoxGeometry(0.3, 0.14, 0.05), this.tailMat);
        t.position.set(s * (W / 2 - 0.25), 0.3 + lowerH * 0.7, -L / 2 - 0.01);
        parts.push(h, t);
      }
      if (d.police) {
        for (const s of [1, -1]) {
          const stripe = new THREE.Mesh(new THREE.BoxGeometry(0.02, 0.16, L * 0.9), sharedMat.blue);
          stripe.position.set(s * (W / 2 + 0.01), 0.3 + lowerH * 0.55, 0);
          const stripe2 = new THREE.Mesh(new THREE.BoxGeometry(0.02, 0.05, L * 0.9), sharedMat.red);
          stripe2.position.set(s * (W / 2 + 0.01), 0.3 + lowerH * 0.3, 0);
          parts.push(stripe, stripe2);
        }
        this.lightA = new THREE.MeshStandardMaterial({ color: 0x0a2aff, emissive: 0x2050ff, emissiveIntensity: 0.2 });
        this.lightB = new THREE.MeshStandardMaterial({ color: 0x0a2aff, emissive: 0x2050ff, emissiveIntensity: 0.2 });
        const y = 0.3 + lowerH + cabH + 0.12;
        const a = new THREE.Mesh(new THREE.BoxGeometry(0.55, 0.14, 0.25), this.lightA);
        a.position.set(-0.32, y, -cabOff * L);
        const b = new THREE.Mesh(new THREE.BoxGeometry(0.55, 0.14, 0.25), this.lightB);
        b.position.set(0.32, y, -cabOff * L);
        parts.push(a, b);
      }
      if (d.taxi) {
        const sign = new THREE.Mesh(new THREE.BoxGeometry(0.6, 0.18, 0.2),
          new THREE.MeshStandardMaterial({ color: 0xffffff, emissive: 0x99ff99, emissiveIntensity: 0.6 }));
        sign.position.set(0, 0.3 + lowerH + cabH + 0.12, -cabOff * L);
        parts.push(sign);
      }
      if (d.armored) {
        const band = new THREE.Mesh(new THREE.BoxGeometry(W + 0.02, 0.35, L * 0.55), sharedMat.pink);
        band.position.set(0, 0.3 + lowerH + cabH * 0.5, -L * 0.2);
        parts.push(band);
      }
      this.wheels = [];
      this.frontWheels = [];
      const wx = W / 2 - 0.12, wz = L / 2 - 0.75;
      for (const [x, z] of [[wx, wz], [-wx, wz], [wx, -wz], [-wx, -wz]]) {
        const wg = new THREE.Group();
        wg.position.set(x, 0.34, z);
        const w = new THREE.Mesh(sharedGeo.wheel, sharedMat.tyre);
        wg.add(w);
        g.add(wg);
        this.wheels.push(w);
        if (z > 0) this.frontWheels.push(wg);
      }
    }
    for (const p of parts) g.add(p);
    g.traverse((o) => { if (o.isMesh) { o.castShadow = true; o.receiveShadow = false; } });
    this.parts = parts;
    this.mesh = g;
    // Smoke/fire puffs.
    this.smokeT = 0;
  }

  get speed() { return Math.hypot(this.vx, this.vz); }
  get fwdX() { return Math.sin(this.heading); }
  get fwdZ() { return Math.cos(this.heading); }
  get forwardSpeed() { return this.vx * this.fwdX + this.vz * this.fwdZ; }

  sync() {
    this.mesh.position.set(this.x, 0, this.z);
    this.mesh.rotation.y = this.heading;
  }

  // Corner and edge sample points (world space).
  samplePoints(x = this.x, z = this.z, h = this.heading) {
    const fx = Math.sin(h), fz = Math.cos(h), rx = Math.cos(h), rz = -Math.sin(h);
    const L = this.halfL, W = this.halfW;
    const pts = [];
    for (const [a, b] of [[L, W], [L, -W], [-L, W], [-L, -W], [L, 0], [-L, 0], [0, W], [0, -W], [L * 0.5, W], [L * 0.5, -W], [-L * 0.5, W], [-L * 0.5, -W]]) {
      pts.push({ x: x + fx * a + rx * b, z: z + fz * a + rz * b });
    }
    return pts;
  }

  // controls: {throttle -1..1, steer -1..1, handbrake bool}
  physics(dt, controls, grid) {
    const d = this.def;
    if (this.dead) controls = { throttle: 0, steer: 0, handbrake: true };
    const fx = Math.sin(this.heading), fz = Math.cos(this.heading);
    const sx = Math.cos(this.heading), sz = -Math.sin(this.heading);
    let vF = this.vx * fx + this.vz * fz;
    let vS = this.vx * sx + this.vz * sz;
    const max = d.maxSpeed * (this.health < this.maxHealth * 0.25 ? 0.6 : 1);
    const thr = controls.throttle;
    this.brakeLight = 0;
    if (thr > 0) {
      if (vF < -0.5) { vF += 22 * dt; this.brakeLight = 1; }
      else vF += thr * d.accel * Math.max(0, 1 - vF / max) * dt * 1.4;
    } else if (thr < 0) {
      if (vF > 0.5) { vF -= 24 * dt * -thr; this.brakeLight = 1; }
      else vF = Math.max(-max * 0.3, vF + thr * d.accel * 0.7 * dt);
    } else {
      vF -= vF * 0.35 * dt + Math.sign(vF) * Math.min(Math.abs(vF), 1.2 * dt);
    }
    if (controls.handbrake) {
      vF -= Math.sign(vF) * Math.min(Math.abs(vF), 7 * dt);
      this.brakeLight = 1;
    }
    vF -= vF * Math.abs(vF) * 0.0009 * dt; // air drag
    const speedK = Math.min(1, Math.abs(vF) / max);
    const grip = controls.handbrake ? 1.4 : d.grip * (1 - 0.35 * speedK);
    vS *= Math.exp(-grip * dt);
    const targetSteer = controls.steer * d.steer * (1 - 0.6 * speedK);
    this.steerAngle += (targetSteer - this.steerAngle) * Math.min(1, dt * 8);
    const wheelBase = d.len * 0.62;
    let yaw = (vF / wheelBase) * Math.tan(this.steerAngle);
    if (controls.handbrake) yaw *= 1.45;
    this.heading = wrapAngle(this.heading + yaw * dt);
    const nfx = Math.sin(this.heading), nfz = Math.cos(this.heading);
    const nsx = Math.cos(this.heading), nsz = -Math.sin(this.heading);
    this.vx = nfx * vF + nsx * vS;
    this.vz = nfz * vF + nsz * vS;
    // Move with wall collisions.
    const ox = this.x, oz = this.z, oh = this.heading;
    this.x += this.vx * dt;
    this.z += this.vz * dt;
    let impact = 0;
    for (let iter = 0; iter < 4; iter++) {
      let hit = null;
      for (const p of this.samplePoints()) if (grid.blocked(p.x, p.z)) { hit = p; break; }
      if (!hit) break;
      const n = grid.wallNormal(hit.x, hit.z) || { x: -this.vx / (this.speed || 1), z: -this.vz / (this.speed || 1) };
      const vn = this.vx * n.x + this.vz * n.z;
      if (vn < 0) {
        impact = Math.max(impact, -vn);
        this.vx -= 1.35 * vn * n.x;
        this.vz -= 1.35 * vn * n.z;
        this.vx *= 0.85; this.vz *= 0.85;
      }
      this.x += n.x * 0.12;
      this.z += n.z * 0.12;
    }
    // Still stuck: revert.
    if (this.samplePoints().some((p) => grid.blocked(p.x, p.z))) {
      this.x = ox; this.z = oz; this.heading = oh;
      if (this.samplePoints().some((p) => grid.blocked(p.x, p.z))) {
        const n = grid.wallNormal(this.x, this.z);
        if (n) { this.x += n.x * 0.3; this.z += n.z * 0.3; }
      }
      this.vx *= -0.2; this.vz *= -0.2;
    }
    this.throttle = thr;
    return impact;
  }

  damage(amount) {
    if (this.dead) return;
    const k = this.def.armored ? 0.6 : 1;
    this.health -= amount * k;
    if (this.health <= 0) {
      this.health = 0;
      this.burnTimer = this.burnTimer || 4.5;
    }
  }

  explode() {
    this.dead = true;
    this.health = 0;
    for (const p of this.parts) p.material = sharedMat.burnt;
    this.bodyMat = sharedMat.burnt;
  }

  // Visual update: wheels, lights.
  visuals(dt, night, time) {
    const vF = this.forwardSpeed;
    for (const w of this.wheels) w.rotation.x += (vF / 0.34) * dt;
    for (const fw of this.frontWheels) fw.rotation.y = this.steerAngle;
    if (this.headMat) {
      this.headMat.emissiveIntensity = this.dead ? 0 : 0.2 + night * 2.5;
      this.tailMat.emissiveIntensity = this.dead ? 0 : (this.brakeLight ? 2.5 : 0.3 + night * 0.9);
    }
    if (this.lightA) {
      const on = this.siren && !this.dead;
      const ph = Math.floor(time * 6) % 2;
      this.lightA.emissiveIntensity = on ? (ph ? 4 : 0.1) : 0.15;
      this.lightB.emissiveIntensity = on ? (ph ? 0.1 : 4) : 0.15;
      this.lightA.emissive.setHex(0x2050ff);
      this.lightB.emissive.setHex(on ? 0xff2030 : 0x2050ff);
    }
    this.sync();
    // Body roll from lateral speed.
    const vS = this.vx * Math.cos(this.heading) - this.vz * Math.sin(this.heading);
    this.mesh.rotation.z = clamp(-vS * 0.012, -0.08, 0.08);
  }

  // Seat positions for exiting: left side door.
  doorPoint(side = 1) {
    const rx = Math.cos(this.heading), rz = -Math.sin(this.heading);
    const off = this.halfW + 0.8;
    return { x: this.x + rx * off * side, z: this.z + rz * off * side };
  }

  // Ray (origin o, dir d normalized, both THREE.Vector3) vs oriented box. Returns distance or -1.
  rayHit(o, d, maxDist) {
    const c = Math.cos(-this.heading), s = Math.sin(-this.heading);
    const lx = o.x - this.x, lz = o.z - this.z;
    const ox = lx * c + lz * s, oz = -lx * s + lz * c, oy = o.y;
    const dx = d.x * c + d.z * s, dz = -d.x * s + d.z * c, dy = d.y;
    const min = [-this.halfW, 0.2, -this.halfL], max = [this.halfW, this.def.hgt, this.halfL];
    const O = [ox, oy, oz], D = [dx, dy, dz];
    let t0 = 0, t1 = maxDist;
    for (let k = 0; k < 3; k++) {
      if (Math.abs(D[k]) < 1e-8) { if (O[k] < min[k] || O[k] > max[k]) return -1; continue; }
      let a = (min[k] - O[k]) / D[k], b = (max[k] - O[k]) / D[k];
      if (a > b) [a, b] = [b, a];
      t0 = Math.max(t0, a); t1 = Math.min(t1, b);
      if (t0 > t1) return -1;
    }
    return t0;
  }

  // Is point (x,z) inside the footprint grown by r?
  contains(x, z, r = 0) {
    const c = Math.cos(-this.heading), s = Math.sin(-this.heading);
    const lx = x - this.x, lz = z - this.z;
    const px = lx * c + lz * s, pz = -lx * s + lz * c;
    return Math.abs(px) <= this.halfW + r && Math.abs(pz) <= this.halfL + r;
  }
}

// Pairwise car collisions using two circles per car.
export function collideVehicles(list, onImpact) {
  const circ = (v) => {
    const fx = Math.sin(v.heading), fz = Math.cos(v.heading);
    const r = v.halfW * 1.05;
    const off = Math.max(0, v.halfL - r);
    return [{ x: v.x + fx * off, z: v.z + fz * off, r }, { x: v.x - fx * off, z: v.z - fz * off, r }];
  };
  for (let i = 0; i < list.length; i++) {
    const a = list[i];
    for (let j = i + 1; j < list.length; j++) {
      const b = list[j];
      const dx0 = b.x - a.x, dz0 = b.z - a.z;
      const rr = a.radius + b.radius;
      if (dx0 * dx0 + dz0 * dz0 > rr * rr) continue;
      const ca = circ(a), cb = circ(b);
      for (const p of ca) {
        for (const q of cb) {
          const dx = q.x - p.x, dz = q.z - p.z;
          const d2 = dx * dx + dz * dz, R = p.r + q.r;
          if (d2 >= R * R || d2 < 1e-8) continue;
          const dd = Math.sqrt(d2), nx = dx / dd, nz = dz / dd;
          const pen = R - dd;
          const ma = a.def.armored ? 4 : a.def.van ? 1.6 : 1, mb = b.def.armored ? 4 : b.def.van ? 1.6 : 1;
          const ta = mb / (ma + mb), tb = ma / (ma + mb);
          a.x -= nx * pen * ta; a.z -= nz * pen * ta;
          b.x += nx * pen * tb; b.z += nz * pen * tb;
          const rv = (b.vx - a.vx) * nx + (b.vz - a.vz) * nz;
          if (rv < 0) {
            const jimp = -(1.3) * rv / (1 / ma + 1 / mb);
            a.vx -= (jimp / ma) * nx; a.vz -= (jimp / ma) * nz;
            b.vx += (jimp / mb) * nx; b.vz += (jimp / mb) * nz;
            onImpact(a, b, -rv);
          }
        }
      }
    }
  }
}
