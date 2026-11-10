# Mobilier de rue cassable (0.4) : liste et comportements

Proposition du 2026-11-10, à valider avec le fil ragdoll et réactions physiques avant le
code. Elle couvre les objets déjà posés dans la rue de la République par
`tools/city/props.py` et `tools/city/details.py`, plus ceux du catalogue (vagues 1, 8 et 14)
qui le seront. Pour chaque objet du catalogue, on choisit un des trois comportements
ci-dessous, sur le modèle des jeux en monde ouvert : la ville réagit à une voiture lancée,
mais les immeubles et les ouvrages lourds tiennent.

## Trois comportements

| Classe | Ce qui se passe au choc | Technique proposée |
| ---- | ---- | ---- |
| **R : renversable** | L'objet part en corps rigide, roule, se renverse, peut être poussé à pied | L'instance HISM est remplacée au contact par un acteur physique (même maillage, collision simple, masse ci-dessous), qui se rendort après l'arrêt |
| **C : cassable** | Au-delà d'un seuil d'impulsion, il plie ou s'arrache à la base et tombe ; sous le seuil, il arrête ou dévie la voiture | Remplacement par un acteur physique avec une articulation à la base qui cède (seuil réglable), ou une Geometry Collection Chaos pour les seuls vitrages |
| **F : fixe** | Il arrête la voiture et l'abîme, il ne bouge pas | Rien à faire : collision bloquante actuelle |

Budget M1 proposé (à confirmer avec le fil optimisation) : 24 corps physiques réveillés au
maximum en même temps ; un objet tombé reste au sol 60 s puis disparaît hors champ, et
l'instance d'origine réapparaît quand le joueur est à plus de 80 m. Aucun objet cassable ne
projette d'ombre une fois au sol s'il fait moins de 0,5 m.

## Liste par objet

| ID | Objet | Classe | Masse (kg) | Remarque |
| ---- | ---- | ---- | ---- | ---- |
| W1-07, W1-08 | Corbeilles | R | 15 | Le sac transparent W1-07 peut lâcher des déchets W14-65 |
| W1-32, W1-33 | Conteneurs à roulettes | R | 25 / 45 | Roulent |
| W1-34 | Table de café | R | 12 | |
| W1-35, W1-36 | Chaises de terrasse | R | 4 | Volent loin : bon effet, peu coûteux |
| W1-38 | Parasol ouvert | R | 20 | Bascule avec son pied |
| W1-40, W1-41 | Ardoise, porte-menu | R | 6 | |
| W1-50, W8-29, W8-30 | Cônes, balises | R | 3 / 5 / 10 | |
| W1-51, W8-24 | Barrières de foule, en plastique | R | 15 / 8 | |
| W1-14, W1-55, W1-54 | Vélo, trottinette, deux-roues garé | R | 15 / 20 / 110 | Le deux-roues tombe sur le côté |
| W14-05, W14-79 | Laurier en pot, jardinière de terrasse | R | 40 / 60 | Lourds : glissent plus qu'ils ne volent |
| W14-78 | Chauffage de terrasse | R | 25 | |
| W14-73, W14-75, W1-60 | Cagettes, fûts, sacs-poubelle | R | 5 / 30 / 3 | |
| W1-04 | Lampadaire de style | C | 250 | Casse à la base au-delà d'environ 35 km/h ; sa lumière s'éteint (fil jour/nuit) |
| W8-01, W8-02 | Feux tricolores et piétons | C | 120 / 80 | Idem ; le feu tombé reste éteint |
| W8-05 à W8-17, W1-57 | Panneaux sur mât | C | 25 | Plient dès 15 km/h |
| W1-12 | Arceau vélo | C | 30 | Plie, ne s'arrache pas |
| W1-01 | Banc bois et fonte | C | 90 | S'arrache au-delà d'environ 40 km/h |
| W1-13 | Borne Vélo'v | C | 150 | |
| W1-24, W1-25 | Boîte aux lettres, horodateur | C | 60 / 80 | |
| W1-27 | Borne incendie | C | 80 | Arrachée : jet d'eau (effet de particules à prévoir) |
| W1-21, W1-39 | Abribus, pare-vent vitré | C (vitrage seul) | | Le verre éclate (Geometry Collection), la structure reste |
| W1-09 | Potelet anti-stationnement | F | | Les potelets de Lyon arrêtent vraiment les voitures |
| W1-17, W14-63, W14-64 | Grille d'arbre, troncs de platane | F | | Jeune arbre protégé W14-64 : C à discuter |
| W1-19, W1-22, W1-23 | Colonne d'affichage, kiosque, cabine | F | | |
| W1-30, W8-34 | Armoires techniques | F | | |
| W1-18, W1-31 | Fontaine à boire, colonne à verre | F | | |
| W13-xx, W6-xx monumentaux | Statues, fontaines, socles | F | | |
| Détails muraux et de toit | Descentes d'eau, plaques, bannes, cheminées | F | | Ils sont sur les murs, hors d'atteinte |

## Comment l'intégrer sans toucher à la carte

Les tableaux `props_placement.csv` et `details_placement.csv` gagneront une colonne
`breakable` (R, C ou F) et une colonne `mass_kg`, tirées de cette liste par catalogue. Le
script `place_props.py` posera les objets R et C dans des composants HISM marqués par un tag
(`GTP_Breakable_R`, `GTP_Breakable_C`). Un sous-système de monde C++, côté fil ragdoll,
remplacera à l'impact l'instance touchée par un acteur physique. La carte reste rejouable par
script, comme aujourd'hui.
