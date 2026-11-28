// The city: the real rue de la République slice (IGN BD TOPO buildings and
// roads, from assets/geo/processed/rue-republique) in the middle, extended by
// procedural Presqu'île blocks out to the Rhône and the Saône, with Place
// Bellecour to the south.

import * as THREE from 'three';
import { GLTFLoader } from 'three/examples/jsm/loaders/GLTFLoader.js';
import { Grid, T } from './grid.js';
import {
  makeFacade, makeShopfront, makeRoof, makeGroundTexture, makeSignTexture,
  BAY_W, FLOOR_H, BAYS, FLOORS, GROUND_H, SHOP_BAYS,
} from './textures.js';
import { makeRng, hashString, dist } from '../util.js';
import glbUrl from '../../../assets/geo/processed/rue-republique/rue-republique.glb?url';

export const WORLD = { minX: -330, maxX: 330, minZ: -410, maxZ: 410 };

const TINTS = [
  [0.98, 0.94, 0.86], [0.95, 0.88, 0.74], [0.9, 0.88, 0.84], [0.96, 0.84, 0.74],
  [0.97, 0.9, 0.7], [0.93, 0.91, 0.87], [0.92, 0.8, 0.68], [0.98, 0.96, 0.92],
];
const ROOF_TINTS = [[0.62, 0.66, 0.72], [0.55, 0.58, 0.62], [0.7, 0.5, 0.42], [0.6, 0.62, 0.64]];

// Accumulates building triangles into 4 material groups.
class BuildingBatch {
  constructor() {
    this.groups = [0, 1, 2, 3].map(() => ({ pos: [], nor: [], uv: [], col: [] }));
  }

  // p: array of 3 THREE.Vector3; n: outward normal; info: {style, tint, roof, uOff}
  addTriangle(p, n, info) {
    if (n.y > 0.35) {
      this.emit(3, p, n, info, (v) => [v.x / 12, v.z / 12], info.roof);
      return;
    }
    if (n.y < -0.5) return; // floors under buildings are never seen
    const len = Math.hypot(n.x, n.z) || 1;
    const tx = n.z / len, tz = -n.x / len;
    const along = (v) => v.x * tx + v.z * tz + info.uOff;
    const uFn = (v) => along(v) / (BAY_W * BAYS);
    const uShop = (v) => along(v) / (BAY_W * SHOP_BAYS);
    const below = clipPoly(p, GROUND_H, true);
    const above = clipPoly(p, GROUND_H, false);
    if (below.length >= 3) {
      this.emitPoly(0, below, n, info, (v) => [uShop(v), Math.max(0, v.y) / GROUND_H], [1, 1, 1]);
    }
    if (above.length >= 3) {
      this.emitPoly(1 + info.style, above, n, info, (v) => [uFn(v), (v.y - GROUND_H) / (FLOOR_H * FLOORS)], info.tint);
    }
  }

  emitPoly(g, poly, n, info, uvFn, col) {
    for (let i = 1; i + 1 < poly.length; i++) this.emit(g, [poly[0], poly[i], poly[i + 1]], n, info, uvFn, col);
  }

  emit(g, tri, n, info, uvFn, col) {
    const G = this.groups[g];
    for (const v of tri) {
      G.pos.push(v.x, v.y, v.z);
      G.nor.push(n.x, n.y, n.z);
      const uv = uvFn(v);
      G.uv.push(uv[0], uv[1]);
      G.col.push(col[0], col[1], col[2]);
    }
  }

  addBox(x0, z0, x1, z1, h, info, y0 = -0.5) {
    const V = (x, y, z) => new THREE.Vector3(x, y, z);
    const quad = (a, b, c, d, n) => {
      this.addTriangle([a, b, c], n, info);
      this.addTriangle([a, c, d], n, info);
    };
    // Walls (counter-clockwise seen from outside).
    quad(V(x0, y0, z1), V(x1, y0, z1), V(x1, h, z1), V(x0, h, z1), new THREE.Vector3(0, 0, 1));
    quad(V(x1, y0, z0), V(x0, y0, z0), V(x0, h, z0), V(x1, h, z0), new THREE.Vector3(0, 0, -1));
    quad(V(x1, y0, z1), V(x1, y0, z0), V(x1, h, z0), V(x1, h, z1), new THREE.Vector3(1, 0, 0));
    quad(V(x0, y0, z0), V(x0, y0, z1), V(x0, h, z1), V(x0, h, z0), new THREE.Vector3(-1, 0, 0));
    quad(V(x0, h, z1), V(x1, h, z1), V(x1, h, z0), V(x0, h, z0), new THREE.Vector3(0, 1, 0));
  }

