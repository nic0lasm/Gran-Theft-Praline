// Game: owns the renderer, the world and every system, and runs the loop.

import * as THREE from 'three';
import { World } from './world/world.js';
import { Sky } from './world/sky.js';
import { Input } from './input.js';
import { Audio } from './audio.js';
import { Player, CameraRig } from './entities/player.js';
import { collideVehicles } from './entities/vehicle.js';
import { PedSystem } from './systems/peds.js';
import { TrafficSystem } from './systems/traffic.js';
import { PoliceSystem } from './systems/police.js';
import { WeaponSystem } from './systems/weapons.js';
import { PickupSystem } from './systems/pickups.js';
import { MissionSystem, MISSIONS } from './systems/missions.js';
import { Effects } from './systems/effects.js';
import { Hud } from './ui/hud.js';
import { dist, rand, formatMoney } from './util.js';

const SAVE_KEY = 'gran-theft-praline-save-v1';

class Events {
  constructor() { this.map = {}; }
  on(name, fn) {
    (this.map[name] = this.map[name] || []).push(fn);
    return () => { this.map[name] = this.map[name].filter((f) => f !== fn); };
  }
  emit(name, ...args) { for (const fn of this.map[name] || []) fn(...args); }
}

export class Game {
  constructor(container) {
    this.renderer = new THREE.WebGLRenderer({ antialias: true, powerPreference: 'high-performance' });
    this.renderer.setPixelRatio(Math.min(devicePixelRatio, 1.75));
    this.renderer.setSize(innerWidth, innerHeight);
    this.renderer.shadowMap.enabled = true;
    this.renderer.shadowMap.type = THREE.PCFShadowMap;
    this.renderer.toneMapping = THREE.ACESFilmicToneMapping;
    this.renderer.toneMappingExposure = 1.0;
    container.appendChild(this.renderer.domElement);
    this.scene = new THREE.Scene();
    this.camera = new THREE.PerspectiveCamera(62, innerWidth / innerHeight, 0.1, 2600);
    this.input = new Input(this.renderer.domElement);
    this.audio = new Audio();
    this.events = new Events();
    this.sky = new Sky(this.scene);
    this.world = new World(this.scene);
    this.vehicles = [];
    this.time = 0;
    this.money = 0;
    this.state = 'loading';
    this.paused = false;
    this.pauseReason = null;
    this.stats = { kills: 0, copKills: 0, cars: 0, carjacks: 0, shots: 0, traboules: 0 };
    this.save = { pralines: [] };
    this.god = false;
    addEventListener('resize', () => {
      this.camera.aspect = innerWidth / innerHeight;
      this.camera.updateProjectionMatrix();
      this.renderer.setSize(innerWidth, innerHeight);
    });
  }

  async init(progress) {
    await this.world.load(progress);
    this.grid = this.world.grid;
    progress('Calcul des itinéraires…');
    this.grid.buildNav();
    this.effects = new Effects(this.scene);
    const start = this.world.pois.planque;
    this.player = new Player(this, start.x + start.nx * 2, start.z + start.nz * 2);
    this.cam = new CameraRig(this, this.camera);
    this.cam.yaw = Math.atan2(start.nx, start.nz) + Math.PI / 2;
    this.peds = new PedSystem(this);
    this.traffic = new TrafficSystem(this);
    this.police = new PoliceSystem(this);
    this.weapons = new WeaponSystem(this);
    this.pickups = new PickupSystem(this);
    this.missions = new MissionSystem(this);
    this.hud = new Hud(this);
    this.state = 'title';
    document.addEventListener('pointerlockchange', () => {
      if (!this.input.locked && this.state === 'play' && !this.paused) this.setPaused(true, 'menu');
    });
    this.lastT = performance.now();
    this.renderer.setAnimationLoop(() => this.frame());
  }

  hasSave() {
    try { return !!localStorage.getItem(SAVE_KEY); } catch (e) { return false; }
  }

  newGame(load) {
    let data = null;
    if (load) {
      try { data = JSON.parse(localStorage.getItem(SAVE_KEY)); } catch (e) { data = null; }
    }
    const pl = this.player;
    if (data) {
      this.money = data.money || 0;
      this.save.pralines = data.pralines || [];
      pl.ammo = data.ammo || {};
      pl.weapon = data.weapon && (data.weapon === 'fists' || pl.ammo[data.weapon]) ? data.weapon : 'fists';
      pl.armor = data.armor || 0;
      pl.health = Math.max(50, data.health || 100);
      this.sky.time = data.time ?? 10;
      Object.assign(this.stats, data.stats || {});
    } else {
      this.money = 250;
      this.save.pralines = [];
      this.sky.time = 9.5;
    }
    this.pickups.init(this.save.pralines);
    this.missions.init(data ? data.mission || 0 : 0);
    this.state = 'play';
    this.audio.start();
    this.input.lock();
    if (!data) {
      this.hud.big('GRAN THEFT PRALINE', 'Lyon, Presqu’île. Les Sucrerouge ont volé toutes les pralines de la ville.', 5);
      setTimeout(() => this.hud.toast('Allez voir Tatie Rose : suivez le marqueur rose sur la minicarte.'), 5000);
    } else this.hud.toast('Partie chargée.');
  }

