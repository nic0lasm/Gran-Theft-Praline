# Catalogue des assets à générer (Tripo)

*Version anglaise : [asset-catalog.md](asset-catalog.md). Roadmap : [roadmap.fr.md](roadmap.fr.md), section 5.*
*Mis à jour par le fil qui génère : statut de chaque modèle et coût réel en crédits.*

## Règles communes

- **Génération** dans Tripo Studio (interface web), en parallèle, à la
  qualité maximale. Images de référence (Flux, GPT Image ou photos de
  terrain) quand le texte seul ne suffit pas.
- **Master + variante légère** : le GLB d'origine est le master (gardé hors
  Git, stockage à décider) ; une variante légère (polygones et textures
  réduits) sert au profil M1. Voir roadmap, section 1.
- **Fidèle à Lyon** : Tripo génère les objets sans texte ni logo
  (`no text` dans le prompt) ; les vraies enseignes, marques et inscriptions
  sont posées ensuite d'après photos terrain ([bible artistique](../docs/art/art-bible.fr.md),
  section 3.5). Aucun objet copié d'un jeu existant. Prompts, échelle, pivot
  et orientation suivent la charte Tripo de la bible (section 8).
- **Échelle réelle** en mètres notée dans la colonne Taille ; pivot au sol.
- **Parcours d'un modèle** : ⬜ à générer → 🎲 généré (choix parmi les
  variantes) → 🧹 nettoyé dans Blender (échelle, pivot, LOD, collisions) →
  ✅ intégré dans Unreal et inscrit dans `assets/SOURCES.md`.
- **Exports** : `assets/tripo/<vague>/<id>.glb` (masters, hors Git) ;
  assets Unreal dans `Content/Environment/Props/<Famille>/<Asset>/`
  (bible artistique, section 7).

## Vue d'ensemble des vagues

| Vague | Pour la version | Thème | Modèles | Statut |
| ---- | ---- | ---- | ---- | ---- |
| 1 | 0.2 | Mobilier urbain de la rue de la République | 60 | ⬜ |
| 2 | 0.3 | Véhicules (ébauches) et objets de chaussée | 30 | ⬜ |
| 3 | 0.4 | Armes fictives et accessoires de combat | 25 | ⬜ |
| 4 | 0.5 | Intérieur d'une boutique d'alimentation | 40 | ⬜ |
| 5 | 0.6 | Équipement de police fictif, signalétique | 20 | ⬜ |
| 6 | 0.7 | Mobilier des places (Bellecour, Terreaux, Jacobins, République) | 50 | ⬜ |
| 7 | 0.8 | Objets portés et accessoires de piétons | 30 | ⬜ |
| 8 | 0.9 | Mobilier routier (feux, panneaux génériques, barrières) | 40 | ⬜ |
| 9 | 0.10 | Armes et équipement, suite | 30 | ⬜ |
| 10-11 | 0.11 | Intérieurs par type de boutique (vêtements, armurerie, garage, café, planques) | 120 | ⬜ |
| 12 | 0.13 | Deux-roues et mobilier d'activités | 30 | ⬜ |
| 13 | 0.14 | Ornements de façades et de monuments | 60 | ⬜ |
| 14 | 0.15 | Végétation en pot, devantures, petits détails de rue | 80 | ⬜ |
| 15 | 0.16 | Carrosseries finales et accessoires de personnages | 40 | ⬜ |
| 16-17 | 0.17 | Mobilier intérieur final (30 boutiques, 10 bâtiments, 3 planques) | 150 | ⬜ |
| | | **Total** | **≈ 805** | |

Les vagues peuvent être générées **en avance** de leur version : seul
l'ordre compte, pas la date. Coût en crédits à mesurer sur la vague 1, puis
rythme ajusté pour consommer les 25 200 crédits Studio avant expiration.

## Vague 1 : mobilier urbain de la rue de la République (pour la 0.2)

Rue piétonne haussmannienne, pavés et dalles claires, mobilier sobre gris
anthracite ou vert sombre.

