// Low-poly people built from boxes, with a procedural walk/run/aim/fall rig.

import * as THREE from 'three';
import { rand } from '../util.js';

const geoCache = {};
function box(w, h, d, oy = 0) {
  const k = `${w},${h},${d},${oy}`;
  if (!geoCache[k]) {
    const g = new THREE.BoxGeometry(w, h, d);
    g.translate(0, oy, 0);
    geoCache[k] = g;
  }
  return geoCache[k];
}
const matCache = {};
function mat(color) {
  if (!matCache[color]) matCache[color] = new THREE.MeshStandardMaterial({ color, roughness: 0.85 });
  return matCache[color];
}

const SKIN = [0xf1c9a5, 0xe0ac87, 0xc68c64, 0x8d5a3b, 0x5d3a26, 0xf5d6bd];
const HAIR = [0x2b1d14, 0x4a2f1c, 0x8a6a3c, 0xc9a86a, 0x111111, 0x777777];
const TOPS = [0x2e4a7a, 0x7a2e2e, 0x3d6b45, 0xd0c8b8, 0x222222, 0x8a6aa8, 0xc98a2e, 0x4f8fa8, 0xa83f6b, 0x666e73];
const BOTTOMS = [0x1f2a44, 0x2b2b2b, 0x4a3f35, 0x6b6f75, 0x334455, 0x8a7a60];

export function randomOutfit() {
  return {
    skin: rand.pick(SKIN), hair: rand.pick(HAIR), top: rand.pick(TOPS), bottom: rand.pick(BOTTOMS),
    shoes: rand() < 0.5 ? 0x111111 : 0xdddddd, hat: rand() < 0.12 ? rand.pick(TOPS) : null,
  };
}

export const PLAYER_OUTFIT = { skin: 0xe8b996, hair: 0x2b1d14, top: 0xe0559a, bottom: 0x2a3550, shoes: 0xf2f2f2, hat: null };
export const COP_OUTFIT = { skin: 0xe0ac87, hair: 0x2b1d14, top: 0x1d2d5c, bottom: 0x16203f, shoes: 0x111111, hat: 0x0f1834, cop: true };

