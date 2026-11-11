# Mini-carte

*English version: [minimap.md](minimap.md)*

Roadmap : mini-carte (0.5), carte plein écran et GPS (0.10). Décision : [ADR-0007](../adr/adr-0007-interface-settings-minimap.fr.md).

## Aspect

Un radar rond en bas à gauche (rayon 120 px à 1080 lignes, à 36 px du coin) : routes
(principales claires et plus larges, secondaires, chemins gris-vert, tunnels estompés),
eau en bleu, contours des bâtiments discrets, un anneau, un repère nord « N » sur
l'anneau et la flèche du joueur au centre. Elle **tourne avec la caméra** : le haut est
l'endroit où le joueur regarde, comme dans GTA. Zoom : proche 120 m, moyen 220 m (par
défaut), loin 400 m de rayon (loin retire bâtiments et chemins). Activation et zoom
dans les [réglages](settings.fr.md). Elle est masquée quand un menu est ouvert (le HUD
l'est) et quand la map n'a pas de données.

Images de contrôle faites avec les vraies données, proche / moyen / loin, face au nord
rue de la République : `/mnt/project-files/ui/minimap-preview-*.png` (faites par
`tools/ui/preview_minimap.py`, mêmes maths que le jeu).

## Données

`tools/ui/minimap.py` transforme les tables de la ville (`roads.geojson`,
`water.geojson`, `buildings.geojson` de la zone : IGN BD TOPO, Licence Ouverte Etalab
2.0) en `minimap.csv` : une ligne par polyligne, `kind,width_m,points_ue_cm,name`, simplifiée
à 1,5 m et en centimètres Unreal comme les voies du trafic. Presqu'île : 3 497
polylignes, 295 Ko. Avec `--map <Map>` il est copié dans
`GranTheftPraline/Content/City/Minimap/<Map>.minimap.csv`, **non versionné** (données de
ville dérivées, comme les voies) et embarqué avec le jeu
(`DirectoriesToAlwaysStageAsUFS`).

```
python3 tools/ui/minimap.py --map L_City_RueDeLaRepublique
```

Les coordonnées sont les mètres locaux de la zone, même origine que les voies du trafic
et l'import de la ville : mini-carte et monde concordent sans décalage par map. Quand la
map s'étend (quai du Rhône, pont Lafayette) les tables de la zone la couvrent déjà ;
relancer l'outil après un changement de zone.

## Code

- `GTPMinimapCore.h` : sans type Unreal. Lecture du CSV, vue tournante
  (`FMapView::ToScreen`), découpe des segments au disque, élimination des lignes par
  boîte englobante. Testé par `tools/ui/tests/run_cpp_tests.sh`.
- `UGTPMinimapSubsystem` (`GTPMinimap.h`) : sous-système de monde, charge les données au
  démarrage de la map, dessine sur le canvas. `AGTPHUD::DrawHUD` appelle `Draw()` une
  fois en fin de fonction : c'est la seule modification du HUD.
- Pastilles pour les autres systèmes : `UGTPMinimapSubsystem::AddBlip(Actor, Couleur,
  bClampToEdge)` et `RemoveBlip(Actor)`. Voitures de police, piétons ou destination
  s'y inscrivent ; avec `bClampToEdge` le point reste sur l'anneau hors de portée.

## Place et coût

Le bas gauche est libre : panneau de conduite et aides contextuelles en bas à droite,
sous-titres en bas au centre, étoiles de la police en haut à droite. Coût : un élément
de ligne du canvas par segment visible (quelques centaines au zoom moyen), sans
texture ni render target. Le coût mesuré sur le M1 Pro ira dans `docs/ui/test-results/`
(ms CPU et GPU, mini-carte active et coupée, même parcours) ; budget demandé au fil
optimisation : 0,3 ms CPU et 0,2 ms GPU. S'il ne tient pas, repli : une texture statique
pré-rendue par map, tournée par le HUD (un seul appel de dessin).

## Carte plein écran

**M** (croix haut de la manette) ouvre une carte plein écran et met le jeu en pause ; M,
Échap ou B la ferme. Mêmes données, nord en haut, centrée sur le joueur, 700 m de haut,
avec contours des bâtiments, eau, toutes les routes, **noms de voies** (une étiquette par
nom, au milieu de sa polyligne la plus longue ; les noms viennent de la colonne BD TOPO
`nom_voie_ban`, écrite dans la colonne `name` de `minimap.csv`) et la flèche du joueur
tournée vers où il regarde. Dessinée par `UGTPMinimapSubsystem::DrawFullMap` ; le HUD lui
cède la place tant qu'elle est ouverte.

Icônes de carte : UGTPMinimapSubsystem::AddBlip(Actor, Couleur). La voiture que le joueur
prend est marquée en bleu clair (`UGTPVehicleInteractionComponent`) sur la mini-carte et la
carte plein écran ; la police ajoute ses voitures de la même façon.

## GPS (0.9, première version)

Sur la carte plein écran le curseur est la croix au centre de l'écran : déplacer avec les
flèches ou le stick gauche, **Entrée** ou **A** de la manette pose la destination, **Suppr**
ou **X** l'efface. L'itinéraire est le plus court chemin sur les lignes de routes des données
de la map (`GTPGpsCore.h`, Dijkstra sur le graphe des carrefours : Presqu'île = 1 017 routes,
755 carrefours, 0,16 ms pour 700 m) ; le départ comme la destination sont rapprochés de la
route la plus proche. Tracé en magenta sur la mini-carte et la carte plein écran, avec un repère
à la destination (sur l'anneau hors de portée), recalculé quand le joueur quitte l'itinéraire de
plus de 40 m, effacé à l'arrivée (25 m). Les routes sont à double sens pour l'instant : suivre
les sens de circulation est la 0.10 de la roadmap. `-Gps=0` le désactive. Testé hors Unreal
(`tools/ui/tests/run_cpp_tests.sh` : choix du raccourci, trajets sur une même route, accrochage,
île inaccessible, graphe vide).

## Suite (roadmap 0.10)

GPS qui suit les sens de circulation (`lanes.csv`), zoom de la carte plein écran, noms de quartiers, marqueurs d'activités.