  persist() {
    const pl = this.player;
    const data = {
      money: this.money, pralines: this.save.pralines, ammo: pl.ammo, weapon: pl.weapon, armor: pl.armor,
      health: pl.health, time: this.sky.time, mission: this.missions.index, stats: this.stats, savedAt: Date.now(),
    };
    try { localStorage.setItem(SAVE_KEY, JSON.stringify(data)); } catch (e) { /* private mode */ }
  }

  addMoney(n) {
    this.money = Math.max(0, this.money + n);
    if (n > 0) this.audio.cash();
  }

  addVehicle(v) {
    this.vehicles.push(v);
    this.scene.add(v.mesh);
  }

  removeVehicle(v) {
    const i = this.vehicles.indexOf(v);
    if (i >= 0) this.vehicles.splice(i, 1);
    this.scene.remove(v.mesh);
    v.removed = true;
    if (v.driver && v.driver !== 'player' && v.driver.remove) v.driver.remove();
    for (const c of v.crew || []) if (c.inVehicle === v) c.remove();
    if (this.player.lastVehicle === v) this.player.lastVehicle = null;
  }

  setPaused(on, reason = 'menu') {
    this.paused = on;
    this.pauseReason = on ? reason : null;
    const menu = document.getElementById('pause');
    menu.style.display = on && reason === 'menu' ? 'flex' : 'none';
    if (on) { this.input.unlock(); this.audio.engine(false, 0, 0); this.audio.siren(0, 0); }
    else if (this.state === 'play') this.input.lock();
  }

  // ---- Death and arrest -------------------------------------------------
  wasted() { this.endLife('VOUS ÊTES MORT', 'hopital', 0.1, 'Hôpital : soins facturés'); }
  busted() { this.endLife('ARRÊTÉ', 'commissariat', 0.12, 'Commissariat : armes confisquées, amende payée'); }

  endLife(title, where, fine, sub) {
    const pl = this.player;
    if (pl.dead) return;
    pl.dead = true;
    if (title === 'ARRÊTÉ') pl.health = Math.max(pl.health, 1);
    if (pl.vehicle) pl.exitVehicle(true);
    if (this.missions.active) this.missions.fail(title === 'ARRÊTÉ' ? 'Vous avez été arrêté.' : 'Vous êtes mort.');
    this.hud.big(title, '', 4);
    this.audio.fail();
    setTimeout(() => {
      this.hud.fade(1.2, () => {
        const poi = this.world.pois[where];
        const cost = Math.max(100, Math.round(this.money * fine));
        this.money = Math.max(0, this.money - cost);
        if (where === 'commissariat') pl.ammo = {};
        pl.weapon = pl.hasWeapon(pl.weapon) ? pl.weapon : 'fists';
        pl.x = poi.x + poi.nx * 2; pl.z = poi.z + poi.nz * 2; pl.y = 0;
        pl.health = 100; pl.armor = where === 'hopital' ? 0 : pl.armor;
        pl.dead = false;
        pl.vx = pl.vz = 0;
        pl.h.fall = 0;
        this.police.reset();
        this.cam.yaw = Math.atan2(poi.nx, poi.nz) + Math.PI / 2;
        this.hud.toast(`${sub} : -${formatMoney(cost)}`);
        this.persist();
      });
    }, 3500);
  }

  // ---- Vehicles: fire and explosions -----------------------------------
  updateVehicles(dt) {
    const pl = this.player;
    collideVehicles(this.vehicles, (a, b, k) => {
      const dmg = Math.max(0, k - 3) * 22;
      a.damage(dmg); b.damage(dmg);
      if (k > 4 && dist(a.x, a.z, pl.x, pl.z) < 50) this.audio.crash(k);
      const mine = a === pl.vehicle ? b : b === pl.vehicle ? a : null;
      if (mine && k > 5) {
        if (mine.police || mine.crew?.length) this.police.crime(2, mine.x, mine.z, 'cop');
        if (mine.ai && mine.driver && !mine.police) { mine.ai.mode = 'panic'; }
        this.events.emit('vehicleShot', mine);
        this.cam.shake = Math.min(0.5, k * 0.03);
      }
    });
    for (const v of this.vehicles) {
      if (!v.driver && !v.ai && v !== pl.vehicle) {
        // Unattended cars roll to a stop.
        if (v.speed > 0.05) v.physics(dt, { throttle: 0, steer: 0, handbrake: true }, this.grid);
      }
      if (v.burnTimer > 0 && !v.dead) {
        v.burnTimer -= dt;
        if (Math.random() < dt * 25) this.effects.fire(new THREE.Vector3(v.x + rand.range(-0.5, 0.5), 1.2, v.z + rand.range(-0.5, 0.5)));
        if (v.burnTimer <= 0) this.explodeVehicle(v);
      } else if (!v.dead && v.health < v.maxHealth * 0.4 && dist(v.x, v.z, pl.x, pl.z) < 80) {
        v.smokeT -= dt;
        if (v.smokeT <= 0) {
          v.smokeT = 0.15;
          const fx = Math.sin(v.heading), fz = Math.cos(v.heading);
          this.effects.smoke(new THREE.Vector3(v.x + fx * v.halfL * 0.8, 1.1, v.z + fz * v.halfL * 0.8), v.health < v.maxHealth * 0.2 ? 0.15 : 0.6, 0.6);
        }
      } else if (v.dead && Math.random() < dt * 2 && dist(v.x, v.z, pl.x, pl.z) < 60) {
        this.effects.smoke(new THREE.Vector3(v.x, 1, v.z), 0.1, 0.8);
      }
      v.visuals(dt, this.sky.night, this.time);
    }
  }