export class Humanoid {
  constructor(outfit, scale = 1) {
    this.outfit = outfit;
    const root = new THREE.Group();
    const body = new THREE.Group(); // tilts when falling
    root.add(body);
    const m = (c) => mat(c);
    // Hips at y=0.95
    const hips = new THREE.Group();
    hips.position.y = 0.95;
    body.add(hips);
    const torso = new THREE.Mesh(box(0.46, 0.62, 0.26, 0.31), m(outfit.top));
    torso.position.y = 0.02;
    hips.add(torso);
    const pelvis = new THREE.Mesh(box(0.42, 0.16, 0.24, -0.02), m(outfit.bottom));
    hips.add(pelvis);
    const head = new THREE.Group();
    head.position.y = 0.66;
    hips.add(head);
    head.add(new THREE.Mesh(box(0.1, 0.08, 0.1, 0.04), m(outfit.skin)));
    const skull = new THREE.Mesh(box(0.24, 0.27, 0.25, 0.2), m(outfit.skin));
    head.add(skull);
    const hair = new THREE.Mesh(box(0.26, 0.09, 0.27, 0.35), m(outfit.hair));
    head.add(hair);
    if (outfit.hat || outfit.cop) {
      const cap = new THREE.Mesh(box(0.28, 0.1, 0.3, 0.38), m(outfit.hat || 0x0f1834));
      head.add(cap);
      const visor = new THREE.Mesh(box(0.26, 0.03, 0.12, 0.34), m(0x111111));
      visor.position.z = 0.18;
      head.add(visor);
    }
    if (outfit.cop) {
      const badge = new THREE.Mesh(box(0.08, 0.08, 0.02), m(0xd9c36a));
      badge.position.set(0.12, 0.45, 0.14);
      hips.add(badge);
      const belt = new THREE.Mesh(box(0.48, 0.06, 0.28), m(0x111111));
      belt.position.y = 0.06;
      hips.add(belt);
    }
    const limb = (x, y, len, w, c, parent, hand) => {
      const g = new THREE.Group();
      g.position.set(x, y, 0);
      const mesh = new THREE.Mesh(box(w, len, w, -len / 2), m(c));
      g.add(mesh);
      if (hand) {
        const h = new THREE.Mesh(box(0.09, 0.1, 0.09, -len - 0.05), m(hand));
        g.add(h);
      }
      parent.add(g);
      return g;
    };
    this.armL = limb(-0.3, 0.58, 0.58, 0.12, outfit.top, hips, outfit.skin);
    this.armR = limb(0.3, 0.58, 0.58, 0.12, outfit.top, hips, outfit.skin);
    this.legL = limb(-0.12, -0.04, 0.88, 0.16, outfit.bottom, hips);
    this.legR = limb(0.12, -0.04, 0.88, 0.16, outfit.bottom, hips);
    for (const leg of [this.legL, this.legR]) {
      const shoe = new THREE.Mesh(box(0.15, 0.08, 0.26, -0.88), m(outfit.shoes));
      shoe.position.z = 0.05;
      leg.add(shoe);
    }
    // Gun held in the right hand (hidden unless armed).
    this.gun = new THREE.Mesh(box(0.06, 0.1, 0.3), m(0x1a1a1a));
    this.gun.position.set(0, -0.62, 0.12);
    this.gun.visible = false;
    this.armR.add(this.gun);
    root.scale.setScalar(scale);
    root.traverse((o) => { if (o.isMesh) { o.castShadow = true; } });
    this.root = root;
    this.body = body;
    this.hips = hips;
    this.head = head;
    this.phase = rand() * 10;
    this.fall = 0; // 0 standing → 1 lying
  }

  // speed m/s; state: 'idle' | 'walk' | 'run' | 'aim' | 'hands' | 'dead' | 'punch' | 'phone'
  animate(dt, speed, state, punchT = 0) {
    const run = speed > 3.2;
    this.phase += dt * (run ? 11 : 7.5) * Math.min(1.4, Math.max(speed, 0.0) / (run ? 5.5 : 1.5));
    const s = Math.sin(this.phase);
    const amp = speed < 0.1 ? 0 : run ? 0.9 : 0.5;
    if (state === 'dead') {
      this.fall = Math.min(1, this.fall + dt * 3);
    } else {
      this.fall = Math.max(0, this.fall - dt * 3);
    }
    this.body.rotation.x = -this.fall * Math.PI / 2;
    this.body.position.y = this.fall * 0.15;
    this.legL.rotation.x = s * amp;
    this.legR.rotation.x = -s * amp;
    this.hips.position.y = 0.95 + Math.abs(Math.cos(this.phase)) * amp * 0.06;
    this.hips.rotation.x = run ? 0.12 : 0;
    let al = -s * amp * 0.8, ar = s * amp * 0.8, alz = 0.06, arz = -0.06;
    if (state === 'aim') { ar = -Math.PI / 2; arz = 0; al = -Math.PI / 2.4; alz = -0.35; }
    else if (state === 'hands') { al = ar = Math.PI; alz = 0.25; arz = -0.25; }
    else if (state === 'punch') { ar = -Math.PI / 2 * Math.sin(Math.min(1, punchT) * Math.PI); }
    else if (state === 'phone') { ar = -2.4; arz = -0.5; }
    else if (state === 'dead') { al = -2.8; ar = -2.6; this.legL.rotation.x = 0.1; this.legR.rotation.x = -0.15; }
    this.armL.rotation.x = al; this.armR.rotation.x = ar;
    this.armL.rotation.z = alz; this.armR.rotation.z = arz;
  }
}
