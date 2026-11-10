# tools/tripo: Tripo API client (EN)

Upstream drafts and props only; final assets stay Megascans / Fab / MetaHuman. Uses the Tripo API **v3**.

Two separate credit wallets exist: **Studio** (web app) credits are spent only in the Studio web interface; **API** credits (this tool, the CLI, the MCP) are bought or claimed separately in the developer console.

## Key
Create a key (`tsk_...`) in the developer console (API Keys). Store it as env var `TRIPO_API_KEY` or in `.scratch/tripo.env` (`TRIPO_API_KEY=tsk_...`), both git-ignored. The `tcli_...` client id is not an API key.

## Use
```
python3 tools/tripo/tripo.py balance            # free
python3 tools/tripo/tripo.py text "weathered wooden crate" --name crate
python3 tools/tripo/tripo.py image ref.png --name lamp_post
```
`text` and `image` spend API credits, download the GLB to `assets/tripo/api/` (out of Git) and append a row to `assets/SOURCES.md`. Env overrides: `TRIPO_API_BASE`, `TRIPO_MODEL`. Do not put dimensions in prompts (numbers get painted on textures).

# tools/tripo : client de l'API Tripo (FR)

Brouillons et props en amont ; les assets finaux restent Megascans / Fab / MetaHuman. Utilise l'API **v3**.

Deux portefeuilles de crédits distincts : les crédits **Studio** (interface web) ne se dépensent que dans Studio ; les crédits **API** (cet outil, le CLI, le MCP) s'achètent ou se réclament à part dans la console développeur.

## Clé
Créer une clé (`tsk_...`) dans la console développeur (API Keys), la stocker dans la variable `TRIPO_API_KEY` ou dans `.scratch/tripo.env` (`TRIPO_API_KEY=tsk_...`), tous deux git-ignorés. L'identifiant client `tcli_...` n'est pas une clé API.

## Utilisation
Voir les commandes ci-dessus. `balance` est gratuit ; `text` et `image` dépensent des crédits API, téléchargent le GLB dans `assets/tripo/api/` (hors Git) et ajoutent une ligne à `assets/SOURCES.md`. Variables : `TRIPO_API_BASE`, `TRIPO_MODEL`. Aucune dimension dans les prompts.
