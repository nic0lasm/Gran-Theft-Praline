// 2D occupancy grid of the city (1 m cells). Everything that moves on the
// ground (player, cars, pedestrians, police) collides against it, and the AI
// plans its routes on a coarse copy of it.

import { MinHeap } from '../util.js';

export const T = {
  OPEN: 0, // sidewalk, courtyard: walkable, drivable
  ROAD: 1, // carriageway
  BUILD: 2, // building footprint: blocks movement and sight
  WATER: 3, // river: blocks movement
  OBST: 4, // trunk, pedestal, parapet: blocks movement, not sight
  PLAZA: 5, // square (Bellecour): walkable, drivable
  GRASS: 6, // lawn
};

export class Grid {
  constructor(minX, minZ, width, height) {
    this.minX = minX;
    this.minZ = minZ;
    this.w = width;
    this.h = height;
    this.cells = new Uint8Array(width * height);
    this.distBuild = null; // distance (m) to the nearest building cell
    this.distEdge = null; // inside roads: distance to the road edge
  }

  inside(ix, iz) { return ix >= 0 && iz >= 0 && ix < this.w && iz < this.h; }
  ix(x) { return Math.floor(x - this.minX); }
  iz(z) { return Math.floor(z - this.minZ); }
  cx(ix) { return this.minX + ix + 0.5; }
  cz(iz) { return this.minZ + iz + 0.5; }

  type(x, z) {
    const ix = Math.floor(x - this.minX), iz = Math.floor(z - this.minZ);
    if (ix < 0 || iz < 0 || ix >= this.w || iz >= this.h) return T.BUILD;
    return this.cells[iz * this.w + ix];
  }
  typeI(ix, iz) {
    if (ix < 0 || iz < 0 || ix >= this.w || iz >= this.h) return T.BUILD;
    return this.cells[iz * this.w + ix];
  }
  set(ix, iz, t) { if (this.inside(ix, iz)) this.cells[iz * this.w + ix] = t; }

  static blocks(t) { return t === T.BUILD || t === T.WATER || t === T.OBST; }
  blocked(x, z) { return Grid.blocks(this.type(x, z)); }
  blockedI(ix, iz) { return Grid.blocks(this.typeI(ix, iz)); }
  isRoad(x, z) { return this.type(x, z) === T.ROAD; }
  walkable(x, z) { return !Grid.blocks(this.type(x, z)); }

  distB(x, z) {
    const ix = Math.floor(x - this.minX), iz = Math.floor(z - this.minZ);
    if (!this.inside(ix, iz)) return 0;
    return this.distBuild[iz * this.w + ix];
  }
  distE(x, z) {
    const ix = Math.floor(x - this.minX), iz = Math.floor(z - this.minZ);
    if (!this.inside(ix, iz)) return 0;
    return this.distEdge[iz * this.w + ix];
  }

  // Fill a triangle given in world XZ (rasterize by cell centres).
  fillTriangle(ax, az, bx, bz, cx, cz, t, onlyIf = null) {
    const x0 = Math.max(0, Math.floor(Math.min(ax, bx, cx) - this.minX));
    const x1 = Math.min(this.w - 1, Math.ceil(Math.max(ax, bx, cx) - this.minX));
    const z0 = Math.max(0, Math.floor(Math.min(az, bz, cz) - this.minZ));
    const z1 = Math.min(this.h - 1, Math.ceil(Math.max(az, bz, cz) - this.minZ));
    const d = (bz - cz) * (ax - cx) + (cx - bx) * (az - cz);
    if (Math.abs(d) < 1e-9) return;
    for (let iz = z0; iz <= z1; iz++) {
      const pz = this.minZ + iz + 0.5;
      for (let ix = x0; ix <= x1; ix++) {
        const px = this.minX + ix + 0.5;
        const l1 = ((bz - cz) * (px - cx) + (cx - bx) * (pz - cz)) / d;
        const l2 = ((cz - az) * (px - cx) + (ax - cx) * (pz - cz)) / d;
        const l3 = 1 - l1 - l2;
        if (l1 >= -1e-6 && l2 >= -1e-6 && l3 >= -1e-6) {
          const i = iz * this.w + ix;
          if (onlyIf === null || onlyIf(this.cells[i])) this.cells[i] = t;
        }
      }
    }
  }

  fillRect(x0, z0, x1, z1, t, onlyIf = null) {
    const a = Math.max(0, Math.floor(Math.min(x0, x1) - this.minX));
    const b = Math.min(this.w - 1, Math.ceil(Math.max(x0, x1) - this.minX) - 1);
    const c = Math.max(0, Math.floor(Math.min(z0, z1) - this.minZ));
    const e = Math.min(this.h - 1, Math.ceil(Math.max(z0, z1) - this.minZ) - 1);
    for (let iz = c; iz <= e; iz++) {
      for (let ix = a; ix <= b; ix++) {
        const i = iz * this.w + ix;
        if (onlyIf === null || onlyIf(this.cells[i])) this.cells[i] = t;
      }
    }
  }

