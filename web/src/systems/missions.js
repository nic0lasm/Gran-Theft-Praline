// The story: Tatie Rose's pâtisserie against the Sucrerouge praline chain.
// Five missions built from small reusable steps.

import { Vehicle } from '../entities/vehicle.js';
import { Driver } from './traffic.js';
import { Ped } from './peds.js';
import { markerMesh } from './pickups.js';
import { T } from '../world/grid.js';
import { dist, rand } from '../util.js';

const TATIE = 'Tatie Rose';

// ---- Step builders --------------------------------------------------------
const dialog = (lines) => ({
  type: 'dialog',
  start(m) { m.d.line = -1; m.d.t = 0; },
  update(m, dt, g) {
    m.d.t -= dt;
    if (m.d.t <= 0 || g.input.hit('Enter') || g.input.hit('Space')) {
      m.d.line++;
      if (m.d.line >= lines.length) { g.hud.subtitle(null); return true; }
      const [who, text] = lines[m.d.line];
      g.hud.subtitle(who, text);
      m.d.t = 2.2 + text.length * 0.045;
    }
    return false;
  },
});

const at = (g, where) => (typeof where === 'string' ? g.world.pois[where] : typeof where === 'function' ? where(g) : where);

const goto = (where, text, opts = {}) => ({
  type: 'goto',
  text,
  start(m, g) { m.setBlip(at(g, where), opts.radius ?? 3); },
  update(m, dt, g) {
    const p = at(g, where), pl = g.player;
    if (opts.vehicle === true && !pl.vehicle) { g.hud.objective(opts.needCar || 'Il vous faut un véhicule.'); return false; }
    if (opts.vehicleRef && pl.vehicle !== m.d[opts.vehicleRef]) { g.hud.objective('Retournez dans le véhicule de la mission.'); m.setBlip(m.d[opts.vehicleRef], 2); return false; }
    if (opts.onFoot && pl.vehicle) { g.hud.objective(text + ' (à pied)'); }
    else g.hud.objective(text);
    m.setBlip(p, opts.radius ?? 3);
    const r = (opts.radius ?? 3) + (pl.vehicle ? 2 : 0);
    if (dist(pl.x, pl.z, p.x, p.z) < r && (!opts.onFoot || !pl.vehicle)) {
      if (pl.vehicle && opts.stop !== false && pl.vehicle.speed > 6) { g.hud.objective('Arrêtez-vous.'); return false; }
      return true;
    }
    return false;
  },
});

const steal = (text, ref = null) => ({
  type: 'steal',
  start(m) { m.clearBlip(); },
  update(m, dt, g) {
    g.hud.objective(text);
    if (ref && m.d[ref]) m.setBlip(m.d[ref], 2);
    if (ref) return g.player.vehicle === m.d[ref];
    return !!g.player.vehicle;
  },
});

const pickupAt = (kind, where, text, key) => ({
  type: 'pickup',
  start(m, g) {
    const p = at(g, where);
    const q = g.grid.findNearest(p.x, p.z, (t) => t === T.OPEN || t === T.PLAZA || t === T.ROAD, 12) || p;
    m.d[key] = g.pickups.spawn(kind, q.x, q.z, { mission: true });
    m.own(m.d[key]);
  },
  update(m, dt, g) {
    const it = m.d[key];
    g.hud.objective(text + (g.player.vehicle ? ' (à pied)' : ''));
    m.setBlip(it, 1.5);
    return !it.active;
  },
});

const wanted = (level) => ({ type: 'instant', start(m, g) { g.police.setWanted(Math.max(g.police.wanted, level)); }, update: () => true });

const lose = (text = 'Semez la police.') => ({
  type: 'lose',
  start(m) { m.clearBlip(); },
  update(m, dt, g) {
    g.hud.objective(text);
    return g.police.wanted === 0;
  },
});

const custom = (def) => ({ type: 'custom', ...def });

