# Audio du jeu

*English version: [audio-design.md](audio-design.md). Décision : [ADR-0009](../adr/adr-0009-audio-mix.fr.md).*

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

| Lit d'ambiance : rue piétonne, quai du Rhône, pont Lafayette, nuit, pluie (rue et toit de voiture), foule | boucles `make_ambiences.py`, grille de zones `zone_map.py` | `UGTPAmbienceBed` |
| Cris des passants : blessé, peur, sursaut | banques `Voices/<Scream|Hurt|Gasp>` (`make_voices_ui.py`) | `UGTPStreetReactions` (écoute `OnPedestrianHit` / `OnPedestrianScared`) |
| Radio de police quand le joueur est recherché : alerte à chaque étoile, fin de poursuite, bavardage | banques `Radio/<Chatter|Alert|Clear>` | même |
| Interface : menus, carte, étoiles | banques `UI/<Nom>` | `UGTPAudioMix::PlayUI(Monde, EGTPUISound::Confirm)` |

Au volant, l'ambiance baisse (60 %) et perd ses aigus (6 kHz) : on est dans
l'habitacle.

## Mixage

Catégories (classes de son créées au lancement) : Master > Effects >
Footsteps, Vehicles, Impacts, Sirens ; Master > Ambience, Music, Voice,
Interface. Volumes, limites de voix, distances d'atténuation, seuils
d'impact, hauteur de pose du pied : section `[/Script/GranTheftPraline.GTPAudioSettings]`
de `Config/DefaultGame.ini`. Les sons lointains perdent leurs aigus (filtre
selon la distance) comme dans une rue.

## Lit d'ambiance

La boucle de rue de jour joue toujours ; d'autres boucles se fondent par-dessus
en 2 s selon l'endroit et le monde :

| Condition | Couche | Boucle de rue |
|---|---|---|
| zone « piétonne » (rue de la République, masque G) | `SW_Amb_Pedestrian` : voix, pas, moineaux | 45 % |
| à moins de 45 m du Rhône | `SW_Amb_Quay` : fleuve, vent, trafic du quai, mouettes | 35 % |
| au-dessus de l'eau (pont Lafayette) | `SW_Amb_Bridge` : vent, voitures proches, joints de dilatation | 20 % |
| nuit (`CityLights` du MPC_Environment, ADR-0008) | `SW_Amb_Night` : ville sourde, rares voitures | × 30 % |
| pluie (`RainIntensity`) | `SW_Amb_Rain`, ou `SW_Amb_RainCar` (toit) en voiture | — |
| piétons visibles à moins de 25 m | `SW_Amb_Crowd`, plein à 10 piétons | — |

Toutes les boucles ont le même niveau (RMS −24 dBFS), les poids de
`DefaultGame.ini` font le mixage. Sans grille de zones ni paramètres météo,
seule la rue joue.

## Moteurs par modèle

Le synthé de moteur de la 0.3 reçoit un profil par modèle (`EngineProfiles` de
`DefaultGame.ini`, choisi par un mot du nom de classe) : nombre de cylindres,
plage de régime, résonance, cliquetis diesel, sifflement et soupir du turbo.
La plage de régime de la physique Chaos est ramenée sur celle du vrai moteur,
et la charge est coupée pendant chaque changement de rapport.

| Modèle | Moteur | Caractère |
|---|---|---|
| Clio V | TCe 90, 1.0 3 cylindres turbo essence | ronronnement de 3 cylindres, sifflement léger |
| 208 II | PureTech 100, 1.2 3 cylindres turbo essence | plus rugueux, plus grave au ralenti |
| Kangoo II | dCi 90, 1.5 4 cylindres turbo diesel | cliquetis au ralenti, monte moins haut (4 500 tr/min) |
| autres | 1.6 4 cylindres essence | neutre |

## Réactions de la rue

- Un passant renversé crie (`Voices/Hurt`). Un passant effrayé crie ou sursaute une fois sur deux, avec 0,35 s au moins entre deux cris : une foule qui fuit ne devient pas un mur de cris. Chaque voix a sa hauteur (×0,82 à ×1,28).
- Un choc moyen ou fort diffuse `UGTPAudioWorld::OnLoudSound(lieu, rayon, cause)`, avec un rayon de 8 m (moyen) ou 20 m (fort). Les piétons y branchent `ScareAround` par une lambda.
- Radio de police, en 2D et sans paroles : tonalités d'alerte puis un appel à chaque étoile gagnée, appel de fin quand la poursuite s'arrête. Entre les deux, un bavardage toutes les 7 à 16 s, plus serré avec les étoiles. Les sons d'étoile du HUD (`StarUp`, `StarDown`) partent au même moment.

