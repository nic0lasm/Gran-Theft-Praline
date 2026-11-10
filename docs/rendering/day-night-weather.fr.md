# Cycle jour/nuit et météo

*Version anglaise : [day-night-weather.md](day-night-weather.md)*

Décision : [ADR-0008](../adr/adr-0008-day-night-weather.fr.md). Valeurs
artistiques : [bible artistique](../art/art-bible.fr.md), sections 3 et 9.
Budget : [ADR-0002](../adr/adr-0002-performance-budget.fr.md).

## 1. Principe

Le sous-système de monde C++ `UGTPEnvironmentSubsystem`
(`Source/GranTheftPraline/*/GTPEnvironmentSubsystem.*`) pilote l'éclairage
**à l'exécution**. Aucune map n'est modifiée : il reprend le soleil, le ciel,
le brouillard et les nuages de la map, et ajoute ses propres composants sur un
acteur transitoire `GTP_Environment` (lune, post-traitement, couche de pluie,
lumières de nuit, brumes de Saône). Une map sans lumière directionnelle n'est
pas touchée ; `-NoEnvironment` rend l'éclairage d'origine (la fixation EV100 14
de la map sert de repli).

Tous les réglages sont dans `Config/DefaultGame.ini`, section
`[/Script/GranTheftPraline.GTPEnvironmentSubsystem]`.

## 2. Temps, soleil et lune

- Position du soleil : équations NOAA (≈ 0,1°) ; lune : série tronquée
  (≈ 1°) avec sa phase. Observateur : 45,7597 N, 4,8320 E (place Bellecour).
- Heure locale de Lyon avec heure d'été européenne (dernier dimanche de mars
  et d'octobre à 1 h UTC).
- Départ : 15 octobre 2026, 14 h (automne, bible 3.3). Vitesse par défaut
  30 : une journée dure 48 minutes.