  build(materials) {
    const geo = new THREE.BufferGeometry();
    const total = this.groups.reduce((s, G) => s + G.pos.length / 3, 0);
    const pos = new Float32Array(total * 3), nor = new Float32Array(total * 3);
    const uv = new Float32Array(total * 2), col = new Float32Array(total * 3);
    let start = 0;
    this.groups.forEach((G, i) => {
      pos.set(G.pos, start * 3); nor.set(G.nor, start * 3); uv.set(G.uv, start * 2); col.set(G.col, start * 3);
      const count = G.pos.length / 3;
      geo.addGroup(start, count, i);
      start += count;
    });
    geo.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3));
    geo.setAttribute('normal', new THREE.Float32BufferAttribute(nor, 3));
    geo.setAttribute('uv', new THREE.Float32BufferAttribute(uv, 2));
    geo.setAttribute('color', new THREE.Float32BufferAttribute(col, 3));
    geo.computeBoundingSphere();
    const mesh = new THREE.Mesh(geo, materials);
    mesh.castShadow = true;
    mesh.receiveShadow = true;
    return mesh;
  }
}

// Sutherland–Hodgman against the plane y = h; keep the part below (or above).
function clipPoly(poly, h, keepBelow) {
  const inside = (v) => (keepBelow ? v.y <= h : v.y >= h);
  const out = [];
  for (let i = 0; i < poly.length; i++) {
    const a = poly[i], b = poly[(i + 1) % poly.length];
    const ia = inside(a), ib = inside(b);
    if (ia) out.push(a);
    if (ia !== ib) {
      const t = (h - a.y) / (b.y - a.y);
      out.push(new THREE.Vector3(a.x + (b.x - a.x) * t, h, a.z + (b.z - a.z) * t));
    }
  }
  return out;
}

export class World {
  constructor(scene) {
    this.scene = scene;
    this.grid = new Grid(WORLD.minX, WORLD.minZ, WORLD.maxX - WORLD.minX, WORLD.maxZ - WORLD.minZ);
    this.rng = makeRng(1789);
    this.materials = [];
    this.lamps = [];
    this.trees = [];
    this.traboules = [];
    this.pois = {};
    this.nightMaterials = [];
  }

  async load(onProgress = () => {}) {
    onProgress('Chargement de la rue de la République (IGN BD TOPO)…');
    const gltf = await new GLTFLoader().loadAsync(glbUrl);
    gltf.scene.updateMatrixWorld(true);
    const buildings = [];
    let roads = null, ground = null;
    gltf.scene.traverse((o) => {
      if (!o.isMesh) return;
      if (o.name.startsWith('B_')) buildings.push(o);
      else if (o.name.startsWith('Roads')) roads = o;
      else if (o.name.startsWith('Ground')) ground = o;
    });
    const gb = new THREE.Box3().setFromObject(ground);
    this.core = { x0: gb.min.x, x1: gb.max.x, z0: gb.min.z, z1: gb.max.z };
    this.buildingCount = buildings.length;

    onProgress('Rasterisation de la ville…');
    const grid = this.grid;
    const batch = new BuildingBatch();
    this.layout(batch);
    this.rasterRoads(roads);
    this.addGlbBuildings(buildings, batch);
    this.finishGrid();

    onProgress('Façades lyonnaises…');
    this.buildMaterials();
    this.buildingMesh = batch.build(this.materials);
    this.scene.add(this.buildingMesh);

    onProgress('Sol, quais et fleuves…');
    this.buildGround();
    this.buildWater();
    this.buildBackdrop();

    onProgress('Platanes et lampadaires…');
    this.buildProps();
    this.buildBellecour();
    this.findTraboules();
    this.placePois();
    return this;
  }