## Intérieurs et missions (0.9)

`tools/audio/make_places.py` (graine 17, 32 fichiers, aucun enregistrement) écrit trois dossiers :
- `Interior/` : boucles sans couture de 12 s (RoomTone, Fryer, Grill, Hood, Cold, Shake, Drinks) ;
- `Shop/` : ouverture et fermeture de caisse, pièces, billets, imprimante à tickets, bip de carte,
  minuterie de friteuse, panier plongé, carillons de commande et de porte, buzzer de porte arrière,
  coffre ;
- `Mission/` : disponible, départ, objectif, point de passage, réussite, échec, récompense,
  compte à rebours.

`UGTPPlaceAudio`, tenu par le monde audio, les joue :

- Quand un niveau chargé dont le nom contient `Interior` devient visible (ADR-0012), ses
  composants sont parcourus une fois.
  - Les émetteurs de boucle viennent d'une étiquette `GTP_AudioLoop=<Boucle>`, sinon d'un nom
    contenant fryer, grill, hood, fridge/freezer/cold, shake ou drink. Une même boucle à moins
    de 1,5 m ne compte qu'une fois.
  - Les salles viennent des composants étiquetés `GTP_AudioRoom`, sinon des bornes du niveau.
  - Les caisses viennent d'un nom contenant till, register, kiosk ou pos.
- Un groupe d'au plus 6 voix en boucle (`MaxRoomVoices`) suit les émetteurs les plus proches, à
  moins de 25 m. Ce groupe est créé une seule fois : charger ou décharger un intérieur ne crée ni
  ne détruit aucun objet sonore.
- Dans une salle, le mix intérieur `SM_GTP_Indoor` règle les sons de la rue :
  - la boucle de rue et le lit passent à 0,3 ;
  - la circulation passe à 0,35 et les sirènes à 0,7 ;
  - le tout est filtré au-dessus de 1,5 kHz.
  L'ambiance de la salle monte en fondu, et le carillon de porte sonne à l'entrée.
- Tant que les boucles de travail des employés n'appellent pas `UGTPPlaceAudio::PlayShop`, une
  petite vie de boutique tourne près des caisses et des friteuses, toutes les 3 à 9 s : bips de
  carte, tickets, carillons de commande, pièces, minuteries. `-ShopLife=0` la coupe.
- `UGTPPlaceAudio::PlayMission` joue un jingle en 2D par la classe Interface.

Réglages : `[/Script/GranTheftPraline.GTPPlaceAudio]` dans `DefaultGame.ini`. Coût : au plus
6 voix en boucle et une ambiance de salle, environ 0,06 % d'un cœur par voix d'échantillon selon
l'estimation, moins de 0,5 % au total.

## Budget de foule (50 piétons, 25 voitures)

Toutes les 0,25 s, `UGTPAudioWorld` classe les moteurs allumés et les
marcheurs par distance à l'écoute. Seuls les 6 moteurs les plus proches et à
moins de leur distance de silence (45 m) sont calculés ; les autres
s'éteignent en fondu. Seuls les 8 marcheurs visibles les plus proches et à
moins de 25 m cherchent leurs pas ; la couche foule du lit couvre les autres.
La voiture du joueur et le joueur passent toujours en premier. Les couches du
lit muettes sont arrêtées au lieu de jouer à zéro. Les voitures lointaines
regardent leurs pneus 4 fois par seconde au lieu de 20.

| Poste | Sans le budget | Avec |
|---|---|---|
| Synthés de moteur, fil audio (mesuré hors Unreal, cœur x86 à 2,1 GHz) | 25 × 0,44 % = 11 % d'un cœur | 6 × 0,44 % = 2,6 % |
| Marcheurs qui cherchent leurs pas, fil de jeu | jusqu'à 50 | 8 |
| Boucles du lit décodées et mixées | 8 | la rue et les couches utiles (2 à 4) |
| Voix au plus | ~70 | 42 : pas 10, chocs 6, sirènes 3, crissements 4, voix 4, radio 1, interface 3, moteurs 6, lit 5 |

