# Git LFS

*Version anglaise : [git-lfs.md](git-lfs.md).*

## Règles

- Tout fichier binaire passe par Git LFS. Les motifs sont dans
  `.gitattributes` : assets Unreal (`.uasset`, `.umap`, `.ubulk`, `.uexp`),
  3D (`.fbx`, `.glb`, `.gltf`, `.obj`, `.usd*`, `.abc`, `.blend`), images,
  audio, vidéo, polices, données géo et archives.
- Le texte reste dans Git normal : code, `.ini`, `.uproject`, Markdown,
  CSV, scripts.
- Jamais versionnés : `Binaries/`, `Intermediate/`, `Saved/`,
  `DerivedDataCache/`, jeux empaquetés (`Builds/`), données géo brutes
  (`assets/geo/raw/`), masters Tripo haute qualité (stockés hors Git,
  emplacement à décider).
- Avant un push qui ajoute des binaires, vérifier ce qui part en LFS :
  `git lfs status` et `git lfs ls-files --size`.

## Quota

GitHub Free donne 10 Gio de stockage LFS et 10 Gio de bande passante LFS
par mois. La consommation se lit sur GitHub dans Settings > Billing and
licensing > Usage (produit « Git LFS »). Règle : rester sous 70 % de chacun ;
au-delà, le fil qui le constate présente une carte à Nico (élaguer, acheter
un pack de données ou passer à un autre stockage). Les chiffres à jour sont
suivis dans `production/roadmap-status.fr.md` (Indicateurs).