// ---- Missions -------------------------------------------------------------
export const MISSIONS = [
  {
    id: 'm1', title: 'Les pralines de Tatie', reward: 500,
    steps: [
      dialog([
        [TATIE, 'Nicolito ! Te voilà enfin. Tu sais ce qu’ils ont fait, les Sucrerouge ?'],
        [TATIE, 'Ils ont racheté toutes les pralines de Lyon. Toutes ! Plus une seule pour mes tartes.'],
        [TATIE, 'Une caisse m’attend à l’entrepôt du quai, côté Rhône. Prends une voiture et ramène-la-moi.'],
      ]),
      steal('Trouvez une voiture : approchez-vous et appuyez sur F.'),
      goto('entrepot', 'Allez à l’entrepôt du quai.', { vehicle: true, needCar: 'Trouvez une voiture.' }),
      pickupAt('crate', 'entrepot', 'Récupérez la caisse de pralines.', 'crate'),
      goto('tatie', 'Rapportez la caisse à Tatie Rose.', { radius: 3 }),
      dialog([[TATIE, 'Magnifique ! Ça sent déjà la tarte. Reviens me voir, j’aurai encore besoin de toi.']]),
    ],
  },
  {
    id: 'm2', title: 'Vitrine rose', reward: 1500,
    steps: [
      custom({
        start(m, g) {
          if (!g.player.hasWeapon('pistol')) { g.player.giveWeapon('pistol', 36); g.hud.toast('Tatie vous glisse son vieux pistolet « Gone 9 ».'); }
        },
        update: () => true,
      }),
      dialog([
        [TATIE, 'La Maison Sucrerouge, vers Bellecour. Leur caisse déborde, et la recette de leurs pralines dort dans le tiroir.'],
        [TATIE, 'Montre ton pistolet à la vendeuse, prends la caisse et la première page de la recette.'],
        [TATIE, 'Personne n’est blessé, compris ? Et si la police s’en mêle, pense aux traboules.'],
      ]),
      custom({
        start(m, g) {
          const p = g.world.pois.sucrerouge;
          const ped = new Ped(g, p.x + p.nx * 1.2, p.z + p.nz * 1.2, { persistent: true, outfit: { skin: 0xf1c9a5, hair: 0x8a6a3c, top: 0xad1457, bottom: 0x222222, shoes: 0x111111, hat: null } });
          ped.state = 'idle';
          ped.heading = Math.atan2(p.nx, p.nz);
          g.peds.list.push(ped);
          m.d.clerk = ped;
          m.own(ped);
        },
        update: () => true,
      }),
      goto('sucrerouge', 'Allez à la Maison Sucrerouge.', { onFoot: true, radius: 6 }),
      custom({
        start(m) { m.d.hold = 0; },
        update(m, dt, g) {
          const c = m.d.clerk, pl = g.player;
          m.setBlip(c, 1);
          if (c.dead || m.d.hold >= 3.5) {
            g.hud.progress(null);
            g.pickups.dropCash(c.x, c.z, 0);
            m.d.bagAt = { x: c.x - g.world.pois.sucrerouge.nx * 0.2, z: c.z - g.world.pois.sucrerouge.nz * 0.2 };
            if (!c.dead) { c.state = 'cower'; c.timer = 30; }
            g.police.setWanted(c.dead ? 3 : 2, c.x, c.z);
            return true;
          }
          // Aiming roughly at her is enough (a cone in front of the player).
          const dc = dist(pl.x, pl.z, c.x, c.z);
          const ang = Math.abs(Math.atan2(Math.sin(Math.atan2(c.x - pl.x, c.z - pl.z) - pl.heading), Math.cos(Math.atan2(c.x - pl.x, c.z - pl.z) - pl.heading)));
          const aimed = !pl.vehicle && pl.weapon !== 'fists' && g.input.aiming() && dc < 12 && (ang < 0.35 || (pl.aimTarget && pl.aimTarget.target === c));
          if (aimed) { m.d.hold += dt; c.state = 'hands'; c.timer = 3; }
          g.hud.objective(pl.weapon === 'fists' ? 'Sortez une arme (molette ou 2) puis visez la vendeuse (clic droit).' : 'Braquez la vendeuse : visez-la (clic droit) sans tirer.');
          g.hud.progress('Braquage', m.d.hold / 3.5);
          return false;
        },
      }),
      pickupAt('bag', (g) => g.missions.active.d.bagAt, 'Ramassez la caisse et la page de recette.', 'bag'),
      custom({ start(m, g) { g.addMoney(800); g.hud.toast('+800 € de la caisse'); }, update: () => true }),
      lose('Semez la police (astuce : une traboule, touche E devant une porte « Traboule »).'),
      goto('tatie', 'Retournez chez Tatie Rose.'),
      dialog([[TATIE, 'La première page ! « Amandes, noisettes, sucre cuit au cuivre… » Il nous manque la suite.']]),
    ],
  },
  {
    id: 'm3', title: 'Livraison express', reward: 2000,
    steps: [
      dialog([
        [TATIE, 'Avec cette page, j’ai sorti une fournée de tartes à la praline. Trois clients attendent.'],
        [TATIE, 'L’utilitaire est garé juste là. Doucement : si tu le cabosses, les tartes sont fichues.'],
        [TATIE, 'Tu as 3 minutes 30. Les tartes, ça se mange tiède.'],
      ]),
      custom({
        start(m, g) {
          const p = g.world.pois.tatie;
          const spot = g.grid.findNearest(p.x, p.z, (t, ix, iz) => t === T.ROAD && g.grid.distEdge[iz * g.grid.w + ix] >= 2.5, 60);
          const v = new Vehicle('utilitaire', spot.x, spot.z, g.traffic.roadDir(spot.x, spot.z), 0xf2f2f2);
          v.mission = true;
          g.addVehicle(v);
          m.d.van = v;
          m.own(v);
        },
        update: () => true,
      }),
      steal('Montez dans l’utilitaire de Tatie Rose.', 'van'),
      custom({ start(m, g) { m.d.deadline = g.time + 210; }, update: () => true }),
      goto('boulangerie', 'Livrez la Boulangerie du Bon Pain.', { vehicleRef: 'van' }),
      custom({ start(m, g) { g.addMoney(150); g.hud.toast('Livraison 1/3 : +150 €'); }, update: () => true }),
      goto('armurerie', 'Livrez l’Arsenal du Rhône (même les armuriers aiment les tartes).', { vehicleRef: 'van' }),
      custom({ start(m, g) { g.addMoney(150); g.hud.toast('Livraison 2/3 : +150 €'); }, update: () => true }),
      goto('hopital', 'Livrez l’hôpital (pour le moral des infirmières).', { vehicleRef: 'van' }),
      custom({ start(m, g) { g.addMoney(150); g.hud.toast('Livraison 3/3 : +150 €'); m.d.deadline = null; }, update: () => true }),
      goto('tatie', 'Ramenez l’utilitaire chez Tatie Rose.', { vehicleRef: 'van' }),
      dialog([[TATIE, 'Tout le monde en redemande ! Les Sucrerouge doivent être verts de rage.']]),
    ],
    check(m, g) {
      const v = m.d.van;
      if (v && (v.dead || v.health < v.maxHealth * 0.4)) return 'Les tartes sont écrasées !';
      if (m.d.deadline && g.time > m.d.deadline) return 'Trop tard : les tartes ont refroidi.';
      if (m.d.deadline) g.hud.timer(m.d.deadline - g.time);
      if (v && m.step >= 3 && v.health < v.maxHealth) g.hud.progress('État des tartes', (v.health / v.maxHealth - 0.4) / 0.6);
      return null;
    },
  },
  {
    id: 'm4', title: 'Le coursier', reward: 2500,
    steps: [
      dialog([
        [TATIE, 'Les Sucrerouge font porter la deuxième page de la recette par un coursier en scooter.'],
        [TATIE, 'Il vient de passer. Arrête-le et récupère la mallette, tous les moyens sont bons.'],
      ]),
      custom({
        start(m, g) {
          const pl = g.player;
          let spot = null;
          for (let k = 0; k < 60 && !spot; k++) {
            const a = rand() * Math.PI * 2, r = rand.range(45, 80);
            const x = pl.x + Math.sin(a) * r, z = pl.z + Math.cos(a) * r;
            if (g.grid.type(x, z) === T.ROAD && g.grid.distE(x, z) >= 2) spot = { x, z };
          }
          spot = spot || g.grid.findNearest(pl.x, pl.z, (t) => t === T.ROAD, 80);
          const v = new Vehicle('scooter', spot.x, spot.z, g.traffic.roadDir(spot.x, spot.z), 0xad1457);
          v.mission = true;
          v.ai = new Driver(v, { mode: 'flee', cruise: 13 });
          const d = new Ped(g, v.x, v.z, { persistent: true, outfit: { skin: 0xe0ac87, hair: 0x111111, top: 0xad1457, bottom: 0x222222, shoes: 0x111111, hat: 0xad1457 } });
          d.inVehicle = v;
          v.driver = d;
          g.peds.list.push(d);
          g.addVehicle(v);
          m.d.scooter = v; m.d.courier = d;
          m.own(v); m.own(d);
        },
        update: () => true,
      }),
      custom({
        update(m, dt, g) {
          const v = m.d.scooter, d = m.d.courier;
          m.setBlip(v, 2);
          g.hud.objective('Arrêtez le coursier Sucrerouge (tirez ou percutez son scooter).');
          if (v.health < v.maxHealth * 0.55 || d.dead || v.dead || (d.inVehicle !== v)) {
            if (d.inVehicle === v && !d.dead) {
              d.inVehicle = null; d.x = v.x + 1.5; d.z = v.z; d.state = 'flee'; d.threat = { x: g.player.x, z: g.player.z }; d.timer = 20;
            }
            v.driver = null; v.ai = null;
            m.d.caseAt = { x: v.x, z: v.z };
            g.police.setWanted(Math.max(g.police.wanted, 2), v.x, v.z);
            return true;
          }
          return false;
        },
      }),
      pickupAt('briefcase', (g) => g.missions.active.d.caseAt, 'Ramassez la mallette.', 'case'),
      lose(),
      goto('tatie', 'Rapportez la mallette à Tatie Rose.'),
      dialog([[TATIE, 'Deuxième page : « …puis enrober deux fois, et colorer au rose de Lyon. » Il ne manque plus que le secret final.']]),
    ],
    check(m, g) {
      const v = m.d.scooter;
      if (v && m.step === 2 && dist(v.x, v.z, g.player.x, g.player.z) > 260) return 'Le coursier vous a semé.';
      return null;
    },
  },
  {
    id: 'm5', title: 'Le casse du fourgon', reward: 10000,
    steps: [
      dialog([
        [TATIE, 'Le grand jour. Le fourgon blindé Sucrerouge transporte la Praline d’Or… et la recette complète.'],
        [TATIE, 'Il fait le tour de la Presqu’île. Il faudra de la puissance de feu : passe à l’armurerie si besoin.'],
        [TATIE, 'Fais-le sauter, prends la Praline d’Or et file à ta planque. Après ça, Lyon retrouvera ses pralines.'],
      ]),
      custom({
        start(m, g) {
          const pl = g.player;
          let best = null;
          for (let k = 0; k < 200; k++) {
            const a = rand() * Math.PI * 2, r = rand.range(110, 170);
            const x = pl.x + Math.sin(a) * r, z = pl.z + Math.cos(a) * r;
            if (g.grid.type(x, z) === T.ROAD && g.grid.distE(x, z) >= 3) { best = { x, z }; break; }
          }
          best = best || g.grid.findNearest(pl.x + 120, pl.z, (t) => t === T.ROAD, 120);
          const v = new Vehicle('fourgon', best.x, best.z, g.traffic.roadDir(best.x, best.z));
          v.mission = true;
          v.ai = new Driver(v, { mode: 'cruise', cruise: 10 });
          const d = new Ped(g, v.x, v.z, { persistent: true, cop: false });
          d.inVehicle = v;
          v.driver = d;
          g.peds.list.push(d);
          g.addVehicle(v);
          m.d.van = v;
          m.own(v); m.own(d);
          m.d.attacked = false;
          m.d.offShot = g.events.on('vehicleShot', (veh) => {
            if (veh === v && !m.d.attacked) { m.d.attacked = true; v.ai.mode = 'flee'; v.ai.cruise = 14; g.police.setWanted(Math.max(3, g.police.wanted), v.x, v.z); }
          });
        },
        update: () => true,
      }),
      custom({
        update(m, dt, g) {
          const v = m.d.van;
          m.setBlip(v, 2.5);
          g.hud.objective(m.d.attacked ? 'Détruisez le fourgon blindé !' : 'Trouvez et détruisez le fourgon blindé Sucrerouge.');
          g.hud.progress('Blindage', v.health / v.maxHealth);
          if (v.health <= v.maxHealth * 0.2 && !m.d.attacked) { m.d.attacked = true; g.police.setWanted(3, v.x, v.z); }
          if (v.health <= 0) {
            g.hud.progress(null);
            if (v.driver && v.driver !== 'player') {
              const d = v.driver; d.inVehicle = null; d.x = v.x + 2; d.z = v.z; d.state = 'flee'; d.threat = { x: v.x, z: v.z }; d.timer = 20;
              v.driver = null;
            }
            v.ai = null;
            m.d.goldAt = { x: v.x + Math.sin(v.heading + 1.6) * 3, z: v.z + Math.cos(v.heading + 1.6) * 3 };
            return true;
          }
          return false;
        },
      }),
      pickupAt('golden', (g) => g.missions.active.d.goldAt, 'Ramassez la Praline d’Or !', 'gold'),
      wanted(4),
      lose('Semez la police : quatre étoiles, un hélicoptère vous survole !'),
      goto('planque', 'Rentrez à votre planque.'),
      dialog([
        [TATIE, 'La Praline d’Or… et la recette complète ! Nicolito, tu es un héros de la gastronomie lyonnaise.'],
        [TATIE, 'Demain, toutes les boulangeries de la ville auront des pralines. Et les Sucrerouge ? Ruinés.'],
      ]),
    ],
    finale: true,
  },
];