  // ---- Layout of the procedural extension -------------------------------
  layout(batch) {
    const g = this.grid, c = this.core, rng = this.rng;
    const R = 14; // ring boulevard width
    this.ring = { x0: c.x0 - R, x1: c.x1 + R, z0: c.z0 - R, z1: c.z1 + R };
    const QUAI_IN = 288, QUAI_OUT = 302, NORTH_ROAD = [-395, -381], SOUTH_ROAD = [382, 396];
    this.quai = { inner: QUAI_IN, outer: QUAI_OUT };
    this.plaza = { x0: -185, x1: 185, z0: this.ring.z1, z1: SOUTH_ROAD[0] };
    // Everything starts as sidewalk.
    g.cells.fill(T.OPEN);
    // Outer rows of buildings beyond the edge roads (north/south) and the river (east/west).
    const road = (x0, z0, x1, z1) => g.fillRect(x0, z0, x1, z1, T.ROAD);
    // Ring boulevard around the IGN slice, extended to the quais.
    road(-QUAI_IN, this.ring.z0, QUAI_IN, c.z0);
    road(-QUAI_IN, c.z1, QUAI_IN, this.ring.z1);
    road(this.ring.x0, NORTH_ROAD[0], c.x0, SOUTH_ROAD[1]);
    road(c.x1, NORTH_ROAD[0], this.ring.x1, SOUTH_ROAD[1]);
    // Quais along the rivers.
    road(QUAI_IN, NORTH_ROAD[0], QUAI_OUT, SOUTH_ROAD[1]);
    road(-QUAI_OUT, NORTH_ROAD[0], -QUAI_IN, SOUTH_ROAD[1]);
    // Edge roads north and south.
    road(-QUAI_OUT, NORTH_ROAD[0], QUAI_OUT, NORTH_ROAD[1]);
    road(-QUAI_OUT, SOUTH_ROAD[0], QUAI_OUT, SOUTH_ROAD[1]);
    // Parapets and rivers.
    g.fillRect(QUAI_OUT, WORLD.minZ, QUAI_OUT + 1, WORLD.maxZ, T.OBST);
    g.fillRect(-QUAI_OUT - 1, WORLD.minZ, -QUAI_OUT, WORLD.maxZ, T.OBST);
    g.fillRect(QUAI_OUT + 1, WORLD.minZ, WORLD.maxX, WORLD.maxZ, T.WATER);
    g.fillRect(WORLD.minX, WORLD.minZ, -QUAI_OUT - 1, WORLD.maxZ, T.WATER);

    // Cross streets.
    const crossZ = [-340, -205, -120, -35, 50, 135, 215, 340];
    for (const zc of crossZ) {
      road(this.ring.x1, zc - 6, QUAI_IN, zc + 6);
      road(-QUAI_IN, zc - 6, this.ring.x0, zc + 6);
    }
    const crossXNorth = [-110, 10, 120];
    for (const xc of crossXNorth) road(xc - 6, NORTH_ROAD[1], xc + 6, this.ring.z0);

    const info = () => ({
      style: rng() < 0.55 ? 0 : 1,
      tint: rng.pick(TINTS),
      roof: rng.pick(ROOF_TINTS),
      uOff: rng() * 40,
    });
    const blockFill = (x0, z0, x1, z1) => {
      const inset = 3.5;
      x0 += inset; z0 += inset; x1 -= inset; z1 -= inset;
      if (x1 - x0 < 8 || z1 - z0 < 8) return;
      const alongX = x1 - x0 >= z1 - z0;
      const depth = alongX ? z1 - z0 : x1 - x0;
      const rows = depth > 34 ? 2 : 1;
      for (let r = 0; r < rows; r++) {
        let a = alongX ? x0 : z0;
        const end = alongX ? x1 : z1;
        while (a < end - 4) {
          let w = rng.range(12, 24);
          if (end - (a + w) < 9) w = end - a;
          const h = rng.range(15, 27);
          const rd0 = (alongX ? z0 : x0) + (rows === 2 ? (r * depth) / 2 + (r === 1 ? 4 : 0) : 0);
          const rd1 = rows === 2 ? rd0 + depth / 2 - 4 : (alongX ? z1 : x1);
          const bx0 = alongX ? a : rd0, bx1 = alongX ? a + w : rd1;
          const bz0 = alongX ? rd0 : a, bz1 = alongX ? rd1 : a + w;
          batch.addBox(bx0, bz0, bx1, bz1, h, info());
          g.fillRect(bx0, bz0, bx1, bz1, T.BUILD);
          a += w;
        }
      }
    };
    // East and west strips.
    const allZ = [...crossZ.map((z) => [z - 6, z + 6]), [this.ring.z0, c.z0], [c.z1, this.ring.z1]]
      .sort((a, b) => a[0] - b[0]);
    let zPrev = NORTH_ROAD[1];
    const zSpans = [];
    for (const [za, zb] of allZ) { if (za > zPrev) zSpans.push([zPrev, za]); zPrev = Math.max(zPrev, zb); }
    zSpans.push([zPrev, SOUTH_ROAD[0]]);
    for (const [za, zb] of zSpans) {
      blockFill(this.ring.x1, za, QUAI_IN, zb);
      blockFill(-QUAI_IN, za, this.ring.x0, zb);
    }
    // North strip between the ring roads.
    const xs = [this.ring.x0 + 14, ...crossXNorth.flatMap((x) => [x - 6, x + 6]), this.ring.x1 - 14];
    for (let i = 0; i + 1 < xs.length; i += 2) blockFill(xs[i], NORTH_ROAD[1], xs[i + 1], this.ring.z0);
    // Outer frame of tall buildings (north and south edges).
    for (let x = -QUAI_OUT; x < QUAI_OUT; x += 22) {
      const w = Math.min(22, QUAI_OUT - x);
      const h1 = rng.range(18, 30), h2 = rng.range(18, 30);
      batch.addBox(x, WORLD.minZ - 12, x + w, NORTH_ROAD[0] - 3, h1, info());
      batch.addBox(x, SOUTH_ROAD[1] + 3, x + w, WORLD.maxZ + 12, h2, info());
    }
    g.fillRect(WORLD.minX, WORLD.minZ, WORLD.maxX, NORTH_ROAD[0] - 3, T.BUILD, (t) => t !== T.WATER && t !== T.OBST);
    g.fillRect(WORLD.minX, SOUTH_ROAD[1] + 3, WORLD.maxX, WORLD.maxZ, T.BUILD, (t) => t !== T.WATER && t !== T.OBST);
    // Place Bellecour.
    const p = this.plaza;
    g.fillRect(p.x0, p.z0, p.x1, p.z1, T.PLAZA);
  }

