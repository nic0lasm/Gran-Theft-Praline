# Balayage perf M1 — 2026-11-10 / M1 perf sweep — 2026-11-10

## Français

**Build** : Shipping 0.3, commit 95e2aed (carte étendue au Rhône) + argument `-GfxCVars` (cette branche). Profil m1, fenêtre 1728×1117, screen percentage 48,3 %. MacBook Pro M1 Pro.
**Conditions** : Mac redémarré 15 min avant ; Spotlight indexait (charge 25 → 2 pendant le balayage, détail dans `load.txt` des preuves). Non comparable aux mesures des jours précédents ; seuls les écarts entre passes de ce jour comptent.
**Méthode** : une passe de 60 s par réglage, parcours démo (`-SkipTitle -DemoRoute`), 5 s d'échauffement exclues, un réglage à la fois via `-GfxCVars=`. Deux références (début et fin) : écart 0,9 ms GPU p50 = bruit de mesure.

**Pourquoi `-GfxCVars`** : la build Shipping ignore `-ExecCmds` et `-DPCVars` (vérifié : `t.MaxFPS 15` sans effet). `-GfxCVars=a=1,b=2` applique des cvars par-dessus le profil (vérifié : `t.MaxFPS=15` → 66,7 ms).

### Résultats (ms, 60 s chacun)

| Run | Frame p50 | Frame p95 | % <= 33 ms | GPU p50 | GPU p95 | Δ GPU p50 vs ref |
|---|---|---|---|---|---|---|
| ref | 34.3 | 43.2 | 37.8 | 34.3 | 42.7 | +0.4 |
| raytracing0 | 33.6 | 42.5 | 42.9 | 33.5 | 42.3 | -0.3 |
| vsm_smrt3_lod05 | 33.6 | 42.4 | 43.1 | 33.5 | 42.1 | -0.4 |
| vsm_off | 33.8 | 42.4 | 41.6 | 33.6 | 42.0 | -0.3 |
| lumen_nomeshsdf | 33.7 | 43.1 | 42.0 | 33.6 | 42.9 | -0.3 |
| lumen_refl_ds2 | 34.4 | 43.3 | 37.0 | 34.3 | 43.0 | +0.4 |
| lumen_sp_ds64 | 33.7 | 42.8 | 41.3 | 33.6 | 42.4 | -0.3 |
| lumen_card_1_16 | 33.3 | 42.1 | 49.3 | 33.2 | 41.6 | -0.7 |
| volfog0 | 33.7 | 42.2 | 40.0 | 33.7 | 42.1 | -0.1 |
| volcloud0 | 21.9 | 24.0 | 99.9 | 21.8 | 23.8 | -12.0 |
| lumen_transrefl0 | 33.2 | 41.6 | 51.9 | 33.1 | 41.5 | -0.8 |
| postfx_low | 33.1 | 41.8 | 54.0 | 33.0 | 41.2 | -0.9 |
| ref2 | 33.6 | 42.1 | 44.8 | 33.4 | 42.0 | -0.4 |

Moyenne des deux références : frame p50 33,9 / p95 42,6 ms, 41 % des images ≤ 33,3 ms, GPU p50 33,9 ms.

### Lecture

- **`r.VolumetricCloud=0` : −12 ms GPU p50, p95 24 ms, 99,9 % ≤ 33 ms.** C'est le seul poste qui compte : les nuages volumétriques coûtent environ un tiers de l'image. **Changement visible** (ciel sans nuages, éclairage plus plat et plus clair, voir `stills/rue-ref.jpg` et `stills/rue-volcloud0.jpg`) : **non appliqué**, à décider.
- Tous les autres réglages sont dans le bruit (≤ 0,9 ms, comme l'écart entre les deux références) : ray tracing, VSM (SMRT 3 + biais 0,5, ou VSM coupées), Lumen (mesh SDF, réflexions /2, screen probes /64, cartes 1/16, réflexions translucides), brouillard volumétrique, post-traitements réduits. Aucun gain à cumuler, donc pas de mesure cumulée de 600 s ni de nouvelle valeur dans DefaultGame.ini / DefaultEngine.ini.
- Non fait faute de temps dans le créneau : profilage éditeur (stat gpu, ProfileGPU) sur 3 points.

### Options pour décision

1. Nuages moins chers sans les couper (à mesurer : `r.VolumetricCloud.ViewRaySampleMaxCount`, `r.VolumetricRenderTarget.Scale`, `r.VolumetricCloud.ShadowMap=0`), avec captures avant/après.
2. Couper les nuages volumétriques sur m1 et les remplacer par un ciel peint / nuages 2D (gain mesuré −12 ms, change l'image).
3. Ne rien changer sur m1.

## English

**Build**: Shipping 0.3, commit 95e2aed (map extended to the Rhône) + the `-GfxCVars` argument (this branch). m1 profile, 1728×1117 window, 48.3 % screen percentage. M1 Pro MacBook Pro.
**Conditions**: Mac rebooted 15 min earlier; Spotlight was indexing (load 25 → 2 during the sweep). Not comparable with earlier days; only the gaps between today's runs matter.
**Method**: one 60 s run per setting on the demo route, 5 s warm-up excluded, one setting at a time through `-GfxCVars=`. Two references (first and last): 0.9 ms GPU p50 apart = measurement noise.

**Why `-GfxCVars`**: the Shipping build ignores `-ExecCmds` and `-DPCVars` (checked: `t.MaxFPS 15` had no effect). `-GfxCVars=a=1,b=2` applies cvars on top of the profile (checked: `t.MaxFPS=15` → 66.7 ms).

### Results

Same table as above.

### Reading

- **`r.VolumetricCloud=0`: −12 ms GPU p50, p95 24 ms, 99.9 % ≤ 33 ms.** The only cost that matters: volumetric clouds take about a third of the frame. **Visible change** (cloudless sky, flatter and brighter lighting, see the two stills): **not applied**, needs a decision.
- Every other setting is within noise (≤ 0.9 ms). Nothing to stack, so no 600 s stacked run and no new value in the ini files.
- Not done within the slot: editor profiling (stat gpu, ProfileGPU) at 3 spots.

### Options for a decision

1. Cheaper clouds without removing them (to measure: `r.VolumetricCloud.ViewRaySampleMaxCount`, `r.VolumetricRenderTarget.Scale`, `r.VolumetricCloud.ShadowMap=0`), with before/after stills.
2. Turn volumetric clouds off on m1 and use a painted sky / 2D clouds instead (measured −12 ms, changes the image).
3. Leave m1 as it is.
