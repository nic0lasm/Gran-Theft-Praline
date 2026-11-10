# ADR-0007 : Textes, réglages et mini-carte de l'interface

*English version: [adr-0007-interface-settings-minimap.md](adr-0007-interface-settings-minimap.md)*

## Statut

Proposé le 2026-11-10 ; appliqué par défaut tant que le responsable du projet ne décide pas autrement.

## Date

2026-11-10

## Décideurs

nic0lasm (responsable du projet)

## Résumé

Comment le jeu tient ses textes en français et en anglais, enregistre les réglages du
joueur et dessine une mini-carte, avec le moins de couplage possible avec les systèmes
construits en parallèle (HUD, trafic, police, piétons, profils de rendu).

## Décisions

1. **Textes : une table compilée**, `GTPStringTable.h` (clé, français, anglais), lue par
   `GTPLoc`. Pas le tableau de localisation d'Unreal ni des fichiers `.po` : pour quelques
   centaines de textes courts en deux langues, une table dans le code se relit dans un
   diff, n'exige aucune étape d'éditeur sur le Mac et se vérifie en CI avec un simple
   compilateur. La table n'a aucun type Unreal : elle pourra passer en fichiers de
   données sans toucher aux appelants. Le français est le texte de référence.
2. **Réglages : un petit fichier texte** (`Saved/Config/GTPSettings.ini`, `clé=valeur`),
   valeurs et format dans `GTPSettingsCore.h`, chargé par `UGTPUserSettings`. Pas
   `UGameUserSettings` : il porte des groupes de qualité du moteur qui concurrenceraient
   les profils de l'ADR-0002. Les réglages graphiques **appellent**
   `UGTPGraphicsProfile::SetProfile` avec un nom de profil ; ils ne posent jamais de cvar
   eux-mêmes, qui restent à la charge du travail d'optimisation.
3. **Commandes : AZERTY / QWERTY / automatique, sensibilité, inversion verticale** pour
   l'instant ; réassignation touche par touche plus tard via les réglages utilisateur
   d'Enhanced Input.
4. **Mini-carte : dessinée sur le canvas** à partir de polylignes générées depuis les
   tables de la ville (`tools/ui/minimap.py`), dans les coordonnées locales de la zone
   partagées avec les voies du trafic. Un appel ajouté à `AGTPHUD::DrawHUD` ; tout le reste
   vit dans son propre sous-système. Les autres systèmes ajoutent des pastilles par
   `AddBlip`.

## Conséquences

- `UGTPGraphicsProfile` gagne `SetProfile` et mémorise les valeurs de cvars qu'il remplace,
  donc un retour à Ultra les restaure (petite modification d'un fichier que possède le
  travail d'optimisation, signalée à ce fil).
- Les textes des écrans titre et pause viennent maintenant de la table.
- Les aides des autres systèmes passeront à `GTPLoc` quand on les touchera.
- Les données de la mini-carte dérivent de la BD TOPO et restent hors de Git, comme les
  voies du trafic.

## Alternatives écartées

- Localisation Unreal (tables de chaînes, `.po`) : flux plus lourd, étapes d'éditeur, pas de contrôle en CI.
- `UGameUserSettings` : entre en conflit avec le mécanisme de profils.
- Mini-carte en render target (seconde capture de scène) : coûte une vue complète de plus
  sur un M1 Pro déjà limité par le GPU ; les lignes du canvas coûtent une fraction de
  milliseconde. Une texture pré-rendue est le repli si le coût du canvas dépasse son budget.

## Validation

`tools/ui/tests/run_cpp_tests.sh` (job CI « Interface core tests ») ; preuves en jeu sur
le Mac dans `docs/ui/test-results/`.