  rasterRoads(roads) {
    const g = this.grid;
    const geo = roads.geometry;
    const pos = geo.attributes.position;
    const idx = geo.index;
    const v = [new THREE.Vector3(), new THREE.Vector3(), new THREE.Vector3()];
    const n = idx ? idx.count : pos.count;
    for (let i = 0; i < n; i += 3) {
      for (let k = 0; k < 3; k++) {
        v[k].fromBufferAttribute(pos, idx ? idx.getX(i + k) : i + k).applyMatrix4(roads.matrixWorld);
      }
      g.fillTriangle(v[0].x, v[0].z, v[1].x, v[1].z, v[2].x, v[2].z, T.ROAD);
    }
  }

  addGlbBuildings(meshes, batch) {
    const g = this.grid;
    const a = new THREE.Vector3(), b = new THREE.Vector3(), c = new THREE.Vector3();
    const e1 = new THREE.Vector3(), e2 = new THREE.Vector3(), n = new THREE.Vector3();
    const vn = new THREE.Vector3();
    this.buildingHeights = [];
    for (const m of meshes) {
      const rng = makeRng(hashString(m.name));
      const info = {
        style: rng() < 0.7 ? 0 : 1, // the République is mostly dressed stone
        tint: TINTS[Math.floor(rng() * TINTS.length)],
        roof: ROOF_TINTS[Math.floor(rng() * ROOF_TINTS.length)],
        uOff: rng() * 40,
      };
      const geo = m.geometry;
      const pos = geo.attributes.position, nor = geo.attributes.normal;
      const idx = geo.index;
      const count = idx ? idx.count : pos.count;
      let maxY = 0;
      for (let i = 0; i < count; i += 3) {
        const i0 = idx ? idx.getX(i) : i, i1 = idx ? idx.getX(i + 1) : i + 1, i2 = idx ? idx.getX(i + 2) : i + 2;
        a.fromBufferAttribute(pos, i0).applyMatrix4(m.matrixWorld);
        b.fromBufferAttribute(pos, i1).applyMatrix4(m.matrixWorld);
        c.fromBufferAttribute(pos, i2).applyMatrix4(m.matrixWorld);
        e1.subVectors(b, a); e2.subVectors(c, a);
        n.crossVectors(e1, e2);
        if (n.lengthSq() < 1e-10) continue;
        n.normalize();
        if (nor) {
          vn.fromBufferAttribute(nor, i0);
          if (vn.dot(n) < 0) n.negate();
        }
        maxY = Math.max(maxY, a.y, b.y, c.y);
        batch.addTriangle([a.clone(), b.clone(), c.clone()], n.clone(), info);
        if (n.y > 0.35) g.fillTriangle(a.x, a.z, b.x, b.z, c.x, c.z, T.BUILD);
      }
      this.buildingHeights.push(maxY);
    }
  }

  finishGrid() {
    const g = this.grid;
    // The IGN road polygons are narrow carriageway axes: widen them across the
    // street space of the slice, keeping a 3 m sidewalk along the facades.
    const db = g.distanceField((t) => t === T.BUILD || t === T.WATER);
    const c = this.core;
    for (let pass = 0; pass < 6; pass++) {
      const grow = [];
      for (let iz = g.iz(c.z0); iz < g.iz(c.z1); iz++) {
        for (let ix = g.ix(c.x0); ix < g.ix(c.x1); ix++) {
          const i = iz * g.w + ix;
          if (g.cells[i] !== T.OPEN || db[i] < 3.2) continue;
          if (g.cells[i - 1] === T.ROAD || g.cells[i + 1] === T.ROAD || g.cells[i - g.w] === T.ROAD || g.cells[i + g.w] === T.ROAD) grow.push(i);
        }
      }
      for (const i of grow) g.cells[i] = T.ROAD;
    }
    // Close the world's outer border.
    g.fillRect(WORLD.minX, WORLD.minZ, WORLD.maxX, WORLD.minZ + 1, T.BUILD);
    g.fillRect(WORLD.minX, WORLD.maxZ - 1, WORLD.maxX, WORLD.maxZ, T.BUILD);
    g.computeFields();
    this.distRoad = g.distanceField((t) => t === T.ROAD, 40);
  }

  // ---- Rendering --------------------------------------------------------
  buildMaterials() {
    const shop = makeShopfront(7);
    const fa = makeFacade(0, 11);
    const fb = makeFacade(1, 23);
    const roof = makeRoof(5);
    const mk = (t, rough = 0.85) => {
      const m = new THREE.MeshStandardMaterial({
        map: t.map || t, vertexColors: true, roughness: rough, metalness: 0,
      });
      if (t.emissive) {
        m.emissiveMap = t.emissive;
        m.emissive = new THREE.Color(1, 0.9, 0.75);
        m.emissiveIntensity = 0;
        this.nightMaterials.push(m);
      }
      return m;
    };
    this.materials = [mk(shop, 0.7), mk(fa), mk(fb), mk(roof, 0.6)];
  }

