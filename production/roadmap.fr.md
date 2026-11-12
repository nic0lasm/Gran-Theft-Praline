# Roadmap : Gran Theft Praline

*Version anglaise : [roadmap.md](roadmap.md)*
*Version 4, 2026-11-12 : recadrage. Remplace la version 3 (ordre 0.4 → 1.0)
et met en pause le [plan vers la phase de test 0.9](test-phase-plan.fr.md).
Les versions précédentes restent dans l'historique Git.*
*Suivi au jour le jour : [roadmap-status.fr.md](roadmap-status.fr.md).
Liste de contrôle des systèmes : [systems-inventory.fr.md](systems-inventory.fr.md).
Concept : [game-concept.fr.md](../design/gdd/game-concept.fr.md).*

---

## 1. L'objectif

**Priorité 1 : un GTA-like réaliste du centre de Lyon.** Les mécaniques de
GTA (marcher, voler une voiture, conduire dans le trafic, se battre, être
recherché par la police et la semer) et l'intelligence de ses personnages non
joueurs (des piétons qui réagissent, un trafic crédible, une police qui
traque), dans une rue de Lyon qui ressemble à la vraie.

L'histoire vient **après**. La piste est posée : des vols de produits
lyonnais et des fuites par les traboules. Elle ne sera écrite que lorsque la
priorité 1 tient son critère. En attendant, la carte prévoit déjà les
traboules (passages, cours, portes) pour qu'elles puissent devenir des
itinéraires de fuite.

## 2. Où on en est, honnêtement

- **Dernière version publique : 0.3.0 « Au volant »** (10 novembre 2026,
  macOS). On y marche et on y conduit dans la rue de la République, avec en
  avant-première les piétons, la police, la mini-carte, le jour, la nuit et
  la météo.
- **Branche `next`** (préproduction) : beaucoup de systèmes existent en
  code (piétons et témoins, trafic, police et étoiles, combat à mains nues et
  au pistolet, ragdoll, braquage de caisse, missions, mini-carte, intérieur
  de fast-food, jour, nuit, météo). La plupart sont **à moitié finis** et
  aucun n'a encore été jugé en situation de jeu.
- **L'image est loin du but.** La première planche de comparaison photo
  réelle / capture du jeu (candidat 17b, 12 novembre) donne 1 sur 5 à deux
  vues et 0 aux trois autres : ombres noires, carte vide au-delà de 45 m,
  sol en carrelage gris au lieu de surfaces scannées, vitrines et nuit
  noires, joueur encore en mannequin, ni piétons ni voitures sur les vues.
  Le niveau réel est celui d'une **maquette améliorée**, pas d'un jeu.
- **Pourquoi :** on a optimisé un prototype pour un portable M1 avant de
  savoir à quoi il devait ressembler, on a ouvert trop de chantiers à la
  fois, et la direction artistique manquait.

## 3. La cible : la tranche rue de la République

Tout l'effort va sur **300 à 400 m de rue, de Bellecour aux Jacobins.**

**C'est fini quand :**

1. **10 minutes de jeu dans la tranche ressemblent à GTA** : on vole une
   voiture, on roule dans un trafic qui s'arrête aux feux et s'écarte, on
   descend à pied parmi des passants qui vivent et réagissent (fuient,
   crient, appellent la police), on se bat, on prend des étoiles, la police
   traque et on la sème ou on se fait arrêter, on meurt et on réapparaît.
   Preuve : **une vidéo des 10 minutes**, sans montage.
2. **L'image tient la comparaison** sur **5 paires photo réelle / capture du
   jeu** prises aux mêmes points de vue, refaites à chaque candidat :
   entrée côté Bellecour, façade du cinéma, vitrines à hauteur d'homme,
   bassin de la place de la République, nuit devant le cinéma.
3. **Stable sur la machine de référence** : package PC sans plantage.

### Au cœur de la priorité 1

Conduite, piétons, trafic, police, combat de base, et l'image de la rue
(sol et façades scannés, vitrines, éclairage, nuit, joueur réaliste).