- Le soleil ne tourne que par pas de 0,1° pour garder le cache des ombres
  virtuelles ; il cesse de projeter des ombres sous l'horizon et s'éteint
  sous −10°. La lune (sans ombre, lumière d'atmosphère 1) prend le relais
  sous −4°, à 0,4 lux × phase × (1 − 0,85 × nuages).

## 3. Exposition

Histogramme, bornes EV100 calculées en continu : cible selon la hauteur du
soleil (courbe `ExposureCurve`) plus le décalage de la météo, ± 0,5 EV.
Composant de post-traitement non borné, priorité 1000, au-dessus du volume de
la map. Adaptation 2 EV/s vers le clair, 1 EV/s vers le sombre.

| État de la bible | Soleil | Cible obtenue | Plage de la bible |
| ---- | ---- | ---- | ---- |
| Midi (21 juin, 13 h 30) | 67,6° | 14,5 | 13,5 – 15 |
| Heure dorée (15 oct., 18 h 15) | ≈ 6° | 11,5 | 10 – 12,5 |
| Pluie (9 nov., 11 h) | 24,7° | 13,45 − 2,4 = 11,05 | 10,5 – 12,5 |
| Nuit (15 janv., 22 h) | −48° | 3,5 | 2 – 6 |

Balance des blancs 6500 K le jour, 5400 K la nuit (chaude sans virer au
sépia). Saturation selon la météo (0,8 sous la pluie). `tools/environment/environment_model.py`
refait ces calculs hors Unreal et les tests vérifient chaque état contre la bible.

## 4. Nuit

- **Lumières** : fichier `Content/Environment/Data/NightLights.csv` généré
  par `tools/environment/build_night_lights.py` (hors Git, positions
  OpenStreetMap) : un point lumineux par lampadaire W1-04 (lanterne à
  4,10 m, 3500 lm, 3000 K, rayon 18 m), une lumière rectangulaire par
  vitrine (2,60 m, 60 cm devant la façade, inclinée de 25°, 1200 lm,
  4000 K, rayon 9 m), une applique W14-43 quand l'habillage en pose.
  S'y ajoutent les acteurs de la map tagués `GTP_StreetLight` ou
  `GTP_ShopLight`.
- **Budget** : toutes les 0,2 s, seules les N lumières les plus proches de la
  caméra sont allumées (celles de derrière comptent 2,5 fois plus loin), les
  plus proches capables d'ombre en projettent, fondu sur le dernier
  cinquième de la distance.

  | Profil | Lumières | Avec ombres | Distance | Brouillard volumétrique |
  | ---- | ---- | ---- | ---- | ---- |
  | m1 | 64 | 4 | 90 m | non |
  | fidelity | 96 | 6 | 120 m | oui |
  | ultra | 160 | 12 | 150 m | oui |

- **Voitures** : chaque véhicule (trafic, police, joueur) reçoit deux phares
  (projecteurs 12 000 cd, 5000 K, cône 32°, portée 35 m) et un feu arrière
  rouge, sans ombre ; la nuit, seules les `MaxLitCars` voitures les plus
  proches sont allumées (m1 : 4, fidelity : 6, ultra : 10).
- **Nuages** : qualité par profil (`bVolumetricClouds`, `CloudSampleScale`,
  `bCloudShadows`, `bDriveCloudCoverage`). Les nuages volumétriques coûtent
  ≈ 12 ms GPU en m1 (mesure du 2026-11-10) : en m1, la météo ne change pas
  leur couverture (le couvert passe par le soleil, le ciel et le brouillard),
  en attendant la décision sur leur qualité. Par météo, `CloudSampleScale`
  multiplie celui du profil, et `bCloudDomeWhenOvercast` affiche le dôme 2D à
  la place des nuages volumétriques par temps couvert, pluie, orage et
  brouillard (`bPreferCloudDome`). Repli bon marché : `bCloudDome`
  coupe les nuages volumétriques et affiche un dôme de nuages 2D non éclairé
  autour de la caméra (`M_CloudDome`, couverture `Cloudiness`, luminance
  calée sur l'exposition par `CloudLuminance`).
- **Allumage** : entre +2° et −4° de soleil (plus tôt sous les nuages).
- **Émissifs** : `EmissiveScale` de `MPC_Environment` passe de 1 à 20 la
  nuit ; les matériaux de vitrines et d'enseignes le multiplient.
- **Ciel** : le brouillard prend une teinte orangée de ville la nuit.

## 5. Météo

Six états qui se fondent en 60 s ; la météo automatique en tire un toutes les
3 h de jeu, pondéré pour l'automne lyonnais.

| État | Nuages | Soleil | Brouillard | Pluie | EV | Poids |
| ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| Clear | 0,15 | 1 | 0,008 | 0 | 0 | 3 |
| Cloudy | 0,55 | 0,7 | 0,012 | 0 | −0,7 | 3 |
| Overcast | 0,95 | 0,15 | 0,02 | 0 | −2,2 | 2 |
| Rain | 1 | 0,1 | 0,035 | 0,7 | −2,4 | 2 |
| Storm | 1 | 0,06 | 0,05 | 1 | −3 | 0,5 |
| Fog | 0,7 | 0,35 | 0,12 | 0 | −1,5 | 1 |

- **Sol mouillé** : `Wetness` monte avec la pluie (≈ 40 s à pluie 1) et
  sèche en ≈ 7 min ; `Puddles` suit au-delà de 0,55. Les générateurs de
  matériaux (ville, import des props, habillage) appellent
  `tools/environment/unreal/gtp_weather.py` `add_weather(mat, rôles)` en fin
  de graphe : albédo assombri (× 0,55) et rugosité 0,12 avec `Wetness`
  (moins sur les murs), flaques sur le sol plat (rugosité 0,03, bruit de
  6 m), émissif × `EmissiveScale`. Le mouillé survit ainsi aux régénérations ;
  la collection et la texture de flaques sont créées au premier appel.
  En attendant, `GTP_ENV_WET=on` applique les mêmes nœuds aux matériaux
  existants (`M_City_Ground`, `M_City_Shell`, `M_City_Kit`,
  `M_Master_Surface`, `M_Sign_Fascia`), une seule fois par matériau.
- **Pluie** : cylindre translucide éclairé de 24 m autour de la caméra, deux
  couches de traînées qui défilent, opacité × `RainIntensity`, fondu de
  profondeur.
- **Brume de Saône** : six volumes de brouillard locaux le long des quais
  de la Presqu'île (`RiverFogCentres`, précision ≈ 50 m, à recaler sur la
  map étendue), au poids de l'état plus la brume du matin (5 h – 10 h,
  pic à 7 h 30, d'octobre à mars).

## 6. Commandes

Ligne de commande : `-TimeOfDay=22 -Date=2027-01-15 -Weather=Rain
-TimeScale=0` (0 fige le temps pour mesurer), `-Ambience=<noon|golden|dusk|night|rain|night-rain|fog>`
(ambiance de test figée, définie dans `Ambiences`), `-CloudDome=1` (dôme de nuages 2D à la place des nuages volumétriques, 0 = jamais),
`-DayNight=0` ou `-NoEnvironment` (coupe le système). Au démarrage, une ligne `daynight: on | …` ou `daynight: off` sort sur stdout, Shipping compris.

Enchaînement sans intervention : `tools/environment/setup-environment-mac.sh`
(sortie non nulle en cas d'erreur, rapport JSON), puis le packaging, puis
`GAME_ARGS="-Ambience=night" tools/build/test-build-mac.sh` ou
`tools/environment/measure-ambiences-mac.sh`.

Console (hors Shipping) : `gtp.Time 18.25`, `gtp.Date 2026-06-21`,
`gtp.TimeScale 600`, `gtp.Weather Rain 30`, `gtp.EnvStatus`.

## 7. Mise en place sur le Mac et mesure

1. `tools/environment/setup-environment-mac.sh` (après l'import de la ville) :
   fichier des lumières, `MPC_Environment`, textures procédurales, pluie,
   matériaux mouillés seulement avec `GTP_ENV_WET=on`. Rapport `Saved/Environment/setup_report.json`, avec
   les paramètres du matériau de nuages à reporter dans
   `CloudCoverageParameters`.
2. Compiler et packager (`tools/build/package-mac.sh`).
3. `tools/environment/measure-ambiences-mac.sh <version>` : sept ambiances
   figées (midi, heure dorée, crépuscule, nuit, pluie, nuit de pluie,
   brouillard), 120 s de mesure et 20 s de film chacune, tableau
   `Builds/<version>/evidence/ambiences-<date>.md`.

Critères (profil m1) : p95 ≤ 42 ms dans chaque ambiance ; nuit ≤ 3 ms GPU
de plus que le jour ; pluie ≤ 1 ms ; nuages, ciel et brouillard de jour
≤ 1,5 ms (budgets donnés par le fil optimisation, à confirmer).

## 8. Limites connues

- Pas encore d'éclaboussures ni de ruissellement, pas de son de pluie, pas de
  reflets d'eau dynamiques ; la pluie traverse les auvents.
- Les noms des paramètres de couverture du matériau de nuages natif sont à
  vérifier sur le Mac (rapport de l'étape 1).
- Pas de lampes allumées dans les têtes de lampadaires (émissif) tant que le
  modèle W1-04 n'a pas de matériau de vitre séparé.