  buildGround() {
    const g = this.grid;
    const tex = makeGroundTexture(g, T, 3);
    const geo = new THREE.PlaneGeometry(g.w, g.h);
    geo.rotateX(-Math.PI / 2);
    const mat = new THREE.MeshStandardMaterial({ map: tex, roughness: 0.92 });
    const mesh = new THREE.Mesh(geo, mat);
    mesh.position.set(g.minX + g.w / 2, 0, g.minZ + g.h / 2);
    mesh.receiveShadow = true;
    this.scene.add(mesh);
    this.groundMaterial = mat;
    // Minimap image (1 px per metre).
    const c = document.createElement('canvas');
    c.width = g.w; c.height = g.h;
    const ctx = c.getContext('2d');
    const img = ctx.createImageData(g.w, g.h);
    const col = {
      [T.OPEN]: [120, 128, 140], [T.ROAD]: [52, 56, 64], [T.BUILD]: [186, 178, 165],
      [T.WATER]: [64, 116, 150], [T.OBST]: [120, 128, 140], [T.PLAZA]: [168, 112, 92], [T.GRASS]: [90, 130, 80],
    };
    for (let i = 0; i < g.cells.length; i++) {
      const cc = col[g.cells[i]];
      img.data[i * 4] = cc[0]; img.data[i * 4 + 1] = cc[1]; img.data[i * 4 + 2] = cc[2]; img.data[i * 4 + 3] = 255;
    }
    ctx.putImageData(img, 0, 0);
    this.mapImage = c;
  }

  buildWater() {
    const q = this.quai.outer + 1;
    const mat = new THREE.MeshStandardMaterial({ color: 0x3f5f5c, roughness: 0.15, metalness: 0.3 });
    this.waterMaterial = mat;
    for (const side of [1, -1]) {
      const geo = new THREE.PlaneGeometry(700, 2600);
      geo.rotateX(-Math.PI / 2);
      const m = new THREE.Mesh(geo, mat);
      m.position.set(side * (q + 350), -2.2, 0);
      m.receiveShadow = true;
      this.scene.add(m);
      // Quai wall.
      const wall = new THREE.Mesh(new THREE.BoxGeometry(1.2, 3.2, 2600),
        new THREE.MeshStandardMaterial({ color: 0xb8ab94, roughness: 0.9 }));
      wall.position.set(side * (q - 0.4), -0.7, 0);
      wall.receiveShadow = true;
      this.scene.add(wall);
      // Parapet.
      const par = new THREE.Mesh(new THREE.BoxGeometry(0.6, 0.9, 820),
        new THREE.MeshStandardMaterial({ color: 0xcfc4b0, roughness: 0.85 }));
      par.position.set(side * (q - 0.5), 0.45, 0);
      par.castShadow = true;
      this.scene.add(par);
    }
  }

  // Far banks: Guillotière to the east, Vieux Lyon and the Fourvière hill to the west.
  buildBackdrop() {
    const rng = makeRng(99);
    const batch = new BuildingBatch();
    const info = () => ({ style: rng() < 0.5 ? 0 : 1, tint: rng.pick(TINTS), roof: rng.pick(ROOF_TINTS), uOff: rng() * 20 });
    for (let z = -900; z < 900; z += rng.range(14, 22)) {
      const w = rng.range(12, 20);
      batch.addBox(500, z, 520 + rng.range(5, 20), z + w, rng.range(16, 30), info(), -2.5);
      batch.addBox(-530 - rng.range(5, 20), z, -490, z + w, rng.range(12, 22), info(), -2.5);
    }
    const mesh = batch.build(this.materials);
    mesh.castShadow = false;
    this.scene.add(mesh);
    // Banks.
    const bankMat = new THREE.MeshStandardMaterial({ color: 0x9c9686, roughness: 1 });
    for (const [x, w] of [[600, 220], [-600, 240]]) {
      const b = new THREE.Mesh(new THREE.BoxGeometry(w, 2, 2600), bankMat);
      b.position.set(x, -1.2, 0);
      this.scene.add(b);
    }
    // Fourvière hill with its basilica and metal tower (silhouettes).
    const hill = new THREE.Mesh(new THREE.SphereGeometry(420, 32, 16, 0, Math.PI * 2, 0, Math.PI / 2),
      new THREE.MeshStandardMaterial({ color: 0x5f7448, roughness: 1, flatShading: true }));
    hill.scale.set(1, 0.32, 1.6);
    hill.position.set(-1050, -4, -150);
    this.scene.add(hill);
    const stone = new THREE.MeshStandardMaterial({ color: 0xe9e3d3, roughness: 0.8 });
    const basil = new THREE.Group();
    const body = new THREE.Mesh(new THREE.BoxGeometry(30, 22, 70), stone);
    body.position.y = 11;
    basil.add(body);
    for (const [x, z] of [[-15, -35], [15, -35], [-15, 35], [15, 35]]) {
      const tw = new THREE.Mesh(new THREE.BoxGeometry(8, 38, 8), stone);
      tw.position.set(x, 19, z);
      basil.add(tw);
    }
    basil.position.set(-760, 128, -180);
    this.scene.add(basil);
    const tower = new THREE.Mesh(new THREE.CylinderGeometry(1.5, 6, 85, 4),
      new THREE.MeshStandardMaterial({ color: 0x8d8f92, metalness: 0.6, roughness: 0.5 }));
    tower.position.set(-800, 170, -90);
    this.scene.add(tower);
  }