### Gelé jusqu'au critère tenu

Missions, audio (au-delà de l'existant), menus, intérieurs, sauvegarde de
partie, mini-carte, remappage des touches, crédits. Le fast-food reste en
maintenance (corrections seulement). Le build de test du quartier (0.9) est
reporté. Les systèmes gelés ne reçoivent que des corrections de plantage.

## 4. Comment on travaille

| Règle | Ce que ça veut dire |
| ---- | ---- |
| **PC d'abord** | La qualité d'image se règle sur un PC Windows (RTX 3070 Ti) avec Lumen matériel et Nanite. Un changement entre sur `next` quand la CI est verte, que le projet compile sur le PC et qu'un package PC tourne 120 s sans plantage. |
| **Mac ensuite** | Le MacBook Pro M1 Pro fait une mesure par nuit sur `next`, à titre d'alerte, avec un profil bas ; il ne bloque plus les fusions. Le portage Mac jouable vient plus tard. |
| **Assets gratuits seulement** | Zéro euro : Quixel Megascans, MetaHuman, City Sample, Fab gratuit, fait maison. Chaque asset a sa ligne de licence dans [assets/SOURCES.md](../assets/SOURCES.md). |
| **Un candidat par jour au plus** | Chaque candidat arrive avec ses 5 paires photo / capture. Un petit gain n'est pas présenté comme un succès : on juge sur les paires et la vidéo. |
| **Quatre chantiers actifs** | Image de la rue ; personnages et PNJ ; véhicules et police ; machines (compilation, packages, mesures). Tout le reste est à l'arrêt. |

## 5. Calendrier

Des ordres de grandeur, revus à chaque point, sans promesse au-delà.

| Quand | Quoi |
| ---- | ---- |
| **Fin novembre 2026** | La tranche rue de la République tient son critère sur PC |
| **Mi à fin décembre 2026** | Le nord de la Presqu'île, de Bellecour aux Terreaux (~1,5 km²), au même niveau de mécaniques |
| **Février 2027** | Portage Mac |
| **Vers février 2027, avec une forte marge** | Un quartier jouable et beau sur PC |
| Ensuite | Le scénario : vols de produits lyonnais, fuites par les traboules |

Hors de portée de ce plan : tout Lyon, les cinématiques.

## 6. Prochaine étape (candidat 18)

- Joueur MetaHuman à la place du mannequin.
- Surfaces scannées posées sur le sol et les trottoirs de la tranche.
- Ville régénérée : bâtiments de fond jusqu'à l'horizon, tour du cinéma,
  bassin, tronçon jusqu'à Bellecour, toits en zinc, relief de façade.
- Profil d'image PC ultra avec Lumen matériel.
- Seconde planche des 5 vues et package PC 120 s.

## 7. Risques

| Risque | Parade |
| ---- | ---- |
| L'image reste loin malgré les surfaces et le MetaHuman | Les 5 paires montrent l'écart à chaque candidat ; on corrige le plus gros écart d'abord |
| Le PC (8 Go de mémoire vidéo, disque serré) limite l'image ou les packages | Un gros travail à la fois, anciens packages purgés, budgets mesurés |
| Des systèmes à moitié finis gâchent les 10 minutes | Les systèmes gelés restent coupés dans la tranche ; on finit ceux du cœur avant d'en ouvrir d'autres |
| Le Mac décroche pendant le travail sur PC | Mesure nocturne d'alerte, portage planifié en février |

## 8. Ce qui est livré

| Version | Contenu | Sortie |
| ---- | ---- | ---- |
| 0.1 Jouer en un clic | Lancer le jeu et marcher rue de la République | 9 nov. 2026 |
| 0.2 La rue habillée | Une rue fidèle et habillée, jouable sur un portable M1 | 10 nov. 2026 |
| 0.3 Au volant | Voler une voiture, traverser la circulation jusqu'au quai du Rhône | 10 nov. 2026 |

La prochaine version publique sortira quand la tranche tient son critère ;
son numéro sera fixé à ce moment.
