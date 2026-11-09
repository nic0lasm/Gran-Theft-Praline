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
