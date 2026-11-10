# Nuages volumétriques sur M1 — 2026-11-10 / Volumetric clouds on M1 — 2026-11-10

## Français

Même build et même méthode que [perf-m1-sweep.md](perf-m1-sweep.md) : Shipping 0.3 (95e2aed) avec `-GfxCVars`, profil m1, parcours démo, 60 s par configuration, un réglage à la fois. Charge stable entre 2 et 4 pendant la série. Bruit de mesure entre deux références ce jour : environ 0,9 ms.

Captures : parcours démo filmé à 1 image/s, images à t = 5, 20 et 35 s, en trois points de la rue (`stills/clouds/<config>-00000|00015|00030.jpg`). Chaque image contient, de gauche à droite : la référence, la configuration et leur différence ×4. Le parcours ne repasse pas exactement au même endroit d'une passe à l'autre, donc la différence fait ressortir aussi les bords décalés. Pour juger, on regarde surtout la luminosité globale et les ombres.

| Run | CVar | Frame p50 | Frame p95 | % <= 33 ms | GPU p50 | Δ GPU p50 |
|---|---|---|---|---|---|---|
| ref | `—` | 33.5 | 42.0 | 44.6 | 33.6 | +0.0 |
| vrt_mode3 | `r.VolumetricRenderTarget.Mode=3` | 36.5 | 46.0 | 12.7 | 36.6 | +3.0 |
| vrt_scale05 | `r.VolumetricRenderTarget.Scale=0.5` | 33.2 | 42.5 | 52.1 | 33.5 | -0.1 |
| rays384 | `r.VolumetricCloud.ViewRaySampleMaxCount=384` | 33.2 | 42.0 | 51.4 | 33.2 | -0.4 |
| shadowrays | `r.VolumetricCloud.ShadowViewRaySampleMaxCount=40` | 32.7 | 41.3 | 61.5 | 32.7 | -0.9 |
| skyao0 | `r.VolumetricCloud.SkyAO=0` | 33.2 | 42.2 | 51.6 | 33.5 | -0.1 |
| shadowmap0 | `r.VolumetricCloud.ShadowMap=0` | 25.1 | 33.6 | 94.6 | 24.9 | -8.6 |

### Lecture

- **`r.VolumetricCloud.ShadowMap=0` : −8,6 ms, 95 % des images ≤ 33 ms.** L'essentiel du coût des nuages vient de leur ombre projetée sur la ville. Mais l'image change visiblement : le sol et les façades deviennent nettement plus clairs, l'éclairage s'aplatit (`shadowmap0-00015.jpg`, `shadowmap0-00030.jpg`). C'est le même effet que `r.VolumetricCloud=0`. **Rejeté** au titre de la règle « aucun changement visible ».
- `ShadowViewRaySampleMaxCount=40` (−0,9 ms) et `ViewRaySampleMaxCount=384` (−0,4 ms) restent dans le bruit, sans différence visible hors décalage de caméra.
- `VolumetricRenderTarget.Scale=0.5` et `SkyAO=0` ne changent rien. `VolumetricRenderTarget.Mode=3` est plus lent (+3 ms).
- **Aucune M1CVar candidate** ne donne un gain mesurable sans changer l'image. Je n'ai rien cumulé et rien écrit dans les .ini.

### Pistes pour une décision

1. Ombre des nuages moins chère sans la couper : réduire la résolution de la shadow map des nuages et la mettre à jour moins souvent (cvars `r.VolumetricCloud.ShadowMap.*`, à mesurer avec une caméra fixe).
2. Couper l'ombre des nuages sur m1 et compenser l'exposition ou la lumière pour retrouver la luminosité de la référence (−8,6 ms, demande un réglage artistique).
3. Ne rien changer.

## English

Same build and method as [perf-m1-sweep.md](perf-m1-sweep.md): Shipping 0.3 (95e2aed) with `-GfxCVars`, m1 profile, demo route, 60 s per configuration, one setting at a time. Load stable between 2 and 4 during the series. Noise between two references today: about 0.9 ms.

Stills: demo route filmed at 1 fps, frames at t = 5, 20 and 35 s, at three spots in the street (`stills/clouds/<config>-00000|00015|00030.jpg`). Each image shows, left to right: the reference, the configuration and their difference ×4. The route does not pass through exactly the same spot from one run to the next, so the difference also lights up shifted edges. To judge, look mainly at overall brightness and shadows.

Same table as above.

### Reading

- **`r.VolumetricCloud.ShadowMap=0`: −8.6 ms, 95 % of frames ≤ 33 ms.** Most of the cloud cost is their shadow cast on the city. But the image clearly changes: ground and façades get much brighter and the lighting flattens (`shadowmap0-00015.jpg`, `shadowmap0-00030.jpg`), the same effect as `r.VolumetricCloud=0`. **Rejected** under the "no visible change" rule.
- `ShadowViewRaySampleMaxCount=40` (−0.9 ms) and `ViewRaySampleMaxCount=384` (−0.4 ms) stay within noise, with no visible difference beyond the camera offset.
- `VolumetricRenderTarget.Scale=0.5` and `SkyAO=0` change nothing; `VolumetricRenderTarget.Mode=3` is slower (+3 ms).
- **No candidate M1CVar** gives a measurable gain without changing the image. Nothing was stacked and nothing was written to the ini files.

### Leads for a decision

1. Cheaper cloud shadow without removing it: lower the cloud shadow map resolution and update it less often (`r.VolumetricCloud.ShadowMap.*` cvars, to be measured with a fixed camera).
2. Turn the cloud shadow off on m1 and compensate exposure or lighting to get the reference brightness back (−8.6 ms, needs an art pass).
3. Leave it as it is.