  fillCircle(x, z, r, t, onlyIf = null) {
    const r2 = r * r;
    for (let iz = Math.floor(z - r - this.minZ); iz <= Math.ceil(z + r - this.minZ); iz++) {
      for (let ix = Math.floor(x - r - this.minX); ix <= Math.ceil(x + r - this.minX); ix++) {
        if (!this.inside(ix, iz)) continue;
        const dx = this.cx(ix) - x, dz = this.cz(iz) - z;
        if (dx * dx + dz * dz > r2) continue;
        const i = iz * this.w + ix;
        if (onlyIf === null || onlyIf(this.cells[i])) this.cells[i] = t;
      }
    }
  }

  // Two-pass chamfer distance transform from cells matching `isSource`.
  distanceField(isSource, maxD = 60) {
    const w = this.w, h = this.h;
    const d = new Float32Array(w * h);
    for (let i = 0; i < w * h; i++) d[i] = isSource(this.cells[i]) ? 0 : maxD;
    const D = 1.4142;
    for (let z = 0; z < h; z++) {
      for (let x = 0; x < w; x++) {
        const i = z * w + x;
        let v = d[i];
        if (x > 0) v = Math.min(v, d[i - 1] + 1);
        if (z > 0) {
          v = Math.min(v, d[i - w] + 1);
          if (x > 0) v = Math.min(v, d[i - w - 1] + D);
          if (x < w - 1) v = Math.min(v, d[i - w + 1] + D);
        }
        d[i] = v;
      }
    }
    for (let z = h - 1; z >= 0; z--) {
      for (let x = w - 1; x >= 0; x--) {
        const i = z * w + x;
        let v = d[i];
        if (x < w - 1) v = Math.min(v, d[i + 1] + 1);
        if (z < h - 1) {
          v = Math.min(v, d[i + w] + 1);
          if (x < w - 1) v = Math.min(v, d[i + w + 1] + D);
          if (x > 0) v = Math.min(v, d[i + w - 1] + D);
        }
        d[i] = v;
      }
    }
    return d;
  }

  computeFields() {
    this.distBuild = this.distanceField((t) => t === T.BUILD || t === T.WATER);
    this.distEdge = this.distanceField((t) => t !== T.ROAD, 30);
  }

  // Is the segment free of sight blockers (buildings)? Steps of 0.5 m.
  lineOfSight(x0, z0, x1, z1) {
    const dx = x1 - x0, dz = z1 - z0;
    const len = Math.sqrt(dx * dx + dz * dz);
    const n = Math.ceil(len / 0.5);
    for (let i = 1; i < n; i++) {
      const t = i / n;
      if (this.type(x0 + dx * t, z0 + dz * t) === T.BUILD) return false;
    }
    return true;
  }

  // Distance travelled from (x,z) along (dx,dz) until a blocked cell (max `max`).
  rayDistance(x, z, dx, dz, max, step = 0.75, test = null) {
    for (let s = step; s <= max; s += step) {
      const px = x + dx * s, pz = z + dz * s;
      const t = this.type(px, pz);
      if (test ? test(t) : Grid.blocks(t)) return s - step;
    }
    return max;
  }

  // Segment clear for a moving body of radius r (samples 3 parallel lines).
  clearPath(x0, z0, x1, z1, r) {
    const dx = x1 - x0, dz = z1 - z0;
    const len = Math.sqrt(dx * dx + dz * dz) || 1;
    const nx = -dz / len * r, nz = dx / len * r;
    const n = Math.ceil(len / 0.7);
    for (let i = 0; i <= n; i++) {
      const t = i / n;
      const px = x0 + dx * t, pz = z0 + dz * t;
      if (this.blocked(px, pz) || this.blocked(px + nx, pz + nz) || this.blocked(px - nx, pz - nz)) return false;
    }
    return true;
  }

  // Outward normal of nearby blocked cells, used to bounce off walls.
  wallNormal(x, z) {
    let nx = 0, nz = 0;
    for (let oz = -2; oz <= 2; oz++) {
      for (let ox = -2; ox <= 2; ox++) {
        if (this.blocked(x + ox, z + oz)) { nx -= ox; nz -= oz; }
      }
    }
    const l = Math.sqrt(nx * nx + nz * nz);
    if (l < 1e-6) return null;
    return { x: nx / l, z: nz / l };
  }

  // Find the nearest cell (spiral search) satisfying `ok(type, ix, iz)`.
  findNearest(x, z, ok, maxR = 60) {
    const cx = this.ix(x), cz = this.iz(z);
    for (let r = 0; r <= maxR; r++) {
      let best = null, bestD = Infinity;
      for (let oz = -r; oz <= r; oz++) {
        for (let ox = -r; ox <= r; ox++) {
          if (Math.max(Math.abs(ox), Math.abs(oz)) !== r) continue;
          const ix = cx + ox, iz = cz + oz;
          if (!this.inside(ix, iz)) continue;
          if (!ok(this.cells[iz * this.w + ix], ix, iz)) continue;
          const d = ox * ox + oz * oz;
          if (d < bestD) { bestD = d; best = { x: this.cx(ix), z: this.cz(iz) }; }
        }
      }
      if (best) return best;
    }
    return null;
  }