  buildProps() {
    const g = this.grid, rng = this.rng, dr = this.distRoad;
    const trees = [], lamps = [];
    const near = (list, x, z, d) => {
      for (let i = list.length - 1; i >= Math.max(0, list.length - 400); i--) {
        const p = list[i];
        if ((p.x - x) ** 2 + (p.z - z) ** 2 < d * d) return true;
      }
      return false;
    };
    for (let iz = 2; iz < g.h - 2; iz += 1) {
      for (let ix = 2; ix < g.w - 2; ix += 1) {
        const i = iz * g.w + ix;
        if (g.cells[i] !== T.OPEN) continue;
        const db = g.distBuild[i], d = dr[i];
        const x = g.cx(ix), z = g.cz(iz);
        if (d >= 1 && d <= 1.6 && db >= 4.5 && rng() < 0.2 && !near(trees, x, z, 10)) trees.push({ x, z });
        else if (d >= 0.9 && d <= 1.1 && db >= 2.2 && rng() < 0.15 && !near(lamps, x, z, 24) && !near(trees, x, z, 3)) lamps.push({ x, z });
      }
    }
    this.addTrees(trees);
    this.addLamps(lamps);
  }

  addTrees(list) {
    const rng = this.rng, g = this.grid;
    const trunkGeo = new THREE.CylinderGeometry(0.22, 0.32, 4, 6);
    trunkGeo.translate(0, 2, 0);
    const crownGeo = new THREE.IcosahedronGeometry(3, 1);
    const trunkMat = new THREE.MeshStandardMaterial({ color: 0x9b8f78, roughness: 1 });
    const crownMat = new THREE.MeshStandardMaterial({ color: 0x5b7a3a, roughness: 1, flatShading: true });
    const trunks = new THREE.InstancedMesh(trunkGeo, trunkMat, list.length);
    const crowns = new THREE.InstancedMesh(crownGeo, crownMat, list.length * 2);
    const m = new THREE.Matrix4(), q = new THREE.Quaternion(), s = new THREE.Vector3(), p = new THREE.Vector3();
    const col = new THREE.Color();
    list.forEach((t, i) => {
      const k = rng.range(0.85, 1.25);
      m.compose(p.set(t.x, 0, t.z), q.identity(), s.set(k, k * rng.range(0.9, 1.3), k));
      trunks.setMatrixAt(i, m);
      for (let j = 0; j < 2; j++) {
        q.setFromEuler(new THREE.Euler(rng() * 3, rng() * 3, rng() * 3));
        m.compose(p.set(t.x + rng.range(-1, 1), 5.2 * k + j * 1.6, t.z + rng.range(-1, 1)), q, s.set(k * rng.range(0.9, 1.2), k * 0.8, k * rng.range(0.9, 1.2)));
        crowns.setMatrixAt(i * 2 + j, m);
        crowns.setColorAt(i * 2 + j, col.setHSL(0.24 + rng() * 0.06, 0.38, 0.3 + rng() * 0.1));
      }
      g.fillCircle(t.x, t.z, 0.4, T.OBST, (c) => c === T.OPEN || c === T.PLAZA || c === T.GRASS);
      this.trees.push(t);
    });
    trunks.castShadow = crowns.castShadow = true;
    crowns.receiveShadow = true;
    this.scene.add(trunks, crowns);
  }

  addLamps(list) {
    const g = this.grid;
    const poleGeo = new THREE.CylinderGeometry(0.08, 0.12, 5, 6);
    poleGeo.translate(0, 2.5, 0);
    const headGeo = new THREE.BoxGeometry(0.45, 0.7, 0.45);
    const poleMat = new THREE.MeshStandardMaterial({ color: 0x1d2a24, roughness: 0.6, metalness: 0.4 });
    this.lampHeadMat = new THREE.MeshStandardMaterial({ color: 0xfff1cc, emissive: 0xffc46b, emissiveIntensity: 0 });
    this.nightMaterials.push(this.lampHeadMat);
    const poles = new THREE.InstancedMesh(poleGeo, poleMat, list.length);
    const heads = new THREE.InstancedMesh(headGeo, this.lampHeadMat, list.length);
    const m = new THREE.Matrix4();
    list.forEach((l, i) => {
      m.makeTranslation(l.x, 0, l.z);
      poles.setMatrixAt(i, m);
      m.makeTranslation(l.x, 5.2, l.z);
      heads.setMatrixAt(i, m);
      g.set(g.ix(l.x), g.iz(l.z), T.OBST);
      this.lamps.push(l);
    });
    poles.castShadow = true;
    this.scene.add(poles, heads);
  }