| ID | Objet | Notes pour la génération | Taille (m) | Statut |
| ---- | ---- | ---- | ---- | ---- |
| W1-01 | Banc public en bois et fonte | lattes bois, piètement fonte anthracite, accoudoirs | 1,8 × 0,6 × 0,8 | ⬜ |
| W1-02 | Banc public métal moderne | tôle perforée gris anthracite | 1,8 × 0,6 × 0,8 | ⬜ |
| W1-03 | Banquette béton | bloc béton clair avec assise bois | 2,0 × 0,5 × 0,45 | ⬜ |
| W1-04 | Lampadaire de style ancien | fût fonte cannelé, lanterne quatre faces | 4,5 de haut | ⬜ |
| W1-05 | Lampadaire contemporain | mât cylindrique, tête LED plate | 6,0 de haut | ⬜ |
| W1-06 | Applique murale d'éclairage public | console fonte, lanterne | 0,9 | ⬜ |
| W1-07 | Corbeille de propreté avec sac transparent | cerclage métal, couvercle anti-pluie | 0,9 de haut | ⬜ |
| W1-08 | Corbeille de propreté cylindrique | tôle perforée vert sombre | 0,9 de haut | ⬜ |
| W1-09 | Potelet anti-stationnement | fonte, tête boule, vert sombre | 1,0 de haut | ⬜ |
| W1-10 | Borne escamotable | inox, bande réfléchissante | 0,8 de haut | ⬜ |
| W1-11 | Barrière de trottoir | tube métal, trois poteaux | 1,5 × 1,0 | ⬜ |
| W1-12 | Arceau vélo | tube inox en U | 0,8 × 0,8 | ⬜ |
| W1-13 | Station de vélos en libre-service, borne | borne générique sans logo, gris | 1,3 de haut | ⬜ |
| W1-14 | Vélo de ville générique | cadre bas, panier, gris | 1,8 de long | ⬜ |
| W1-15 | Jardinière rectangulaire avec arbuste | acier corten | 1,5 × 0,6 × 0,7 | ⬜ |
| W1-16 | Grand bac à arbre en bois | caisse bois, petit olivier | 1,2 × 1,2 × 1,0 | ⬜ |
| W1-17 | Grille d'arbre au sol | fonte, motif rayonnant | 1,5 × 1,5 | ⬜ |
| W1-18 | Fontaine à boire publique | fonte vert sombre, style ancien | 1,2 de haut | ⬜ |
| W1-19 | Colonne d'affichage culturel | cylindre, dôme, affiches fictives sans texte lisible | 3,5 de haut | ⬜ |
| W1-20 | Panneau d'information piéton (plan de quartier) | totem métal, plan illisible | 2,2 de haut | ⬜ |
| W1-21 | Abri de bus vitré | structure acier et verre, banc, sans publicité | 4,0 × 1,5 × 2,6 | ⬜ |
| W1-22 | Kiosque à journaux fermé | cabine métal, volets baissés | 2,5 × 2,0 × 3,0 | ⬜ |
| W1-23 | Cabine téléphonique désaffectée | verre et métal, vide | 1,0 × 1,0 × 2,3 | ⬜ |
| W1-24 | Boîte aux lettres de rue | métal jaune générique, sans logo | 1,2 de haut | ⬜ |
| W1-25 | Horodateur | colonne grise, écran | 1,6 de haut | ⬜ |
| W1-26 | Borne de recharge électrique | colonne blanche et grise, deux câbles | 1,5 de haut | ⬜ |
| W1-27 | Bouche d'incendie | fonte rouge | 0,8 de haut | ⬜ |
| W1-28 | Plaque d'égout ronde | fonte, motif générique | 0,7 diamètre | ⬜ |
| W1-29 | Grille d'eaux pluviales | fonte, bord de trottoir | 0,8 × 0,3 | ⬜ |
| W1-30 | Armoire électrique de rue | boîtier gris, autocollants illisibles | 1,0 × 0,4 × 1,4 | ⬜ |
| W1-31 | Conteneur à verre | cloche verte | 1,5 × 1,5 × 1,7 | ⬜ |
| W1-32 | Poubelle à roulettes 240 l | plastique gris, couvercle vert | 0,6 × 0,7 × 1,1 | ⬜ |
| W1-33 | Poubelle à roulettes 660 l | plastique gris, quatre roues | 1,3 × 0,8 × 1,2 | ⬜ |
| W1-34 | Table de terrasse de café | plateau rond, pied fonte | 0,7 diamètre | ⬜ |
| W1-35 | Chaise de terrasse en rotin | tressage deux tons | 0,5 × 0,5 × 0,85 | ⬜ |
| W1-36 | Chaise de terrasse en métal | métal pliante, vert sombre | 0,5 × 0,5 × 0,85 | ⬜ |
| W1-37 | Parasol de terrasse fermé | toile écrue sans logo | 3,0 de haut | ⬜ |
| W1-38 | Parasol de terrasse ouvert | toile écrue sans logo | 3,0 diamètre | ⬜ |
| W1-39 | Paravent de terrasse vitré | cadre métal, verre | 1,5 × 1,2 | ⬜ |
| W1-40 | Chevalet ardoise de restaurant | bois, ardoise sans texte | 0,6 × 1,0 | ⬜ |
| W1-41 | Porte-menu sur pied | métal, cadre vide | 1,4 de haut | ⬜ |
| W1-42 | Portant de vêtements devant boutique | tube chromé, quelques vêtements | 1,5 × 1,5 | ⬜ |
| W1-43 | Étal de fleuriste | tréteaux bois, seaux de fleurs | 2,0 × 0,8 × 1,0 | ⬜ |
| W1-44 | Rideau métallique de boutique fermé | lames d'acier, pour devanture | 3,0 × 3,0 | ⬜ |
| W1-45 | Store banne replié | caisson, toile unie | 4,0 de large | ⬜ |
| W1-46 | Store banne déployé | toile unie rayée sans texte | 4,0 × 1,5 | ⬜ |
| W1-47 | Climatiseur de façade | unité extérieure blanche | 0,8 × 0,3 × 0,6 | ⬜ |
| W1-48 | Balcon fer forgé (garde-corps) | motif volutes, pour façades | 2,0 × 1,0 | ⬜ |
| W1-49 | Barrière de chantier mobile | grillage, socle béton | 3,5 × 2,0 | ⬜ |
| W1-50 | Cône de signalisation | orange, bandes réfléchissantes | 0,5 de haut | ⬜ |
| W1-51 | Barrière Vauban | métal galvanisé | 2,0 × 1,1 | ⬜ |
| W1-52 | Palette de livraison avec cartons | bois, cartons neutres | 1,2 × 0,8 × 1,0 | ⬜ |
| W1-53 | Diable de livraison | métal, deux roues | 1,2 de haut | ⬜ |
| W1-54 | Scooter garé générique | carénage gris, sans logo | 1,8 de long | ⬜ |
| W1-55 | Trottinette électrique en libre-service | générique sans logo | 1,1 de long | ⬜ |
| W1-56 | Panneau de rue (nom) | plaque bleue émaillée vierge, texte posé dans Unreal | 0,5 × 0,25 | ⬜ |
| W1-57 | Panneau « zone piétonne » générique | mât et panneau sans texte réel | 2,5 de haut | ⬜ |
| W1-58 | Caméra de surveillance urbaine | dôme sur console | 0,4 | ⬜ |
| W1-59 | Distributeur de billets mural | façade inox, écran, sans logo | 0,8 × 1,0 | ⬜ |
| W1-60 | Sac poubelle, tas | trois sacs noirs | 0,8 × 0,6 × 0,5 | ⬜ |