Coût estimé sur le fil de jeu à 30 i/s avec cette foule : environ 0,15 ms
(budget 0,3 ms), à confirmer sur le Mac avec `-AudioStats` ; la ligne de log
donne `heard=` (marcheurs) et `engines=audibles/total`. Réglages
`MaxEngineVoices`, `MaxStepWalkers` et `BudgetInterval` dans `DefaultGame.ini`.

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
| Nos synthèses (`make_sfx.py`, `make_voices_ui.py`, `make_ambiences.py`, `make_street_ambience.py`) | MIT, originales | base de travail de toutes les banques | générées, hors Git |
| Pack Sonniss « GDC Game Audio Bundle » (≈ 7,5 Go) | libre de droits, usage commercial permis, sans attribution, interdiction de revendre les sons bruts (à relire au téléchargement) | pas, chocs de voiture, verre, pneus, ambiances urbaines | Mac, `GTAL/assets/audio/sonniss/`, hors Git |
| Packs CC0 (Kenney « Impact Sounds », OpenGameArt, Freesound filtré CC0) | CC0 | compléments ponctuels | Mac, hors Git |
| Prises de son à Lyon (0.16) | les nôtres | ambiances par rue | plus tard |

Chaque son enregistré qui entre dans une version est listé avec sa licence
dans `assets/SOURCES.md`. Le dépôt ne contient aucun son.

## Chaîne sur le Mac

Sans intervention, en deux temps du passage groupé :

```
tools/audio/mac_audio_pass.sh prepare        # éditeur fermé, avant le package
tools/audio/mac_audio_pass.sh check 0.4      # après le package
```

`prepare` génère les banques et la grille, les importe sans interface et dit
si GASP joue déjà des pas (`.scratch/audio/gasp_foley.json` ; si oui, mettre
son dossier dans `MutedSoundFolders`). `check` fait une course filmée avec le
son (`-DemoRoute -TestSiren -DebugFeet -RecordAudio`, puis
`capture-with-sound.mov`), deux courses A/B (`-Audio=0` et avec), et écrit
`audio-report.md` (cadence des pas, hauteur de pose conseillée, coût du game
thread face au budget de 0,3 ms).

Détail des étapes :

```
python3 -I tools/audio/make_sfx.py .scratch/audio/sfx            # banques générées
python3 -I tools/audio/surface_map.py --ground <gen/rue-de-la-republique/ground>
python3 -I tools/audio/stage_recorded.py picks.csv .scratch/audio/sfx   # prises de son choisies (facultatif)
UnrealEditor-Cmd GranTheftPraline.uproject -run=pythonscript \
    -script="tools/audio/unreal/import_sounds.py .scratch/audio/sfx"
```

`tools/audio/pick_sonniss.py <bibliothèque> picks.csv` prépare la liste à
partir des noms de dossiers et de fichiers (pas + surface, voiture + crash,
verre + bris…) et coupe les enregistrements de marche en pas isolés ; un
rapport `picks.csv.txt` dit ce que reçoit chaque banque, à écouter avant
l'import. `picks.csv` associe un fichier enregistré à une banque
(`/chemin/Gravel_Walk_03.wav,Footsteps/Gravel`). Pour passer une banque
entière aux prises de son, supprimer d'abord son dossier dans l'éditeur.

## Mesure et réglage

- `-AudioStats` : une ligne `[Audio] steps=… impacts=… sirens_on=… skids_on=… walkers=…` par seconde.
- `-Audio=0` : rien de l'ADR-0009 (mesure A/B).
- `-RecordAudio[=s]` (`-RecordAudioDir=`) : enregistre la sortie du jeu en WAV, au même moment que `-CaptureVideo` ; `-TestSiren` : la voiture de trafic la plus proche fait sonner une sirène (deux tons 12 s, yelp 4 s).
- `-DebugFeet` (ou `gtp.Audio.DebugFeet 1`) : hauteur des pieds du joueur et chaque pas avec sa surface (étalonnage de `FootPlantHeight`).
- Hors moteur, en CI : `tools/audio/audio_logic_test.cpp` + `check_synths.py`
  (règles des pas, sirènes, crissement), `tools/audio/tests` (banques, grille,
  prises de son).
