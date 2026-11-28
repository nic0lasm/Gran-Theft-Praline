// Visual effects: bullet tracers, impact sparks, smoke, fire, explosions,
// muzzle flashes. Everything is pooled and short-lived.

import * as THREE from 'three';
import { rand } from '../util.js';

export class Effects {
  constructor(scene) {
    this.scene = scene;
    this.items = [];
    this.puffGeo = new THREE.IcosahedronGeometry(0.5, 0);
    this.sparkGeo = new THREE.BoxGeometry(0.06, 0.06, 0.06);
    this.flash = new THREE.PointLight(0xffc070, 0, 12, 2);
    scene.add(this.flash);
    this.flashT = 0;
    this.boom = new THREE.PointLight(0xff8030, 0, 40, 2);
    scene.add(this.boom);
    this.boomT = 0;
  }

  add(obj, life, update) {
    this.scene.add(obj);
    this.items.push({ obj, life, max: life, update });
  }

  tracer(from, to, color = 0xffe9a0) {
    const geo = new THREE.BufferGeometry().setFromPoints([from.clone(), to.clone()]);
    const mat = new THREE.LineBasicMaterial({ color, transparent: true, opacity: 0.9 });
    const line = new THREE.Line(geo, mat);
    this.add(line, 0.07, (it, k) => { mat.opacity = k * 0.9; });
  }

  muzzle(pos) {
    this.flash.position.copy(pos);
    this.flash.intensity = 30;
    this.flashT = 0.05;
  }

  sparks(pos, color = 0xffd27a, n = 6) {
    for (let i = 0; i < n; i++) {
      const m = new THREE.Mesh(this.sparkGeo, new THREE.MeshBasicMaterial({ color }));
      m.position.copy(pos);
      const v = new THREE.Vector3(rand.range(-3, 3), rand.range(1, 4), rand.range(-3, 3));
      this.add(m, 0.35, (it, k, dt) => { v.y -= 12 * dt; m.position.addScaledVector(v, dt); m.scale.setScalar(k); });
    }
  }

  blood(pos) { this.sparks(pos, 0x8a1010, 5); }

  smoke(pos, dark = 0.4, size = 1) {
    const mat = new THREE.MeshStandardMaterial({ color: new THREE.Color(dark, dark, dark), transparent: true, opacity: 0.55, depthWrite: false, roughness: 1 });
    const m = new THREE.Mesh(this.puffGeo, mat);
    m.position.copy(pos);
    const vy = rand.range(1.2, 2.4), drift = new THREE.Vector3(rand.range(-0.4, 0.4), 0, rand.range(-0.4, 0.4));
    this.add(m, 1.8, (it, k, dt) => {
      m.position.y += vy * dt;
      m.position.addScaledVector(drift, dt);
      m.scale.setScalar(size * (0.6 + (1 - k) * 2.2));
      mat.opacity = 0.55 * k;
    });
  }

  fire(pos) {
    const mat = new THREE.MeshBasicMaterial({ color: rand() < 0.5 ? 0xff7a1a : 0xffc23a, transparent: true, opacity: 0.9, depthWrite: false });
    const m = new THREE.Mesh(this.puffGeo, mat);
    m.position.copy(pos);
    this.add(m, 0.6, (it, k, dt) => { m.position.y += 2.5 * dt; m.scale.setScalar(0.4 + k * 0.8); mat.opacity = k; });
  }

  explosion(pos) {
    const mat = new THREE.MeshBasicMaterial({ color: 0xffa040, transparent: true, opacity: 1, depthWrite: false });
    const m = new THREE.Mesh(new THREE.SphereGeometry(1, 16, 12), mat);
    m.position.copy(pos);
    this.add(m, 0.7, (it, k) => { m.scale.setScalar(1 + (1 - k) * 7); mat.opacity = k; });
    for (let i = 0; i < 12; i++) this.smoke(pos.clone().add(new THREE.Vector3(rand.range(-2, 2), rand.range(0, 2), rand.range(-2, 2))), 0.15, 2);
    this.sparks(pos, 0xffb040, 20);
    this.boom.position.copy(pos).setY(3);
    this.boom.intensity = 400;
    this.boomT = 0.6;
  }

  update(dt) {
    for (let i = this.items.length - 1; i >= 0; i--) {
      const it = this.items[i];
      it.life -= dt;
      if (it.life <= 0) {
        this.scene.remove(it.obj);
        if (it.obj.geometry && it.obj.geometry !== this.puffGeo && it.obj.geometry !== this.sparkGeo) it.obj.geometry.dispose();
        if (it.obj.material) it.obj.material.dispose();
        this.items.splice(i, 1);
        continue;
      }
      it.update(it, it.life / it.max, dt);
    }
    if (this.flashT > 0) { this.flashT -= dt; if (this.flashT <= 0) this.flash.intensity = 0; }
    if (this.boomT > 0) { this.boomT -= dt; this.boom.intensity = Math.max(0, this.boomT / 0.6) * 400; }
    if (this.items.length > 400) {
      const old = this.items.splice(0, this.items.length - 400);
      for (const it of old) this.scene.remove(it.obj);
    }
  }
}
