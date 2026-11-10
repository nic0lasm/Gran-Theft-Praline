# Budgets de contenu du profil M1

*English version: [content-budgets.md](content-budgets.md)*

Chaque asset doit arriver dans ces limites ; le M1 Pro n'a pas de Nanite, ce
sont donc les LOD, l'instanciation et le culling qui font le travail. Valeurs
de départ tirées des mesures du 10/11/2026, affinées au banc
(`tools/perf/bench.sh`).

| Famille | Triangles LOD0 | LOD | Matériaux | Fin de culling | Ombres |
| ---- | ---- | ---- | ---- | ---- | ---- |
| Module de façade | ≤ 2 k (≤ 50 k pièce maîtresse) | 4 (100/50/15/5 %) | ≤ 2, atlas | 250 m (enseignes 150 m) | oui ; aucune au LOD lointain |
| Grand objet (> 3 m) | ≤ 30 k | 4 + imposteur | ≤ 2 | 250 m | oui |
| Objet moyen (1-3 m) | ≤ 15 k | 3 | ≤ 2 | 120 m | oui |
| Petit objet (< 1 m) | ≤ 5 k | 3 | 1 | 80 m | aucune sous 0,5 m |
| Détail de rue | ≤ 3 k | 2 | 1, atlas partagé | 40-60 m | aucune |
| Piéton (complet) | ≤ 40 k | 4 | ≤ 3 | complet < 20 m, réduit 20-90 m, foule lointaine instanciée | < 20 m |
| Véhicule | ≤ 80 k | 4 | ≤ 4 | 250 m | < 40 m |
| Arbre | ≤ 60 k | 4 + billboard | ≤ 2 | 400 m | < 60 m |

Règles :

- Textures : 2K maximum en M1 (la 4K reste dans les sources pour le profil
  Ultra) ; le pool de streaming fait 1,5 Go.
- Opaque par défaut ; translucidité seulement avec un budget mesuré (vitrines :
  ≤ 0,8 ms pour toute la rue, ou le repli opaque « fausse profondeur »).
- Un HISM par maillage et matériau, jamais des milliers d'acteurs isolés.
- Tout ajout de système ou de contenu s'accompagne d'une passe de banc
  avant/après ; `REFERENCE=<results.csv>` fait échouer la passe si le p95
  régresse de plus de 2 ms ou si une image dépasse 100 ms.
