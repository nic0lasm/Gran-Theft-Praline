# Réglages et textes

*English version: [settings.md](settings.md)*

Roadmap : textes en français et anglais (0.4), réglages sauvegardés (0.5). Décision : [ADR-0007](../adr/adr-0007-interface-settings-minimap.fr.md).

## Ce que voit le joueur

L'écran titre et le menu pause ont un bouton **Réglages**. Chaque ligne de l'écran est
`libellé : valeur` ; un clic (ou Entrée / A de la manette) passe à la valeur suivante,
l'applique tout de suite et l'enregistre. Échap, B de la manette ou **Retour** revient
au menu d'où l'écran a été ouvert.

| Ligne | Valeurs | Appliqué par |
| ----- | ------- | ------------ |
| Langue | Français, English | `GTPSetLanguage` : tous les textes lus par `GTPLoc` |
| Graphismes | Fluide (M1), Fidélité, Ultra | `UGTPGraphicsProfile::SetProfile`, profils de l'[ADR-0002](../adr/adr-0002-performance-budget.fr.md) |
| Volume | 0 à 100 %, par pas de 10 | `FApp::SetVolumeMultiplier` (tout le son du jeu) |
| Sensibilité de la vue | 25 à 200 %, par pas de 25 | `AGTPPlayerController::AddYawInput / AddPitchInput` |
| Vue verticale inversée | oui / non | `AddPitchInput` |
| Clavier | Automatique, AZERTY, QWERTY | `UsesQwerty()`, contextes de mapping réappliqués aussitôt |
| Mini-carte | oui / non | [mini-carte](minimap.fr.md) |
| Zoom de la mini-carte | proche 120 m, moyen 220 m, loin 400 m | [mini-carte](minimap.fr.md) |

Le fichier est `Saved/Config/GTPSettings.ini` (un `clé=valeur` par ligne, commentaires
avec `#`) ; un fichier abîmé ou édité à la main ne casse jamais le jeu (valeurs
bornées, lignes invalides ignorées). `-GfxProfile=` et `-Qwerty` en ligne de commande
restent prioritaires au démarrage.

Pas dans cette première version, volontairement : réassigner touche par touche (la
paire AZERTY / QWERTY couvre les deux dispositions ; une réassignation complète passe
par les réglages utilisateur d'Enhanced Input et viendra avec le passage sur les
commandes), résolution et mode fenêtre, sous-titres. Ils sont dans la roadmap.

## Écran des commandes

Le bouton **Commandes** (menus titre et pause) liste, pour la disposition de clavier
utilisée (AZERTY ou QWERTY) et pour la manette, la touche de chaque action qui existe
dans le code : interagir, monter / descendre, changer de vue, recentrer, accélérer,
freiner, tourner, frein à main, regarder derrière, carte, pause. Les lignes sont dans
`GTPControls.h` et suivent les assignations (déplacement à pied et saut sont dans les
assets Enhanced Input et ne sont pas encore listés).

## Textes

Tous les textes des menus sont dans une seule table,
`Source/GranTheftPraline/Public/GTPStringTable.h` : une ligne par texte avec une clé
pointée, le texte français et le texte anglais. Dans le code :

```cpp
GTPLoc("menu.play");                                          // « Jouer » / « Play »
GTPLocFormat("settings.volume", {FText::AsNumber(40)});       // « Volume : 40 % » / « Volume: 40% »
```

Une clé inconnue s'affiche telle quelle, une faute de frappe se voit. Ajouter un texte
= une ligne dans la table ; ajouter une langue = une colonne.
`tools/ui/tests/run_cpp_tests.sh` vérifie que chaque clé a ses deux textes, que les
marqueurs `{0}` concordent dans les deux langues et qu'aucune clé n'est en double ; il
tourne en CI.

Les aides des autres systèmes (« E », « F Monter », police, piétons) gardent pour
l'instant leurs propres chaînes ; `prompt.enter_vehicle` et `prompt.exit_vehicle` sont
déjà dans la table pour passer à `GTPLoc` à la prochaine intervention sur ces systèmes.

## Tests

- `tools/ui/tests/run_cpp_tests.sh` : table de chaînes, aller-retour des réglages et
  fichiers abîmés, maths de la mini-carte (sans Unreal, un compilateur suffit).
- En jeu, sans joueur (`GTPUIAutoTest.h`) : `-SkipTitle -UITest=write` ouvre le menu
  pause et l'écran Réglages, enregistre des réglages non par défaut et relit le fichier ;
  un second lancement avec `-UITest=verify` vérifie qu'ils ont persisté ;
  `-UITest=reset` remet les valeurs par défaut. Résultats : `Saved/Logs/ui_test.txt`,
  lignes finissant par `UITEST <étape> PASS` ou `FAIL`.
- Coût de la mini-carte : même parcours avec et sans `-Minimap=0` (ou `-NoMinimap`).
