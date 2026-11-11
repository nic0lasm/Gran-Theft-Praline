# Règle des vraies marques pour les builds partagées

*Version anglaise : [brands-rule.md](brands-rule.md).*

Le quartier est généré à partir de données ouvertes : ses commerces portent
leur **vrai nom** (OpenStreetMap). Le logo d'un commerce est une œuvre de
marque, ce qui est autre chose. La règle vaut pour toute build qui quitte les
machines du propriétaire, même en privé.

## Règle

| Élément | Dépôt public | Build de test privée (Nico et proches) | Sortie publique |
| ---- | ---- | ---- | ---- |
| Nom réel du commerce en simple lettrage (texte généré, `T_Sign_*`) | Non commité (table OSM dérivée, ODbL) | Autorisé | Autorisé (faits, pas d'œuvre) |
| Logo (`T_SignLogo_*`, `Plugins/CityBrands/`) | **Jamais** | **Désactivé par défaut** ; activé seulement par décision explicite de Nico pour une liste nommée de testeurs | **Jamais** |
| Couleurs de marque | — | Autorisées (une couleur n'est pas un logo) | Autorisées |
| Personnes réelles, marques de voitures réelles avec leurs badges | Jamais | Jamais | Jamais |

Conséquences :

1. La **build de test par défaut est la variante sans marques** :
   `Plugins/CityBrands` n'est pas monté, donc les enseignes n'ont que le
   lettrage. C'est l'état que laisse `tools/city/unreal/mount_citybrands.sh`
   quand on ne le lance pas.
2. Une build avec logos n'est produite que sur demande, marquée `-logos` dans
   le nom de l'archive (`GranTheftPraline-0.9.0-rc1-macos-logos.zip`),
   consignée dans `docs/releases/` avec les personnes à qui elle a été
   donnée, et jamais déposée dans un lieu public.
3. `tools/qa/check_release_bundle.py`
   doit ne trouver aucun `T_SignLogo_*` dans une archive par défaut.
4. Les noms de commerces restent factuels. Aucun texte d'enseigne n'est
   inventé pour se moquer d'une marque réelle.

## Décision en attente

Nico choisit entre : (1) lettrage seul dans toute build partagée
(recommandé, c'est la règle ci-dessus) ; (2) noms fictifs à la place des vrais ;
(3) logos pour des testeurs nommés. Tant qu'il n'a pas répondu, l'option 1
s'applique.