export class MissionSystem {
  constructor(game) {
    this.game = game;
    this.index = 0;
    this.active = null;
    this.blip = null;
    this.giverMarker = null;
    this.blipMarker = markerMesh(0xe0559a);
    this.blipMarker.visible = false;
    game.scene.add(this.blipMarker);
  }

  init(index) {
    this.index = index;
    const g = this.game;
    this.giverMarker = markerMesh(0xe0559a);
    const p = g.world.pois.tatie;
    this.giverMarker.position.set(p.x, 0, p.z);
    g.scene.add(this.giverMarker);
  }

  get giver() { return this.game.world.pois.tatie; }

  start(i) {
    const def = MISSIONS[i];
    const g = this.game;
    this.active = {
      def, step: -1, d: {}, owned: [],
      setBlip: (p, r = 3) => { this.blip = p ? { x: p.x, z: p.z, r } : null; },
      clearBlip: () => { this.blip = null; },
      own: (o) => this.active.owned.push(o),
    };
    g.hud.big(def.title, `Mission ${i + 1} / ${MISSIONS.length}`, 3);
    this.next();
  }

  next() {
    const m = this.active, g = this.game;
    m.step++;
    g.hud.progress(null);
    if (m.step >= m.def.steps.length) { this.complete(); return; }
    const s = m.def.steps[m.step];
    if (s.start) s.start(m, g);
  }