## Vague 2 : véhicules (ébauches) et objets de chaussée (pour 0.3)

Prompts : texte de l'objet, puis « single object, isolated, game prop, photorealistic, PBR textures, no logo, no readable text ». Aucune dimension dans les prompts.

| ID | Objet | Notes de génération | Statut |
| ---- | ---- | ---- | ---- |
| W2-01 | Ébauche de carrosserie citadine | réutilise le hatchback provisoire | ⬜ |
| W2-02 | Ébauche de carrosserie berline | réutilise la berline provisoire | ⬜ |
| W2-03 | Ébauche de carrosserie fourgonnette | réutilise le fourgon provisoire | ⬜ |
| W2-04 | Roue alliage avec pneu | alliage à cinq branches, pneu usé | ⬜ |
| W2-05 | Roue en tôle avec enjoliveur | tôle emboutie simple | ⬜ |
| W2-06 | Pneu de secours usé | poussiéreux, flanc fissuré | ⬜ |
| W2-07 | Porte de voiture froissée | débris, tôle pliée | ⬜ |
| W2-08 | Pare-chocs brisé | débris, plastique fissuré | ⬜ |
| W2-09 | Tas de débris d'accident | éclats de verre et fragments de plastique | ⬜ |
| W2-10 | Capot cabossé | débris, peinture éraflée | ⬜ |
| W2-11 | Borne de bord de route en béton | ronde, arêtes ébréchées | ⬜ |
| W2-12 | Balise souple de voie | plastique blanc, bande réfléchissante | ⬜ |
| W2-13 | Borne amovible en acier | socle, cadenas | ⬜ |
| W2-14 | Segment de ralentisseur caoutchouc | bandes noires et jaunes | ⬜ |
| W2-15 | Coussin berlinois | couleur asphalte, marquages peints | ⬜ |
| W2-16 | Poteau de zone de livraison | pictogramme sans texte | ⬜ |
| W2-17 | Potelet de parking rabattable | acier, articulé | ⬜ |
| W2-18 | Barrière de parking | bras rouge et blanc, boîtier | ⬜ |
| W2-19 | Plaque d'immatriculation avant vierge | sans caractères | ⬜ |
| W2-20 | Plaque arrière vierge avec support | sans caractères | ⬜ |
| W2-21 | Pile de vieux pneus | cinq pneus | ⬜ |
| W2-22 | Plaque de chaussée en acier | couvre-tranchée antidérapant | ⬜ |
| W2-23 | Barrière routière remplie d'eau | plastique rouge | ⬜ |
| W2-24 | Glissière en béton | ébréchée, grise | ⬜ |
| W2-25 | Balise de rive | blanche avec catadioptre | ⬜ |
| W2-26 | Batterie de voiture | générique, sans logo | ⬜ |
| W2-27 | Pot d'échappement | débris, rouillé | ⬜ |
| W2-28 | Rétroviseur brisé | débris, câble pendant | ⬜ |
| W2-29 | Bac à sel de voirie | plastique jaune, couvercle articulé | ⬜ |
| W2-30 | Panneau de chantier mobile | triangle vierge sur pied | ⬜ |