  explodeVehicle(v) {
    const pl = this.player;
    v.explode();
    const p = new THREE.Vector3(v.x, 1, v.z);
    this.effects.explosion(p);
    const dp = dist(v.x, v.z, pl.x, pl.z);
    this.audio.explosion();
    this.cam.shake = Math.max(this.cam.shake, Math.max(0, 1 - dp / 40));
    if (v.driver && v.driver !== 'player' && v.driver.die) { v.driver.inVehicle = null; v.driver.x = v.x + 1; v.driver.die(null); }
    if (pl.vehicle === v) { pl.exitVehicle(true); pl.hurt(200); }
    else if (dp < 7) pl.hurt(90 * (1 - dp / 7));
    for (const ped of this.peds.list) {
      if (ped.inVehicle || ped.dead) continue;
      const d = dist(ped.x, ped.z, v.x, v.z);
      if (d < 6) ped.damage(200, v.lastHitBy || null);
      else if (d < 25 && !ped.cop) { ped.state = 'flee'; ped.threat = { x: v.x, z: v.z }; ped.timer = 10; }
    }
    for (const o of this.vehicles) {
      if (o === v) continue;
      const d = dist(o.x, o.z, v.x, v.z);
      if (d < 7) {
        o.damage(500 * (1 - d / 7));
        const k = (7 - d) * 1.5;
        o.vx += ((o.x - v.x) / (d || 1)) * k; o.vz += ((o.z - v.z) / (d || 1)) * k;
      }
    }
    if (v.lastHitBy === 'player' || v.playerOwned) this.police.crime(v.police ? 3 : 2, v.x, v.z, 'explosion');
    this.events.emit('vehicleExploded', v);
  }

  // ---- Main loop ---------------------------------------------------------
  frame() {
    const now = performance.now();
    let dt = (now - this.lastT) / 1000;
    this.lastT = now;
    dt = Math.min(dt, 0.05);
    const input = this.input;
    input.pollPad();
    if (this.state === 'title') {
      // Slow fly-over behind the menu.
      this.time += dt;
      const a = this.time * 0.05;
      this.camera.position.set(10 + Math.sin(a) * 60, 45, Math.cos(a) * 90);
      this.camera.lookAt(10, 8, 0);
      const st = this.sky.update(dt * 0.2, { x: 10, z: 0 });
      this.world.setNight(st.night);
    } else if (this.state === 'play') {
      if (input.hit('Escape') && (this.hud.shopOpen)) this.hud.closeShop();
      else if (input.hit('KeyM') && !this.hud.shopOpen) this.hud.toggleMap();
      else if (input.hit('Escape') && this.hud.mapOpen) this.hud.toggleMap();
      else if ((input.hit('KeyP')) && !this.hud.shopOpen && !this.hud.mapOpen) this.setPaused(!this.paused, 'menu');
      if (!this.paused) this.update(dt);
    }
    if (this.state !== 'loading') this.renderer.render(this.scene, this.camera);
    input.endFrame();
  }

  update(dt) {
    this.time += dt;
    if (!this.input.locked && !this.paused && this.input.clicked) this.input.lock();
    // Wanted blip on bodies: subsystems.
    this.player.update(dt);
    this.traffic.update(dt);
    this.police.update(dt);
    this.peds.update(dt);
    this.updateVehicles(dt);
    this.pickups.update(dt);
    this.missions.update(dt);
    this.cam.update(dt);
    this.effects.update(dt);
    const st = this.sky.update(dt, this.player);
    this.world.setNight(st.night);
    this.hud.update(dt);
    // Water shimmer.
    this.world.waterMaterial.color.setHSL(0.47, 0.2, 0.28 + Math.sin(this.time * 0.7) * 0.01);
    void MISSIONS;
  }
}
