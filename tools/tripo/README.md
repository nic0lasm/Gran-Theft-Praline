# tools/tripo — Tripo API client (EN)

Upstream drafts and props only. Final assets stay Megascans / Fab / MetaHuman.

## Get a key
1. Sign in at https://platform.tripo3d.ai → API Keys → Create (`tsk_...`).
2. Paid plan (~$20/month) required so generated models can be owned/used commercially (free plan = no commercial rights).

## Store the key (never committed)
Env var `TRIPO_API_KEY`, or file `GTAL/.scratch/tripo.env` containing `TRIPO_API_KEY=tsk_...` (also `.scratch/tripo.env` inside the repo; both git-ignored).

## Use
```
python3 tools/tripo/tripo.py balance            # free: shows credits
python3 tools/tripo/tripo.py text "wooden market crate" --name market_crate
python3 tools/tripo/tripo.py image ref.png --name lamp_post
```
`text`/`image` consume credits. The GLB lands in `assets/tripo/` and a row is appended to `assets/SOURCES.md`.

# tools/tripo — client API Tripo (FR)

Brouillons et props en amont uniquement. Les assets finaux restent Megascans / Fab / MetaHuman.

## Obtenir une clé
1. Se connecter sur https://platform.tripo3d.ai → API Keys → Create (`tsk_...`).
2. Plan payant (~20 $/mois) nécessaire pour posséder les modèles générés (plan gratuit = pas de droits commerciaux).

## Enregistrer la clé (jamais versionnée)
Variable `TRIPO_API_KEY`, ou fichier `GTAL/.scratch/tripo.env` contenant `TRIPO_API_KEY=tsk_...` (git-ignoré).

## Utilisation
Voir les commandes ci-dessus. `balance` est gratuit ; `text` et `image` consomment des crédits. Le GLB arrive dans `assets/tripo/` et une ligne est ajoutée à `assets/SOURCES.md`.
