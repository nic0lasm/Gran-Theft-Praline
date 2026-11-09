# Concept du jeu : Gran Theft Praline

*Créé le : 2026-11-09*
*Statut : brouillon. Périmètre fixé par le responsable du projet : parité
fonctionnelle et visuelle avec la génération actuelle des jeux criminels en
monde ouvert, sur un Lyon fidèle. L'histoire, les personnages et le ton
viendront ensuite.*
*Version anglaise : [game-concept.md](game-concept.md)*

---

## Pitch

> Un jeu criminel en monde ouvert photoréaliste dans le Lyon d'aujourd'hui,
> avec la maniabilité, les armes, les boutiques et bâtiments accessibles et la
> richesse d'interactions qu'on attend des meilleurs du genre (Grand Theft
> Auto VI sert de référence), sur une carte reconstruite aussi fidèlement que
> possible à partir de la vraie ville.

L'histoire, le protagoniste et le ton **ne sont pas encore définis**. Ils
seront écrits une fois la base jouable. Ce document définit cette base : ce
que le joueur peut faire, les sensations et le rendu visés, et la partie de
Lyon par laquelle on commence.

---

## Identité

| Aspect | Détail |
| ---- | ---- |
| **Genre** | Action-aventure criminelle en monde ouvert, troisième personne (première personne en option plus tard) |
| **Plateforme** | macOS Apple Silicon (cible de développement), puis PC Windows |
| **Joueurs** | Solo |
| **Durée d'une session** | 30 à 120 minutes |
| **Modèle économique** | Premium, rien de prévu pour l'instant |
| **Ampleur estimée** | Grande (tranche verticale 9 à 12 mois, vision complète sur plusieurs années, développeur solo avec outils génératifs) |
| **Référence** | Grand Theft Auto VI (référence fonctionnelle et visuelle), Grand Theft Auto V (socle des systèmes), Red Dead Redemption 2 (richesse des interactions, animation) |

---

## Fantasme central

Lyon comme une ville vivante et jouable : marcher, conduire, se battre et
entrer dans des lieux, dans des rues qu'on reconnaît, avec une ville qui
réagit comme une vraie.

---

## Originalité

L'expérience de référence du genre, **et en plus** une vraie ville européenne
reconstruite rue par rue : la Presqu'île, les fleuves et les ponts, les
collines, les commerces et la lumière de Lyon, au lieu d'une ville américaine
fictive.

---

## Méthode de référence

La référence est **Grand Theft Auto VI** : ce qui en est montré publiquement
(bandes-annonces, communication officielle) puis, une fois sorti, le jeu
lui-même. Quand une fonctionnalité n'est pas encore documentée, **Grand Theft
Auto V** et **Red Dead Redemption 2** fixent le minimum. Chaque fonctionnalité
ci-dessous a une cible de parité ; une revue de parité a lieu après la sortie
et à chaque jalon.

On reproduit **des fonctions et des niveaux de qualité, jamais du contenu** :
aucun nom, personnage, logo, tracé de carte, mission, musique, élément
d'interface ou asset de ces jeux.

---

## Cibles de parité fonctionnelle

Priorité : **P0** = tranche verticale, **P1** = alpha, **P2** = vision complète.

### 1. Maniabilité à pied et personnage
| Fonctionnalité | Cible | Priorité |
| ---- | ---- | ---- |
| Marcher, trottiner, sprinter, s'accroupir, sauter, franchir, escalader les obstacles bas | Motion matching, poids, réactivité | P0 |
| Caméra | Troisième personne avec changement d'épaule ; première personne | P0 / P1 |
| Réactions physiques | Trébucher, tomber, ragdoll mélangé à l'animation, bousculer la foule | P0 |
| Nage dans la Saône et le Rhône | Nager, plonger, remonter sur les quais | P1 |
| Endurance, santé, armure | Lisibles sans HUD chargé | P0 |
| Interactions contextuelles | Portes, s'asseoir, s'adosser, ramasser, ouvrir des contenants | P0 |
| Apparence | Vêtements, coiffure, accessoires achetés en boutique | P1 |

### 2. Conduite et véhicules
| Fonctionnalité | Cible | Priorité |
| ---- | ---- | ---- |
| Voitures à conduite physique | Transferts de masse, adhérence, freinage, sensations par catégorie (citadine, berline, utilitaire, sportive) | P0 |
| Deux-roues | Scooter, moto, vélo | P0 / P1 |
| Dégâts | Déformation, vitres brisées, phares, crevaisons, fumée | P0 |
| Monter et descendre | Animé, voler une voiture garée, braquer un conducteur | P0 |
| À bord | Radio, clignotants, klaxon, modes de caméra | P1 |
| Transports en commun et bateaux | Bus, tramways, bateaux sur les fleuves, en véhicule ou en décor | P2 |
| Garages, personnalisation | Ranger et modifier ses véhicules | P1 |

