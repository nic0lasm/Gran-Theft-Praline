# Missions sur l'interface

*English version: [missions-hud.md](missions-hud.md)*

Les règles et données des missions sont celles du fil Missions (ADR-0014, `GTPMissionSubsystem.h`).
L'interface dessine ce que le sous-système expose et ne contient aucune logique de mission
(`GTPHudMissions.cpp`). Le fichier ne compile rien si `GTPMissionSubsystem.h` est absent et ne fait
rien avec `-Missions=0`.

- **Cartes** : marqueurs de départ en losanges sur la mini-carte et la grande carte (jaune disponible,
  gris sinon), rafraîchis deux fois par seconde (`GetStartMarkers` alloue) ; le point de passage courant
  en bleu clair (sur l'anneau de la mini-carte hors de portée) et le **GPS vers lui**, posé quand il
  change et effacé avec lui (une destination choisie par le joueur est laissée jusqu'à ce qu'un point de
  passage la remplace).
- **Indice de l'objectif** en haut au centre pendant la mission (clé des tables de mission, texte dans
  `GTPStringTable.h`).
- **Invite de départ** près d'un marqueur disponible : « Commencer : <titre> » et « E / croix droite :
  commencer ».
- **Carte de résultat** : succès avec la récompense (« Mission accomplie : +150 € »), Entrée ou A
  continue ; échec avec la raison (`mission.fail.<raison>`), Entrée ou A réessaie, Retour arrière ou B
  abandonne.
- Textes : les lignes de `Content/Data/Missions/strings.tsv` sont dans la table de chaînes (un test
  vérifie les deux langues) ; les nouveaux textes de mission passent par le fil interface en une passe.

Argent (HUD) : voir `GTPHudMoney.cpp`, le solde en haut à droite sous les étoiles de la police avec un flash +/-, lu dans `UGTPWalletSubsystem::GetMoney`.