  cleanup(keepVehicles = false) {
    const m = this.active, g = this.game;
    if (!m) return;
    if (m.d.offShot) m.d.offShot();
    for (const o of m.owned) {
      if (o.kind !== undefined && o.mesh) { if (o.active) g.pickups.remove(o); continue; }
      if (o.def) { // vehicle
        o.mission = false;
        if (!keepVehicles && o !== g.player.vehicle) g.removeVehicle(o);
        continue;
      }
      if (o.h) { o.persistent = false; if (!keepVehicles && !o.inVehicle) o.remove(); }
    }
    g.hud.subtitle(null);
    g.hud.progress(null);
    g.hud.timer(null);
    g.hud.objective('');
    this.blip = null;
    this.active = null;
  }

  complete() {
    const m = this.active, g = this.game;
    const def = m.def;
    this.cleanup(true);
    this.index = Math.max(this.index, MISSIONS.indexOf(def) + 1);
    g.addMoney(def.reward);
    g.audio.pass();
    g.hud.big('MISSION RÉUSSIE', `${def.title} · +${def.reward.toLocaleString('fr-FR')} €`, 4);
    g.persist();
    if (def.finale) setTimeout(() => g.hud.credits(g), 4500);
  }

  fail(reason) {
    const g = this.game;
    if (!this.active) return;
    g.audio.fail();
    g.hud.big('MISSION ÉCHOUÉE', reason, 4);
    this.cleanup(false);
  }