  buildBellecour() {
    const g = this.grid, p = this.plaza;
    const cx = 0, cz = (p.z0 + p.z1) / 2;
    this.bellecourCenter = { x: cx, z: cz };
    // Lawns in the corners.
    for (const [sx, sz] of [[-1, -1], [1, -1], [-1, 1], [1, 1]]) {
      const x0 = cx + sx * 120, z0 = cz + sz * 22;
      g.fillRect(Math.min(x0, x0 + sx * 50), Math.min(z0, z0 + sz * 16), Math.max(x0, x0 + sx * 50), Math.max(z0, z0 + sz * 16), T.GRASS);
    }
    // Plane trees on the east and west sides.
    const trees = [];
    for (let z = p.z0 + 8; z < p.z1 - 6; z += 9) {
      trees.push({ x: p.x0 + 6, z }, { x: p.x0 + 14, z }, { x: p.x1 - 6, z }, { x: p.x1 - 14, z });
    }
    this.addTrees(trees);
    // Pedestal and a bronze rider (a fictional monument).
    const ped = new THREE.Mesh(new THREE.BoxGeometry(6, 5, 10), new THREE.MeshStandardMaterial({ color: 0xd8d0c0, roughness: 0.8 }));
    ped.position.set(cx, 2.5, cz);
    ped.castShadow = ped.receiveShadow = true;
    const bronze = new THREE.MeshStandardMaterial({ color: 0x3d4a3a, metalness: 0.7, roughness: 0.4 });
    const horse = new THREE.Group();
    const body = new THREE.Mesh(new THREE.BoxGeometry(1.6, 1.6, 4), bronze);
    body.position.y = 2;
    const neck = new THREE.Mesh(new THREE.BoxGeometry(0.9, 2, 1), bronze);
    neck.position.set(0, 3, 1.9); neck.rotation.x = 0.5;
    const rider = new THREE.Mesh(new THREE.BoxGeometry(0.9, 2, 0.9), bronze);
    rider.position.set(0, 3.6, 0.1);
    horse.add(body, neck, rider);
    for (const [x, z] of [[-0.5, -1.5], [0.5, -1.5], [-0.5, 1.5], [0.5, 1.5]]) {
      const leg = new THREE.Mesh(new THREE.BoxGeometry(0.35, 1.6, 0.35), bronze);
      leg.position.set(x, 0.8, z);
      horse.add(leg);
    }
    horse.position.set(cx, 5, cz);
    horse.traverse((o) => { if (o.isMesh) o.castShadow = true; });
    this.scene.add(ped, horse);
    g.fillRect(cx - 3.2, cz - 5.2, cx + 3.2, cz + 5.2, T.OBST);
    // Benches around.
    const benchMat = new THREE.MeshStandardMaterial({ color: 0x2f4a3a, roughness: 0.7, metalness: 0.3 });
    for (let a = 0; a < 12; a++) {
      const ang = (a / 12) * Math.PI * 2;
      const b = new THREE.Mesh(new THREE.BoxGeometry(2, 0.5, 0.6), benchMat);
      b.position.set(cx + Math.cos(ang) * 16, 0.25, cz + Math.sin(ang) * 16);
      b.rotation.y = -ang + Math.PI / 2;
      b.castShadow = true;
      this.scene.add(b);
    }
    // Repaint the ground (lawns changed the grid).
    this.groundMaterial.map.dispose();
    this.groundMaterial.map = makeGroundTexture(g, T, 3);
    this.groundMaterial.needsUpdate = true;
  }