### 3. Armes et combat
| Fonctionnalité | Cible | Priorité |
| ---- | ---- | ---- |
| Corps à corps | Coups de poing, coups de pied, neutralisations | P0 |
| Armes à feu | Pistolet, pistolet-mitrailleur, fusil à pompe, fusil d'assaut ; visée, recul, rechargement | P0 |
| Couverture | Se plaquer, tir à l'aveugle, passer d'un abri à l'autre | P0 |
| Roue des armes et inventaire | Sélection rapide, port limité | P0 |
| Réactions aux impacts et modèle de dégâts | Localisés, lisibles | P0 |
| Armes de jet | Grenades, cocktails Molotov | P1 |
| Armurerie | Acheter armes, munitions, améliorations (marques fictives) | P1 |

### 4. Police et système de recherche
| Fonctionnalité | Cible | Priorité |
| ---- | ---- | ---- |
| Témoins et signalement | Les crimes sont signalés, pas détectés par magie | P0 |
| Niveaux de recherche 1 à 5 | Patrouilles, barrages, herses, hélicoptère, unités spéciales | P0 (1 à 3) / P1 |
| Recherche et fuite | Ligne de vue, zone de recherche, se cacher, changer de véhicule | P0 |
| Arrestation et mort | Pénalité en argent et équipement, reprise rapide | P0 |
| Police française fictive | Véhicules, uniformes et procédures inspirés de la police française, insignes fictifs | P0 |

### 5. Intérieurs, boutiques et bâtiments
| Fonctionnalité | Cible | Priorité |
| ---- | ---- | ---- |
| Boutiques accessibles sans écran de chargement | Boulangerie, café, tabac-presse, vêtements, supérette, armurerie, garage | P0 (5 types) |
| Achats | Nourriture, vêtements, armes, véhicules | P0 / P1 |
| Braquage de commerce | Menacer le vendeur, caisse, alarme, recherche | P0 |
| Bâtiments accessibles | Halls, cages d'escalier, cours, quelques appartements, parkings, toits | P0 (sélection) / P1 |
| Planques | Sauvegarder, se changer, ranger armes et véhicules | P0 |
| Bars, restaurants, cinéma, salle de sport | Ambiance et mini-activités | P1 / P2 |

### 6. Ville vivante
| Fonctionnalité | Cible | Priorité |
| ---- | ---- | ---- |
| Foule | Piétons denses avec emplois du temps, réactions à la violence, conversations | P0 |
| Circulation | Respect des voies, feux, piétons qui traversent, accidents, réactions au joueur | P0 |
| Heure et météo | Cycle jour/nuit complet, pluie, brume, surfaces mouillées | P0 |
| Événements d'ambiance | Scènes de rue, disputes, petite délinquance, livraisons | P1 |
| Faune | Pigeons, chiens, oiseaux des fleuves | P2 |

### 7. Systèmes et interface
| Fonctionnalité | Cible | Priorité |
| ---- | ---- | ---- |
| Téléphone | Contacts, carte, messages, réseau social fictif | P1 |
| Carte et GPS | Vrais noms de rues, guidage | P0 |
| Argent et économie | Gagner, dépenser, voler | P0 |
| HUD minimal | Mini-carte, santé, recherche ; masquable | P0 |
| Sauvegarde | Planques, sauvegarde automatique | P0 |
| Accessibilité | Sous-titres, réaffectation des touches, aide à la visée, options de couleurs | P0 |

### 8. Graphismes
| Domaine | Cible | Priorité |
| ---- | ---- | ---- |
| Éclairage | Illumination globale et réflexions Lumen, ciel physique et course du soleil à la latitude de Lyon | P0 |
| Géométrie | Nanite pour bâtiments, rues et objets ; matières de qualité photogrammétrie | P0 |
| Personnages | Visages et peau au niveau MetaHuman, simulation des vêtements | P0 |
| Effets | Surfaces mouillées, flaques, brouillard volumétrique, pluie, fumée, feu, verre | P0 |
| Véhicules | Peinture, saleté, dégâts, intérieurs détaillés | P0 |
| Densité | Enseignes, mobilier urbain, déchets, terrasses de cafés, scooters garés | P0 |
| Post-traitement | Cinématographique, retenu, focales naturelles | P0 |