## Vagues V : véhicules réels du parc français (pour 0.3 et suivantes)

Source : docs/gameplay/vehicles.fr.md, section 10 (proposé par le fil véhicules ; décision de Nico : les voitures sont celles que l'on trouve en France). Voiture entière à l'échelle réelle, avec images de référence du vrai modèle, sans logo ni plaque (posés en décalques hors Git) ; découpe des roues et vitres et squelette dans Blender ensuite. Aucune dimension dans les prompts. Priorité 1 = conduisibles en 0.3 ; 2 = trafic 0.7 ; 3 = services et deux-roues.

| ID | Véhicule | Priorité | Statut |
| ---- | ---- | ---- | ---- |
| V-01 | Renault Clio V | 1 | ⬜ |
| V-02 | Peugeot 208 II | 1 | ⬜ |
| V-03 | Renault Kangoo II (white) | 1 | ⬜ |
| V-04 | Dacia Sandero III | 2 | ⬜ |
| V-05 | Citroën C3 III | 2 | ⬜ |
| V-06 | Peugeot 308 II | 2 | ⬜ |
| V-07 | Renault Captur II | 2 | ⬜ |
| V-08 | Peugeot 3008 II | 2 | ⬜ |
| V-09 | VW Golf VII | 2 | ⬜ |
| V-10 | Toyota Yaris IV | 2 | ⬜ |
| V-11 | VW Polo VI | 2 | ⬜ |
| V-12 | Renault Twingo III | 2 | ⬜ |
| V-13 | Fiat 500 | 2 | ⬜ |
| V-14 | Renault Mégane IV | 2 | ⬜ |
| V-15 | Dacia Duster II | 2 | ⬜ |
| V-16 | Mercedes A-Class W177 | 2 | ⬜ |
| V-17 | BMW 3 Series G20 | 2 | ⬜ |
| V-18 | Tesla Model Y | 2 | ⬜ |
| V-19 | Citroën Berlingo III | 2 | ⬜ |
| V-20 | Renault Trafic III | 2 | ⬜ |
| V-21 | Peugeot Boxer / Jumper | 2 | ⬜ |
| V-22 | Taxi Toyota Corolla estate | 2 | ⬜ |
| V-23 | TCL articulated bus (Heuliez GX 437 type) | 3 | ⬜ |
| V-24 | TCL trolleybus (Hess lighTram type) | 3 | ⬜ |
| V-25 | Peugeot 5008 Police nationale | 3 | ⬜ |
| V-26 | Lyon municipal police car | 3 | ⬜ |
| V-27 | SDMIS ambulance (VSAV) | 3 | ⬜ |
| V-28 | La Poste Kangoo | 3 | ⬜ |
| V-29 | Métropole waste truck | 3 | ⬜ |
| V-30 | Vélo'v bike | 3 | ⬜ |
| V-31 | Shared kick scooter | 3 | ⬜ |
| V-32 | Piaggio Liberty / Vespa scooter | 3 | ⬜ |
| V-33 | Yamaha TMAX | 3 | ⬜ |
| V-34 | Cargo bike | 3 | ⬜ |

## Ordre de génération prioritaire (rue de la République visible d'abord)

Décision de Nico : générer d'abord ce que l'on voit dans la rue 0.2. Ordre : vague 14, puis 13, puis 6, puis 8 (≈ 230 modèles, budget Studio ≈ 12 650 crédits). Les vagues 3, 4, 5, 7, 9 et suivantes, ainsi que les véhicules hors V-01 à V-34 déjà faits, attendent. Les noms d'objets ci-dessous sont en anglais (ils servent de prompts) ; aucune dimension dans les prompts ; devantures et enseignes sans texte lisible, les vraies enseignes sont posées ensuite en décalques hors Git.

## Vague 14 : Végétation en pot, devantures, petits détails de rue

| ID | Objet | Statut |
| ---- | ---- | ---- |
| W14-01 | Terracotta pot with boxwood ball | ⬜ |
| W14-02 | Zinc planter with olive tree | ⬜ |
| W14-03 | Window box red geraniums | ⬜ |
| W14-04 | Balcony planter lavender | ⬜ |
| W14-05 | Potted bay laurel | ⬜ |
| W14-06 | Potted palm | ⬜ |
| W14-07 | Hanging flower basket | ⬜ |
| W14-08 | Climbing ivy panel | ⬜ |
| W14-09 | Potted bamboo | ⬜ |
| W14-10 | Topiary cone | ⬜ |
| W14-11 | Cafe shopfront | ⬜ |
| W14-12 | Bakery shopfront | ⬜ |
| W14-13 | Pharmacy shopfront | ⬜ |
| W14-14 | Clothing boutique shopfront | ⬜ |
| W14-15 | Tobacconist shopfront | ⬜ |
| W14-16 | Bank branch shopfront | ⬜ |
| W14-17 | Shuttered shop | ⬜ |
| W14-18 | Phone shop shopfront | ⬜ |
| W14-19 | Bookshop shopfront | ⬜ |
| W14-20 | Restaurant shopfront | ⬜ |
| W14-21 | Butcher shopfront | ⬜ |
| W14-22 | Jeweler shopfront | ⬜ |
| W14-23 | Optician shopfront | ⬜ |
| W14-24 | Supermarket entrance | ⬜ |
| W14-25 | Hair salon shopfront | ⬜ |
| W14-26 | Fast food shopfront | ⬜ |
| W14-27 | Blade projecting sign (blank) | ⬜ |
| W14-28 | Pharmacy cross sign | ⬜ |
| W14-29 | Blank shop sign board | ⬜ |
| W14-30 | Shop door | ⬜ |
| W14-31 | Display mannequin | ⬜ |
| W14-32 | Window display shelf | ⬜ |
| W14-33 | Awning crank | ⬜ |
| W14-34 | Doormat | ⬜ |
| W14-35 | Intercom panel | ⬜ |
| W14-36 | Mailboxes wall unit | ⬜ |
| W14-37 | Building entrance door | ⬜ |
| W14-38 | Carriage gate | ⬜ |
| W14-39 | Doorstep stone | ⬜ |
| W14-40 | Boot scraper | ⬜ |
| W14-41 | House number plaque (blank) | ⬜ |
| W14-42 | Gutter downpipe | ⬜ |
| W14-43 | Wall lamp sconce | ⬜ |
| W14-44 | Air vent grille | ⬜ |
| W14-45 | Cellar window grille | ⬜ |
| W14-46 | Meter cabinet | ⬜ |
| W14-47 | Parcel locker | ⬜ |
| W14-48 | Satellite dish | ⬜ |
| W14-49 | Window shutters | ⬜ |
| W14-50 | French window with railing | ⬜ |
| W14-51 | Window with lace curtain | ⬜ |
| W14-52 | Flower pot on sill | ⬜ |
| W14-53 | Bicycle against wall | ⬜ |
| W14-54 | Blank A-board | ⬜ |
| W14-55 | Wall ashtray bin | ⬜ |
| W14-56 | Water valve cover | ⬜ |
| W14-57 | Cobblestone patch | ⬜ |
| W14-58 | Limestone paving slabs | ⬜ |
| W14-59 | Curb stone | ⬜ |
| W14-60 | Gutter channel with grate | ⬜ |
| W14-61 | Stained paving slab | ⬜ |
| W14-62 | Fallen leaves pile | ⬜ |
| W14-63 | Plane tree trunk | ⬜ |
| W14-64 | Young plane tree with guard | ⬜ |
| W14-65 | Street litter | ⬜ |
| W14-66 | Newspaper on ground | ⬜ |
| W14-67 | Rain puddle | ⬜ |
| W14-68 | Pigeon | ⬜ |
| W14-69 | Pigeons group | ⬜ |
| W14-70 | Street cat | ⬜ |
| W14-71 | Dog water bowl | ⬜ |
| W14-72 | Doorstep parcel | ⬜ |
| W14-73 | Stacked fruit crates | ⬜ |
| W14-74 | Milk crate | ⬜ |
| W14-75 | Beer kegs | ⬜ |
| W14-76 | Gas bottle | ⬜ |
| W14-77 | Flower bucket | ⬜ |
| W14-78 | Terrace heater | ⬜ |
| W14-79 | Terrace planter divider | ⬜ |
| W14-80 | Umbrella stand | ⬜ |

## Vague 13 : Ornements de façade et de monument

| ID | Objet | Statut |
| ---- | ---- | ---- |
| W13-01 | Stone balcony with iron railing | ⬜ |
| W13-02 | Continuous iron balcony | ⬜ |
| W13-03 | Carved console bracket | ⬜ |
| W13-04 | Triangular pediment | ⬜ |
| W13-05 | Curved pediment with keystone | ⬜ |
| W13-06 | Molded window surround | ⬜ |
| W13-07 | Keystone mascaron | ⬜ |
| W13-08 | Cornice with dentils | ⬜ |
| W13-09 | Corbel row | ⬜ |
| W13-10 | Corinthian pilaster | ⬜ |
| W13-11 | Ionic column | ⬜ |
| W13-12 | Corinthian column | ⬜ |
| W13-13 | Balustrade | ⬜ |
| W13-14 | Balustrade end post | ⬜ |
| W13-15 | Rusticated block | ⬜ |
| W13-16 | String course | ⬜ |
| W13-17 | Zinc mansard dormer | ⬜ |
| W13-18 | Oeil-de-boeuf | ⬜ |
| W13-19 | Chimney stack | ⬜ |
| W13-20 | Roof finial | ⬜ |
| W13-21 | Zinc ridge and gutter | ⬜ |
| W13-22 | Cartouche (blank shield) | ⬜ |
| W13-23 | Garland swag | ⬜ |
| W13-24 | Lion head relief | ⬜ |
| W13-25 | Rosette medallion | ⬜ |
| W13-26 | Caryatid | ⬜ |
| W13-27 | Atlas figure | ⬜ |
| W13-28 | Allegorical statue | ⬜ |
| W13-29 | Bust on pedestal | ⬜ |
| W13-30 | Equestrian bronze (generic) | ⬜ |
| W13-31 | Standing bronze (generic) | ⬜ |
| W13-32 | Blank bronze plaque | ⬜ |
| W13-33 | Fountain basin | ⬜ |
| W13-34 | Wall fountain | ⬜ |
| W13-35 | Obelisk | ⬜ |
| W13-36 | Stone urn | ⬜ |
| W13-37 | Pedestal | ⬜ |
| W13-38 | Ornate lamp bracket | ⬜ |
| W13-39 | Door knocker | ⬜ |
| W13-40 | Carved door panel | ⬜ |
| W13-41 | Transom grille | ⬜ |
| W13-42 | Window guard railing | ⬜ |
| W13-43 | Oculus vent | ⬜ |
| W13-44 | Facade clock | ⬜ |
| W13-45 | Sundial | ⬜ |
| W13-46 | Gothic portal arch | ⬜ |
| W13-47 | Gargoyle | ⬜ |
| W13-48 | Gothic pinnacle | ⬜ |
| W13-49 | Rose window | ⬜ |
| W13-50 | Statue niche | ⬜ |
| W13-51 | Bell turret | ⬜ |
| W13-52 | Ornate iron gate | ⬜ |
| W13-53 | Spiked iron fence | ⬜ |
| W13-54 | Ornate stone bollard | ⬜ |
| W13-55 | Bronze candelabra lamp | ⬜ |
| W13-56 | Theater arch bay | ⬜ |
| W13-57 | Small triumphal arch | ⬜ |
| W13-58 | Monument steps | ⬜ |
| W13-59 | Granite monument base | ⬜ |
| W13-60 | Ornate rainwater head | ⬜ |

## Vague 6 : Mobilier des places

| ID | Objet | Statut |
| ---- | ---- | ---- |
| W6-01 | Classic square bench | ⬜ |
| W6-02 | Backless stone and wood bench | ⬜ |
| W6-03 | Circular tree bench | ⬜ |
| W6-04 | Large square fountain | ⬜ |
| W6-05 | Equestrian monument pedestal | ⬜ |
| W6-06 | Triple-lantern lamp post | ⬜ |
| W6-07 | Single-lantern lamp post | ⬜ |
| W6-08 | Gravel path edging | ⬜ |
| W6-09 | Low hoop fence | ⬜ |
| W6-10 | Lawn patch | ⬜ |
| W6-11 | Raised flower bed | ⬜ |
| W6-12 | Mature plane tree | ⬜ |
| W6-13 | Clipped lime tree | ⬜ |
| W6-14 | Horse chestnut | ⬜ |
| W6-15 | Tree guard grille | ⬜ |
| W6-16 | Bandstand | ⬜ |
| W6-17 | Morris-style kiosk (blank) | ⬜ |
| W6-18 | Refreshment kiosk | ⬜ |
| W6-19 | Carousel | ⬜ |
| W6-20 | Playground slide | ⬜ |
| W6-21 | Swing set | ⬜ |
| W6-22 | Spring rider | ⬜ |
| W6-23 | Sandbox | ⬜ |
| W6-24 | Concrete ping pong table | ⬜ |
| W6-25 | Petanque court border | ⬜ |
| W6-26 | Petanque boules | ⬜ |
| W6-27 | Steel drinking fountain | ⬜ |
| W6-28 | Square trash bin | ⬜ |
| W6-29 | Recycling column | ⬜ |
| W6-30 | Info board (blank map) | ⬜ |
| W6-31 | Bollard with chain | ⬜ |
| W6-32 | Grass hoop | ⬜ |
| W6-33 | Granite paving patch | ⬜ |
| W6-34 | Ornamental pool | ⬜ |
| W6-35 | Floor water jets | ⬜ |
| W6-36 | Seated bronze statue (generic) | ⬜ |
| W6-37 | Commemorative column | ⬜ |
| W6-38 | Steps with handrail | ⬜ |
| W6-39 | Low stone wall | ⬜ |
| W6-40 | Automatic public toilet | ⬜ |
| W6-41 | Market stall frame | ⬜ |
| W6-42 | Market stall with vegetables | ⬜ |
| W6-43 | Wooden market chalet | ⬜ |
| W6-44 | Generic food truck | ⬜ |
| W6-45 | Square cafe umbrella | ⬜ |
| W6-46 | Terrace table with two chairs | ⬜ |
| W6-47 | Bike hoop row | ⬜ |
| W6-48 | Path light bollard | ⬜ |
| W6-49 | Boxwood hedge | ⬜ |
| W6-50 | Pigeon flock | ⬜ |

## Vague 8 : Mobilier routier

| ID | Objet | Statut |
| ---- | ---- | ---- |
| W8-01 | Vehicle traffic light | ⬜ |
| W8-02 | Pedestrian light | ⬜ |
| W8-03 | Light on arm pole | ⬜ |
| W8-04 | Push-button box | ⬜ |
| W8-05 | Stop sign (no letters) | ⬜ |
| W8-06 | Give way sign | ⬜ |
| W8-07 | No entry sign | ⬜ |
| W8-08 | Speed limit sign (blank) | ⬜ |
| W8-09 | No parking sign | ⬜ |
| W8-10 | One way sign | ⬜ |
| W8-11 | Pedestrian crossing sign | ⬜ |
| W8-12 | Bus stop pole (blank) | ⬜ |
| W8-13 | Direction finger signs (blank) | ⬜ |
| W8-14 | Bike lane sign | ⬜ |
| W8-15 | Roundabout sign | ⬜ |
| W8-16 | Warning triangle | ⬜ |
| W8-17 | Parking sign | ⬜ |
| W8-18 | Pole with two signs | ⬜ |
| W8-19 | Sign gantry (blank) | ⬜ |
| W8-20 | Guardrail | ⬜ |
| W8-21 | Guardrail end | ⬜ |
| W8-22 | Pedestrian railing | ⬜ |
| W8-23 | Concrete barrier | ⬜ |
| W8-24 | Plastic barrier | ⬜ |
| W8-25 | Barrier gate arm | ⬜ |
| W8-26 | Island bollard | ⬜ |
| W8-27 | Road stud | ⬜ |
| W8-28 | Lane separator | ⬜ |
| W8-29 | Tall cone | ⬜ |
| W8-30 | Traffic drum | ⬜ |
| W8-31 | Road works light | ⬜ |
| W8-32 | VMS trailer (blank) | ⬜ |
| W8-33 | Traffic camera pole | ⬜ |
| W8-34 | Signal controller cabinet | ⬜ |
| W8-35 | Zebra crossing patch | ⬜ |
| W8-36 | Painted speed bump | ⬜ |
| W8-37 | Manhole in asphalt | ⬜ |
| W8-38 | Road arrow marking | ⬜ |
| W8-39 | Curb ramp tactile | ⬜ |
| W8-40 | Tactile paving tile | ⬜ |

## Vagues 2 à 5 : aperçu (détail ajouté avant leur lancement)

- **Vague 2 (0.3)** : 3 ébauches de carrosseries fictives (citadine,
  berline, utilitaire), roues et jantes génériques, débris de carrosserie,
  plots, ralentisseur, place de livraison, borne de parking, plaques
  d'immatriculation vierges.
- **Vague 3 (0.4)** : pistolet fictif, chargeurs, étui, couteau, batte,
  caisses et sacs de sable pour couverture, mannequins d'entraînement.
- **Vague 4 (0.5)** : comptoir et caisse, rayonnages, réfrigérateur vitré,
  produits génériques sans marque (bouteilles, boîtes, conserves, fruits),
  paniers, caméra intérieure, porte vitrée.
- **Vague 5 (0.6)** : gyrophares, barrières de police, cônes, menottes,
  radio, gilets, panneaux de déviation, rubalise sans texte réel.