  // ---- Coarse navigation grid (2 m) for A* -------------------------------
  buildNav() {
    const S = 2;
    this.navS = S;
    this.nw = Math.floor(this.w / S);
    this.nh = Math.floor(this.h / S);
    const nw = this.nw, nh = this.nh;
    this.navCost = new Float32Array(nw * nh);
    for (let z = 0; z < nh; z++) {
      for (let x = 0; x < nw; x++) {
        let blocked = false, road = 0, minD = 99;
        for (let oz = 0; oz < S; oz++) {
          for (let ox = 0; ox < S; ox++) {
            const fx = x * S + ox, fz = z * S + oz;
            const t = this.cells[fz * this.w + fx];
            if (Grid.blocks(t)) blocked = true;
            if (t === T.ROAD) road++;
            minD = Math.min(minD, this.distBuild[fz * this.w + fx]);
          }
        }
        let cost;
        if (blocked) cost = -1;
        else {
          cost = road >= 2 ? 1 : 2.5;
          if (minD < 2.5) cost += 3;
          else if (minD < 4) cost += 1;
        }
        this.navCost[z * nw + x] = cost;
      }
    }
    this.gScore = new Float32Array(nw * nh);
    this.came = new Int32Array(nw * nh);
    this.stamp = new Uint32Array(nw * nh);
    this.closed = new Uint32Array(nw * nh);
    this.curStamp = 1;
    this.heap = new MinHeap(4096);
  }

  navIndexNear(x, z) {
    const S = this.navS;
    let nx = Math.floor((x - this.minX) / S), nz = Math.floor((z - this.minZ) / S);
    for (let r = 0; r < 8; r++) {
      for (let oz = -r; oz <= r; oz++) {
        for (let ox = -r; ox <= r; ox++) {
          if (Math.max(Math.abs(ox), Math.abs(oz)) !== r) continue;
          const ax = nx + ox, az = nz + oz;
          if (ax < 0 || az < 0 || ax >= this.nw || az >= this.nh) continue;
          if (this.navCost[az * this.nw + ax] > 0) return az * this.nw + ax;
        }
      }
    }
    return -1;
  }

  // A* over the coarse grid. Returns world-space waypoints (smoothed) or null.
  findPath(x0, z0, x1, z1, maxIter = 40000, radius = 1.2) {
    const start = this.navIndexNear(x0, z0);
    const goal = this.navIndexNear(x1, z1);
    if (start < 0 || goal < 0) return null;
    const nw = this.nw, nh = this.nh, S = this.navS;
    const stamp = ++this.curStamp;
    const g = this.gScore, came = this.came, st = this.stamp, closed = this.closed, cost = this.navCost;
    const heap = this.heap; heap.clear();
    const gx = goal % nw, gz = (goal / nw) | 0;
    const h = (i) => {
      const dx = Math.abs((i % nw) - gx), dz = Math.abs(((i / nw) | 0) - gz);
      return (dx + dz + (1.4142 - 2) * Math.min(dx, dz));
    };
    st[start] = stamp; g[start] = 0; came[start] = -1;
    heap.push(start, h(start));
    let iter = 0, found = false;
    const DX = [1, -1, 0, 0, 1, 1, -1, -1];
    const DZ = [0, 0, 1, -1, 1, -1, 1, -1];
    const DC = [1, 1, 1, 1, 1.4142, 1.4142, 1.4142, 1.4142];
    while (heap.size > 0 && iter++ < maxIter) {
      const cur = heap.pop();
      if (closed[cur] === stamp) continue;
      closed[cur] = stamp;
      if (cur === goal) { found = true; break; }
      const cx = cur % nw, cz = (cur / nw) | 0;
      for (let k = 0; k < 8; k++) {
        const ax = cx + DX[k], az = cz + DZ[k];
        if (ax < 0 || az < 0 || ax >= nw || az >= nh) continue;
        const ni = az * nw + ax;
        const c = cost[ni];
        if (c < 0) continue;
        if (k >= 4 && (cost[cz * nw + ax] < 0 || cost[az * nw + cx] < 0)) continue;
        const ng = g[cur] + DC[k] * c;
        if (st[ni] !== stamp || ng < g[ni]) {
          st[ni] = stamp; g[ni] = ng; came[ni] = cur;
          heap.push(ni, ng + h(ni));
        }
      }
    }
    if (!found) return null;
    const raw = [];
    for (let i = goal; i >= 0; i = came[i]) {
      raw.push({ x: this.minX + (i % nw) * S + S / 2, z: this.minZ + ((i / nw) | 0) * S + S / 2 });
      if (i === start) break;
    }
    raw.reverse();
    raw[raw.length - 1] = { x: x1, z: z1 };
    // String pulling.
    const out = [raw[0]];
    let anchor = 0;
    for (let i = 2; i < raw.length; i++) {
      const a = raw[anchor];
      if (!this.clearPath(a.x, a.z, raw[i].x, raw[i].z, radius) || i - anchor > 25) {
        out.push(raw[i - 1]);
        anchor = i - 1;
      }
    }
    if (raw.length > 1) out.push(raw[raw.length - 1]);
    return out;
  }
}
