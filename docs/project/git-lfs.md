# Git LFS

*French version: [git-lfs.fr.md](git-lfs.fr.md).*

## Rules

- Every binary file goes through Git LFS. The patterns live in
  `.gitattributes`: Unreal assets (`.uasset`, `.umap`, `.ubulk`, `.uexp`),
  3D (`.fbx`, `.glb`, `.gltf`, `.obj`, `.usd*`, `.abc`, `.blend`), images,
  audio, video, fonts, geodata and archives.
- Text stays in plain Git: code, `.ini`, `.uproject`, Markdown, CSV,
  scripts.
- Never committed: `Binaries/`, `Intermediate/`, `Saved/`,
  `DerivedDataCache/`, packaged games (`Builds/`), raw geodata
  (`assets/geo/raw/`), high-quality Tripo masters (stored outside Git,
  location to be decided).
- Before a push that adds binaries, check what goes to LFS:
  `git lfs status` and `git lfs ls-files --size`.

## Quota

GitHub Free gives 10 GiB of LFS storage and 10 GiB of LFS bandwidth per
month. Usage is shown on GitHub in Settings > Billing and licensing > Usage
(product "Git LFS"). Rule: stay under 70 % of each; above that, the thread
that notices it puts a decision card to Nico (prune, buy a data pack, or
move to other storage). Current figures are tracked in
`production/roadmap-status.md` (Indicators).