  // Traboules: passages through buildings, the Lyon way to vanish.
  findTraboules() {
    const g = this.grid, rng = makeRng(1543);
    const found = [];
    const dirs = [[1, 0], [-1, 0], [0, 1], [0, -1]];
    const walk = (t) => t === T.OPEN || t === T.PLAZA;
    for (let tries = 0; tries < 6000 && found.length < 8; tries++) {
      const x = rng.range(this.core.x0 + 10, this.core.x1 - 10);
      const z = rng.range(this.core.z0 + 10, this.core.z1 - 10);
      if (!walk(g.type(x, z))) continue;
      const [dx, dz] = dirs[Math.floor(rng() * 4)];
      if (g.type(x + dx, z + dz) !== T.BUILD) continue;
      let s = 1;
      while (s < 50 && g.type(x + dx * s, z + dz * s) === T.BUILD) s++;
      if (s < 12 || s >= 50) continue;
      const ex = x + dx * (s + 0.5), ez = z + dz * (s + 0.5);
      if (!walk(g.type(ex, ez)) || !walk(g.type(ex + dx, ez + dz))) continue;
      if (found.some((t) => dist(t.a.x, t.a.z, x, z) < 60 || dist(t.b.x, t.b.z, ex, ez) < 60)) continue;
      // Snap the doors onto the cell edges.
      const a = { x: Math.floor(x) + 0.5 + dx * 0.45, z: Math.floor(z) + 0.5 + dz * 0.45, nx: -dx, nz: -dz };
      const b = { x: Math.floor(ex) + 0.5 - dx * 0.45, z: Math.floor(ez) + 0.5 - dz * 0.45, nx: dx, nz: dz };
      found.push({ a, b, length: s });
    }
    const doorMat = new THREE.MeshStandardMaterial({ color: 0x2a1c14, roughness: 0.9 });
    const plate = new THREE.MeshStandardMaterial({ map: makeSignTexture('TRABOULE', '#1e2f4a', '#f3e6c8', 256, 64, 'bold 36px Georgia, serif') });
    for (const t of found) {
      for (const d of [t.a, t.b]) {
        const door = new THREE.Mesh(new THREE.PlaneGeometry(2.2, 3.4), doorMat);
        door.position.set(d.x + d.nx * 0.08, 1.7, d.z + d.nz * 0.08);
        door.lookAt(d.x + d.nx * 2, 1.7, d.z + d.nz * 2);
        const sign = new THREE.Mesh(new THREE.PlaneGeometry(1.4, 0.35), plate);
        sign.position.set(d.x + d.nx * 0.1, 3.8, d.z + d.nz * 0.1);
        sign.lookAt(d.x + d.nx * 2, 3.8, d.z + d.nz * 2);
        this.scene.add(door, sign);
      }
    }
    this.traboules = found;
  }

  // Shops and landmarks, snapped to a sidewalk cell against a facade.
  placePois() {
    const g = this.grid;
    const defs = {
      planque: { x: 30, z: -140, name: 'Planque', color: '#2e7d32' },
      tatie: { x: -40, z: -30, name: 'Pâtisserie Tatie Rose', color: '#c2185b' },
      armurerie: { x: 250, z: 80, name: "L'Arsenal du Rhône", color: '#37474f' },
      boulangerie: { x: -30, z: 150, name: 'Boulangerie du Bon Pain', color: '#8d6e63' },
      peinture: { x: -250, z: -120, name: 'Peinture Express', color: '#1565c0', road: true },
      hopital: { x: 110, z: -350, name: 'Hôpital', color: '#c62828' },
      commissariat: { x: -250, z: 300, name: 'Commissariat', color: '#283593' },
      sucrerouge: { x: 60, z: 210, name: 'Maison Sucrerouge', color: '#ad1457' },
      entrepot: { x: 270, z: -300, name: 'Entrepôt du quai', color: '#5d4037' },
      garage: { x: -260, z: 180, name: 'Garage Gerland', color: '#ef6c00', road: true },
    };
    for (const [id, d] of Object.entries(defs)) {
      const p = g.findNearest(d.x, d.z, (t, ix, iz) => {
        if (d.road) return t === T.ROAD && g.distEdge[iz * g.w + ix] >= 1.5 && g.distEdge[iz * g.w + ix] <= 2.6;
        return t === T.OPEN && g.distBuild[iz * g.w + ix] <= 1.5 && g.distBuild[iz * g.w + ix] >= 1;
      }, 80);
      if (!p) continue;
      // Outward direction: up the building-distance gradient.
      const gx = g.distB(p.x + 1, p.z) - g.distB(p.x - 1, p.z);
      const gz = g.distB(p.x, p.z + 1) - g.distB(p.x, p.z - 1);
      const l = Math.hypot(gx, gz) || 1;
      const poi = { id, ...d, x: p.x, z: p.z, nx: gx / l, nz: gz / l };
      this.pois[id] = poi;
      if (!d.road) {
        const sign = new THREE.Mesh(new THREE.PlaneGeometry(4.5, 1.1),
          new THREE.MeshStandardMaterial({ map: makeSignTexture(d.name, d.color), emissive: 0xffffff, emissiveIntensity: 0.25 }));
        sign.material.emissiveMap = sign.material.map;
        // The wall is ~1 m behind the sidewalk cell.
        sign.position.set(p.x - poi.nx * 0.9, 4.6, p.z - poi.nz * 0.9);
        sign.lookAt(sign.position.x + poi.nx, 4.6, sign.position.z + poi.nz);
        this.scene.add(sign);
      }
    }
  }

  zoneName(x, z) {
    if (x > this.quai.inner - 2) return 'Quai Jules-Courmont';
    if (x < -this.quai.inner + 2) return 'Quai Saint-Antoine';
    const p = this.plaza;
    if (x > p.x0 && x < p.x1 && z > p.z0 && z < p.z1) return 'Place Bellecour';
    if (Math.abs(x - 10) < 12 && z > this.core.z0 && z < this.core.z1) return 'Rue de la République';
    if (z < this.ring.z0) return 'Vers les Terreaux';
    if (z < -120) return 'Cordeliers';
    if (z < 120) return 'Jacobins';
    return 'Bellecour';
  }

  setNight(n) {
    for (const m of this.nightMaterials) m.emissiveIntensity = n * (m === this.lampHeadMat ? 3 : 1.1);
  }
}