  update(dt) {
    const g = this.game, pl = g.player;
    // Giver marker.
    const giverOn = !this.active && this.index < MISSIONS.length;
    this.giverMarker.visible = giverOn;
    if (giverOn) {
      this.giverMarker.userData.arrow.rotation.y += dt * 2;
      const p = this.giver;
      if (dist(pl.x, pl.z, p.x, p.z) < 2.5 && !pl.vehicle && !pl.dead) {
        if (g.police.wanted > 0) g.hud.help('Semez d’abord la police avant de voir Tatie Rose.');
        else {
          g.hud.help(`E : mission « ${MISSIONS[this.index].title} »`);
          if (g.input.hit('KeyE')) this.start(this.index);
        }
      }
    }
    const m = this.active;
    if (m) {
      const def = m.def;
      if (def.check) {
        const fail = def.check(m, g);
        if (fail) { this.fail(fail); return; }
      }
      const s = def.steps[m.step];
      if (s && s.update(m, dt, g)) this.next();
    }
    // World marker on the blip (not on moving targets).
    const b = this.blip;
    if (b && this.active) {
      this.blipMarker.visible = true;
      this.blipMarker.position.set(b.x, 0, b.z);
      const s = Math.max(0.6, b.r / 1.4);
      this.blipMarker.scale.set(s, 1, s);
      this.blipMarker.userData.arrow.rotation.y += dt * 2;
    } else this.blipMarker.visible = false;
  }
}
