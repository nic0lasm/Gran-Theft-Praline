# Ragdoll et réactions physiques

*Version anglaise : [ragdoll.md](ragdoll.md)* · Décision : [ADR-0006](../adr/adr-0006-ragdoll-and-impacts.fr.md)

Code : `GTPImpactLogic.h` (règles), `GTPRagdoll.h` (composant, plafond),
`GTPPlayerReaction.h` (joueur). Contrôle : `python3 tools/ragdoll/check_impact_logic.py`.

## Ce qu'une voiture fait à quelqu'un à pied

Vitesse de rapprochement le long de la ligne voiture-personne ; une voiture qui
s'éloigne ou à l'arrêt ne blesse personne.

| Vitesse de rapprochement | Gravité | Effet | Dégâts | Issue |
| --- | --- | --- | --- | --- |
| moins de 4 km/h | aucune | rien | 0 | - |
| 4 à 12 | bousculade | trébuchement (plus lent 0,7 s) | 0-6 | continue de marcher |
| 12 à 28 | renversement | ragdoll, petite poussée | 12-45 | se relève après environ 2 s |
| 28 à 55 | projection | ragdoll, lancé avec hauteur et vrille | 45-100 | se relève après environ 4 s s'il vit |
| 55 et plus | violent | ragdoll, longue projection | 100 | reste au sol |

Le corps part à environ 55 % de la vitesse de la voiture (partagée selon les
masses), avec 35 % de hauteur pour les projections, 18 m/s au plus. La voiture
perd moins de 2 km/h.

## Chutes et sauts de voiture (joueur)

| Vitesse verticale à l'atterrissage | Réaction |
| --- | --- |
| moins de 6,5 m/s | rien |
| 6,5 à 9 | trébuchement |
| 9 à 12,5 | roulade (trébuchement pour l'instant, montage plus tard) |
| plus de 12,5 | ragdoll |

Accroupi, la vitesse comptée est divisée par deux. Sortir d'une voiture à 8-30 km/h :
trébuchement ; 30-55 : renversé ; au-delà de 55 : projeté. Environ 3 m de chute
donnent un trébuchement, 8 m un ragdoll.

## Coût

Au plus 3 ragdolls simulés en m1 (6 fidelity, 10 ultra) ; au-delà, le plus loin est
gelé. Aucun coût sans ragdoll (pas de tick). Mesurer sur le Mac avant de relever
les plafonds.

## Accroches pour les autres systèmes

- `UGTPPlayerReactionComponent::OnHurt(Damage, HealthLeft)` : HUD, niveau de
  recherche.
- `UGTPRagdollComponent::OnRagdollEnded(Ragdoll, bGotUp)` : police (corps au sol),
  piétons (retour au pool).
- `GTPImpact::ClassifyCarHit`, `ClassifyLanding`, `ClassifyBailOut` : fonctions
  pures, appelables par tout système.

## Ligne de commande et test automatique

- `-Ragdoll=0` : aucune réaction physique (le build journalise `ragdoll: on` ou `off`).
- `-MaxRagdolls=N` : remplace le plafond du profil.
- `-RagdollTest` (+ `-RagdollTestQuit`) : scénario sans joueur : chocs à 20, 40 et
  70 km/h, chutes de 3 et 8 m, quatre corps à la fois face au plafond de trois ;
  journalise `GTPRagdollTest: ... PASS` ou `FAIL` et le temps d'image moyen.

## Mobilier de rue cassable

`GTPBreakableLogic.h` et `UGTPBreakableSubsystem` (les props de rue sont instanciés :
un acteur par asset, donc le sous-système remplace l'instance touchée par un acteur
physique). Tags sur les composants instanciés, posés depuis les tableaux de
placement par le script de la rue : `GTP_Breakable_R` (se renverse),
`GTP_Breakable_C` (casse), `GTP_MassKg=<n>`, option `GTP_BrokenMesh=<chemin>`. Les
objets fixes (potelets) n'ont pas besoin de tag. Règles : un renversable tombe dès
une poussée de 3 km/h ; un cassable demande 400 J + 20 J par kg (un lampadaire de
250 kg casse sous une voiture vers 11 km/h, un panneau de 25 kg vers 5 km/h ; une
personne ne casse pas un lampadaire). Le corps est lancé avec une part de la vitesse
de l'impacteur et la voiture perd un peu. Au plus 6 corps vivants en m1 (16
ailleurs), 25 s chacun. `-Breakables=0` coupe tout. Liste des objets :
docs/gameplay/breakable-street-furniture.fr.md (fil réalisme de la rue).