La **cadence et la résolution cibles** sur le Mac de développement sont fixées
par un ADR avant la production, puis jamais assouplies sans décision du
responsable du projet.

---

## Fidélité à Lyon

- **Tracé** : réseau de rues, emprises et hauteurs des bâtiments, relief,
  fleuves et quais d'après l'IGN (LiDAR HD, BD TOPO), les données ouvertes du
  Grand Lyon (bâtiments 3D) et OpenStreetMap. Crédits affichés en jeu.
- **Monuments** : reconstruits à l'échelle réelle à partir de références
  publiques (place Bellecour, Hôtel de Ville, Opéra, ponts, Fourvière à
  l'horizon).
- **Façades et rez-de-chaussée** : matières et couleurs propres à chaque rue,
  vrais commerces aux bons endroits avec leurs **vraies enseignes et
  marques**, reproduites d'après photos terrain (décision de Nico du
  2026-11-09, usage non commercial ; voir la [bible artistique](../../docs/art/art-bible.fr.md),
  section 3.5) ; **visages fictifs**.
- **Intérieurs** : absents des données ouvertes ; reconstitués en fiction crédible.
- **Référence visuelle uniquement** : Google Photorealistic 3D Tiles et photos
  de terrain guident la modélisation mais ne sont jamais livrées dans le jeu.
- **Outils pour reproduire la ville** (chaîne de données géo, Cesium for
  Unreal, façades procédurales, photogrammétrie, outils génératifs) : évalués
  dans une étude séparée.

---

## Boucle de jeu (socle, sans histoire)

### À la seconde (30 secondes)
Se déplacer dans Lyon à pied ou en véhicule, se battre, fuir ou entrer dans un lieu.

### À court terme (5 à 15 minutes)
Une activité autonome : braquer une boutique, voler une voiture sur commande,
échapper à un niveau de recherche, acheter et s'équiper, livrer quelque chose
à l'autre bout de la ville.

### À l'échelle d'une session (30 à 120 minutes)
Une suite d'activités et d'exploration libre qui se termine dans une planque
avec plus d'argent, un meilleur équipement et de nouveaux lieux débloqués. Les
missions d'histoire viendront s'insérer ici.

### Progression à long terme
Argent, armes, véhicules, vêtements, planques, puis l'histoire et de nouveaux
quartiers.

---

## Piliers du jeu

### Pilier 1 : Une maniabilité de référence
Se déplacer, conduire et tirer doit être aussi agréable que dans la
référence ; les sensations sont réglées avant d'ajouter du contenu.

*Test* : entre une nouvelle fonctionnalité et le polissage d'une action
existante, on polit l'action jusqu'à la parité.

### Pilier 2 : Lyon tel qu'il est
La carte est la vraie ville ; dans la zone jouable, on ne déplace, ne fusionne
et n'invente aucune rue.

*Test* : entre un tracé fictif pratique et le vrai tracé, on garde le vrai et
on adapte le gameplay.

### Pilier 3 : Un photoréalisme qu'on peut toucher
Chaque surface, lumière et son doit passer pour une photo ou un
enregistrement ; ce que le joueur voit derrière une vitrine, il doit le plus
souvent pouvoir y entrer ou interagir avec.

*Test* : entre une zone plus grande et une zone plus dense et plus
interactive, on choisit la densité.

### Pilier 4 : Des systèmes avant des scripts
Police, foule, circulation et commerces fonctionnent comme des systèmes qui
réagissent au joueur partout, pour que l'histoire ajoutée ensuite repose sur
une ville vivante.

*Test* : entre une scène scriptée et un système qui produit le même moment,
on construit le système.

### Anti-piliers (ce que le jeu n'est PAS)

- **PAS une copie du contenu d'un autre jeu** : on égale les fonctions et la
  qualité, jamais les noms, personnages, carte, missions ou assets.
- **PAS de personnes réelles** : les lieux et les enseignes sont fidèles
  (vraies marques comprises, usage non commercial), les visages fictifs.
- **PAS toute la ville d'un coup** : un quartier en qualité finale d'abord,
  puis quartier par quartier.
- **PAS de multijoueur** pour l'instant : le budget va à la ville et aux systèmes.
- **PAS encore centré sur l'histoire** : l'histoire s'écrit avec le
  responsable du projet une fois la base jouable.

---

## Tranche verticale : périmètre de la ville

**Quartier : le nord de la Presqu'île, de la place Bellecour à la
place des Terreaux, entre les quais de Saône et du Rhône** (environ 1,5 km²).
*Décidé par le responsable du projet le 2026-11-09.*

