# ADR-0006 : Audio du jeu (mixage, pas, véhicules, sirènes)

*English version: [adr-0006-audio-mix.md](adr-0006-audio-mix.md)*

## Statut

Proposé le 10/11/2026. Code écrit dans le cloud, vérifié hors d'Unreal par la
CI (règles des pas, sirènes, crissement, banques générées) ; reste à compiler
et à mesurer sur le Mac.

## Date

2026-11-10

## Décideur

nic0lasm (chef de projet)

## Résumé

L'audio devient un chantier continu (roadmap v3, ligne « Audio » de
l'inventaire des systèmes), et non une passe de fin. Le mixage et toutes les
règles vivent en C++ et dans `Config/DefaultGame.ini` ; les sons restent hors
de Git, générés d'abord par nos scripts, puis remplacés dossier par dossier
par des prises de son (pack Sonniss GDC, CC0). Rien ne change dans les
classes des autres systèmes ni dans la carte : un sous-système monde donne
leurs sons à ce qui apparaît.

## Décisions

1. **Mixage construit dans le code** (`UGTPAudioMix`, un par instance de
   jeu) : une classe de son par catégorie (Master > Effets > Pas, Véhicules,
   Impacts, Sirènes ; Master > Ambiance, Musique, Voix, Interface), un mix
   utilisateur qui porte les volumes par catégorie (futur menu d'options), un
   mix « au volant » (ambiance de rue à 60 % et filtrée à 6 kHz quand le
   joueur conduit), des groupes de concurrence et des préréglages
   d'atténuation par catégorie. Aucun asset binaire de mixage à fusionner :
   toutes les valeurs sont dans `[/Script/GranTheftPraline.GTPAudioSettings]`.
2. **Banques trouvées par dossier**, pas par référence :
   `/Game/Sound/SFX/Footsteps/<Surface>/`, `Land/<Surface>/`,
   `Impacts/<Type>/<Light|Medium|Heavy>/`, `Impacts/Glass/`. Déposer des
   prises de son dans un dossier remplace les sons générés sans toucher au
   code.
3. **Pas détectés par les os des pieds**, pas par des notifications
   d'animation : fonctionne avec le motion matching de GASP et avec les cycles
   de marche de la foule. Surface : matériau physique s'il y en a un, sinon
   mots-clés du nom du matériau, sinon, sur le sol de la ville (un seul
   matériau piloté par un masque), une grille de 50 cm tirée du même masque
   (`tools/audio/surface_map.py` : asphalte ou pierre aujourd'hui). Règle :
   **le sol sonne comme il se voit** ; le gravier de Bellecour ou l'herbe
   viendront avec le matériau de sol qui les peint (`--areas`, `--vegetation`).
4. **Sons procéduraux quand le son suit le jeu en continu** : le moteur
   (ADR-0003), le crissement des pneus (glissement latéral et dérapage des
   roues Chaos) et la sirène. « Deux tons » de la police française,
   la 435 Hz / ré 580 Hz, 0,55 s chacun, plus un « yelp » de poursuite ;
   effet Doppler calculé dans l'oscillateur.
5. **Banques de sons ponctuels générées d'abord**
   (`tools/audio/make_sfx.py` : 8 surfaces × 8 pas et 3 réceptions de saut,
   5 types d'impact × 3 forces × 3, verre), originales et sous MIT : le jeu a
   du son avant tout téléchargement.
6. **Interrupteur** : `-Audio=0` (ou `-NoGameAudio`) n'ajoute rien de cet ADR,
   pour les mesures A/B ; une ligne `[Audio]` le dit au démarrage.
   `-AudioStats` écrit `[Audio] steps=… impacts=… sirens_on=…` chaque seconde
   pour les tests d'endurance.

## Interfaces pour les autres systèmes

| Système | Ce qui se passe | Changement de son côté |
|---|---|---|
| Joueur, piétons (ADR-0005) | `UGTPFootstepComponent` ajouté à tout `ACharacter` et `AGTPPedestrian` | aucun (caché ou attaché = silencieux) |
| Véhicules, trafic (ADR-0003) | `UGTPVehicleAudioComponent` (impacts, crissement) sur tout `AWheeledVehiclePawn` ; son moteur mis dans la classe Véhicules | aucun |
| Police (ADR-0004) | `UGTPSirenComponent::SetSiren(Voiture, EGTPSirenMode::TwoTone / Yelp / Off)` | une ligne là où la rampe s'allume |
| Menu d'options (plus tard) | `UGTPAudioMix::SetCategoryVolume(Catégorie, Volume)` | — |

## Budget de performance (M1, convenu avec le chantier optimisation)

0,3 ms de game thread, 10 % d'un cœur pour le thread audio, 0 ms GPU. Voix :
10 pas, 6 impacts, 3 sirènes, 4 crissements au plus, la plus lointaine puis
la plus ancienne coupée ; une limite globale de 48 voix relève du chantier
optimisation (`AudioMaxChannels`). Les pas ne sont pas cherchés au-delà de
25 m, le crissement n'est pas calculé au-delà de 60 m (20 Hz). En cas de
dépassement du game thread, on baisse d'abord les sondes de sol par marcheur.

## Conséquences

- Positif : du son pour chaque système dès la 0.3, aucun asset binaire dans
  Git, une seule convention de dossiers pour les sons générés et enregistrés,
  mesurable avec et sans.
- Négatif : des sons ponctuels synthétisés sont une base de travail, pas la
  qualité d'un GTA : des prises de son doivent remplacer les pas et les chocs
  avant la 1.0. La grille de surfaces ne connaît que la rue de la tranche ;
  ailleurs, on retombe sur les noms de matériaux et sur la pierre.
- Risques : les classes de son créées au lancement doivent s'enregistrer
  auprès du périphérique audio (à vérifier sur le Mac) ; la hauteur des os de
  pied demande un étalonnage (`gtp.Audio.DebugFeet 1`) ; GASP joue peut-être
  déjà ses propres pas (à vérifier, et couper l'un des deux).

## Hors de cette étape

Réverbération des rues encaissées et occlusion (0.16), roulement des pneus
selon le sol, frottements de carrosserie, klaxon, portières et autoradio,
radio de la police, voix, armes, météo (pluie au sol et sur la voiture, 0.6),
musique.
