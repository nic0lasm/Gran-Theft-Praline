# Audio du jeu

*English version: [audio-design.md](audio-design.md). Décision : [ADR-0006](../adr/adr-0006-audio-mix.fr.md).*

## Ce qu'on entend en 0.3

| Son | Source | Code |
|---|---|---|
| Ambiance de rue (boucle 120 s) | synthèse hors ligne, `tools/audio/make_street_ambience.py` | `AGTPPlayerController` |
| Moteur des voitures | synthèse temps réel (régime, charge) | `UGTPEngineSoundComponent` (ADR-0003) |
| Pas du joueur et des piétons, selon le sol | banques `Footsteps/<Surface>` | `UGTPFootstepComponent` |
| Réception après un saut ou une chute | banques `Land/<Surface>` | idem |
| Chocs de voiture (voiture, poteau, mur, personne, mobilier) et verre | banques `Impacts/...` | `UGTPVehicleAudioComponent` |
| Crissement des pneus | synthèse temps réel (glissement) | idem |
| Sirène de police « deux tons » et « yelp », avec Doppler | synthèse temps réel | `UGTPSirenComponent` |

Au volant, l'ambiance baisse (60 %) et perd ses aigus (6 kHz) : on est dans
l'habitacle.

## Mixage

Catégories (classes de son créées au lancement) : Master > Effects >
Footsteps, Vehicles, Impacts, Sirens ; Master > Ambience, Music, Voice,
Interface. Volumes, limites de voix, distances d'atténuation, seuils
d'impact, hauteur de pose du pied : section `[/Script/GranTheftPraline.GTPAudioSettings]`
de `Config/DefaultGame.ini`. Les sons lointains perdent leurs aigus (filtre
selon la distance) comme dans une rue.

## Surfaces

Ordre de décision au point touché : matériau physique (nommé dans les
réglages du projet ou par son nom, `PM_Gravel`), puis mots-clés du matériau
(`Wood`, `Metal`, `Steel`, `Cobble`, `Grass`, `Gravel`…), puis, sur le sol de
la ville, la grille `Content/Sound/SurfaceMap/<rue>.gtpsurf`, puis la pierre.

La grille reprend le masque du sol (`tools/city/ground_mask.py`) : chaussée =
asphalte, trottoirs, bordures et rues piétonnes = pierre. Règle : **le sol
sonne comme il se voit**. Le gravier rouge de Bellecour, l'herbe des
squares (`--vegetation`) ou des pavés (`--areas`) s'ajoutent quand le
matériau du sol les montre.

## Sources de sons, gratuites d'abord

| Source | Licence | Usage prévu | Où |
|---|---|---|---|
| Nos synthèses (`make_sfx.py`, `make_street_ambience.py`) | MIT, originales | base de travail de toutes les banques | générées, hors Git |
| Pack Sonniss « GDC Game Audio Bundle » (≈ 7,5 Go) | libre de droits, usage commercial permis, sans attribution, interdiction de revendre les sons bruts (à relire au téléchargement) | pas, chocs de voiture, verre, pneus, ambiances urbaines | Mac, `GTAL/assets/audio/sonniss/`, hors Git |
| Packs CC0 (Kenney « Impact Sounds », OpenGameArt, Freesound filtré CC0) | CC0 | compléments ponctuels | Mac, hors Git |
| Prises de son à Lyon (0.16) | les nôtres | ambiances par rue | plus tard |

Chaque son enregistré qui entre dans une version est listé avec sa licence
dans `assets/SOURCES.md`. Le dépôt ne contient aucun son.

## Chaîne sur le Mac

```
python3 -I tools/audio/make_sfx.py .scratch/audio/sfx            # banques générées
python3 -I tools/audio/surface_map.py --ground <gen/rue-de-la-republique/ground>
python3 -I tools/audio/stage_recorded.py picks.csv .scratch/audio/sfx   # prises de son choisies (facultatif)
UnrealEditor-Cmd GranTheftPraline.uproject -run=pythonscript \
    -script="tools/audio/unreal/import_sounds.py .scratch/audio/sfx"
```

`picks.csv` associe un fichier enregistré à une banque
(`/chemin/Gravel_Walk_03.wav,Footsteps/Gravel`). Pour passer une banque
entière aux prises de son, supprimer d'abord son dossier dans l'éditeur.

## Mesure et réglage

- `-AudioStats` : une ligne `[Audio] steps=… impacts=… sirens_on=… skids_on=… walkers=…` par seconde.
- `-Audio=0` : rien de l'ADR-0006 (mesure A/B).
- `gtp.Audio.DebugFeet 1` : hauteur des pieds du joueur et chaque pas avec sa surface (étalonnage de `FootPlantHeight`).
- Hors moteur, en CI : `tools/audio/audio_logic_test.cpp` + `check_synths.py`
  (règles des pas, sirènes, crissement), `tools/audio/tests` (banques, grille,
  prises de son).