- Pourquoi : les rues commerçantes les plus denses (types de boutiques
  accessibles en P0), des rues larges et des quais pour la conduite et les
  poursuites, deux fleuves et leurs ponts, des monuments emblématiques
  (Bellecour, Hôtel de Ville, Opéra), une excellente couverture en données
  ouvertes, et un terrain plat qui limite les premiers risques techniques.
- Alternatives : les Pentes de la Croix-Rousse (caractère, escaliers, mais
  raides et étroites pour conduire), le Vieux-Lyon et Fourvière (patrimoine,
  mais surtout piétons), la Part-Dieu (tours modernes et boulevards, moins
  emblématique).
- Contenu visé : tout le quartier en qualité finale, environ 30 boutiques
  accessibles réparties en 5 types, 3 planques, 10 bâtiments accessibles,
  6 types de véhicules, 6 armes, recherche niveaux 1 à 3, 6 activités libres.

---

## Aspects techniques

| Point | Évaluation |
| ---- | ---- |
| **Moteur** | Unreal Engine 5.8 (décidé), C++ et Blueprint pour prototyper, Lumen, Nanite, World Partition, Mass pour la foule et la circulation, Chaos Vehicles |
| **Défis techniques** | Reconstruction de la ville depuis les données géo, intérieurs sans chargement et streaming, densité de foule et de circulation, IA de police, performances sur Apple Silicon |
| **Style graphique** | 3D photoréaliste |
| **Complexité de la chaîne graphique** | Très élevée |
| **Audio** | Élevé (ambiance de ville, véhicules, armes, radio) |
| **Réseau** | Aucun |
| **Systèmes procéduraux** | Volumes des bâtiments depuis les données géo, variations de façades, population de foule et de circulation |

---

## Risques et questions ouvertes

- **Périmètre** : égaler en entier un studio de milliers de personnes est
  hors de portée ; la réponse est un seul quartier en qualité finale, P0
  uniquement, avant tout le reste.
- **Performances** : photoréalisme, foule et circulation sur Apple Silicon ;
  l'ADR du budget de performance passe avant la production.
- **Disponibilité de la référence** : une partie de la référence n'est connue
  que par la communication publique ; les cibles de parité sont revues une
  fois le jeu sorti.
- **Intérieurs** : chaque lieu accessible est du contenu fait main ; les
  types de boutiques partagent des kits pour limiter le coût.
- **Question ouverte** : quels outils reproduisent le mieux Lyon pour notre
  budget ? Réponse dans l'étude séparée sur la reproduction de la ville.

---

## Définition du MVP

**Hypothèse centrale** : notre maniabilité (à pied, en voiture, au tir)
atteint les sensations de la référence sur un vrai pâté de maisons lyonnais.

**Indispensable au MVP** :
1. Un pâté de maisons du quartier choisi en maquette grise, construit depuis les vraies données géo.
2. Déplacements à pied et caméra aux sensations visées.
3. Une voiture à conduite physique avec dégâts.
4. Un pistolet avec couverture et corps à corps.
5. Une boutique accessible qu'on peut braquer, et les niveaux de recherche 1 et 2.

**Hors MVP** : graphismes finaux, foule et circulation à pleine densité,
histoire, téléphone, personnalisation.

### Paliers de périmètre

| Palier | Contenu | Fonctionnalités | Durée |
| ---- | ---- | ---- | ---- |
| **MVP** | Un pâté de maisons en maquette grise | Maniabilité, une voiture, une arme, une boutique, recherche 1-2 | 6 à 8 semaines |
| **Tranche verticale** | Quartier choisi en qualité finale | Toutes les fonctionnalités P0 | 9 à 12 mois |
| **Alpha** | Centre de Lyon, deux rives | Fonctionnalités P0 et P1, début de l'histoire | Plusieurs années |
| **Vision complète** | Grand centre de Lyon | Toutes les fonctionnalités, histoire complète | À planifier |

---

## Prochaines étapes

- [x] Décision : quartier de la tranche verticale (nord de la Presqu'île, 2026-11-09)
- [ ] Étude : outils pour reproduire Lyon (données géo, Cesium for Unreal, façades procédurales, photogrammétrie)
- [ ] `/art-bible` : identité visuelle photoréaliste de Lyon
- [ ] ADR du budget de performance
- [ ] `/map-systems` : transformer les tableaux de parité en systèmes et priorités
- [ ] `/prototype handling` : le MVP ci-dessus
- [ ] Histoire, personnages et ton avec le responsable du projet, une fois la base jouable
