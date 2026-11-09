# Tripo to Unreal import pipeline

*French version: [import-pipeline.fr.md](import-pipeline.fr.md). Catalog: [asset-catalog.md](../../production/asset-catalog.md).*

Goal: turn a Tripo master GLB (about 1 M vertices, 64-74 MB) into game-ready
Unreal assets for both rendering profiles, with no manual step.

Status: the preparation half (Python, outside Unreal) is tested in CI on a
procedural sample. The Unreal half (`import_props.py`) is written against the
5.8 Python API but **not yet run in the editor**: first run on the Mac is the
next step, on the coordinator's go (one editor session at a time).

## Flow

```
assets/tripo/w1/W1-07*.glb            masters, out of Git
        |  tools/assets/prepare_props.py   (any machine, Python only, ~1 s per asset)
        v
assets/staging/w1/W1-07/              out of Git, regenerable
   asset.json  W1-07_ultra.glb  W1-07_m1.glb  textures/*.png
        |  tools/assets/import-props-mac.sh   (Unreal editor closed)
        v
Content/Props/W1/W1-07/  SM_W1_07 (M1, LODs)   MI_*   Textures/
Content/Props/W1/Ultra/  SM_W1_07_Ultra (Nanite, local only)
```

## What the preparation does

1. **Normalise**: scale so the longest dimension equals the catalog size
   (`size_m`, the largest number of the Size column), pivot at the bottom
   centre (`back_center` for wall-mounted objects: centred in X/Y, flush with
   the back face), optional `rotate_y_deg`. glTF conventions are kept (Y up,
   +Z front, metres): Unreal's importer converts to Z up and centimetres.
2. **Two geometries**:
   - `*_ultra.glb`: the full master, normalised (Ultra profile, Nanite).
   - `*_m1.glb`: decimated base mesh for the M1 profile. Attribute-aware
     quadric decimation (meshoptimizer; UVs and normals weigh in so seams and
     shading survive). Budget by size: under 1 m 6 000 triangles, under 3 m
     15 000, above 30 000; `m1_tris` overrides per object.
3. **Textures**: extracted next to the meshes as `BaseColor` (sRGB), `ORM`
   (R occlusion, G roughness, B metallic, linear, same packing as glTF) and
   `Normal`. A missing map becomes a 4x4 constant built from the material
   factors. **Textures are never downscaled** (cap 4096 px): the M1 profile
   saves memory through texture streaming and graphics settings, not by
   degrading assets (ADR-0001).
4. **Collision recipe** in `asset.json`: `none` for flat objects (thinner than
   3 cm: manhole, grate) or by override, otherwise a convex decomposition whose
   hull count (1, 4 or 8) follows how far the object is from convex.
5. **LOD plan**: 100 / 35 / 12 / 4 % of the M1 triangles at screen sizes
   1.0 / 0.45 / 0.20 / 0.08.

Re-running is incremental: an asset whose master and settings are unchanged is
skipped (`--force` to redo).

## What the Unreal script does

Per asset: imports the three textures with the right flags (sRGB, masks, normal
map), creates a material instance of `/Game/Props/M_Prop_PBR` (created on first
run: BaseColor, ORM, Normal parameters), imports the M1 mesh (Nanite off, LODs
generated in-engine from the base mesh, collision built from the recipe) and
the Ultra mesh (Nanite on). The importer's placeholder materials are deleted.
Replaces existing assets, so it is safe to re-run. One failing asset does not
stop the batch; the exit code tells.

## Usage

```bash
pip install -r tools/assets/requirements.txt
python3 tools/assets/catalog_to_manifest.py --out assets/manifests/wave-01.json   # only if the catalog changed
python3 tools/assets/prepare_props.py --manifest assets/manifests/wave-01.json \
    --masters assets/tripo/w1 --staging assets/staging/w1 [--ids W1-01 W1-07] [--force]
tools/assets/import-props-mac.sh w1 [W1-01 W1-07]      # Unreal editor closed
```

Masters are matched by id prefix (`W1-07.glb`, `W1-07_bin.glb`). Missing masters
are listed in `assets/staging/w1/report.json` (`no_master_yet`).

## Choices and open points

- **No Blender dependency**: trimesh + meshoptimizer run in CI and on the Mac
  alike. Blender stays optional for hand cleanup (catalog step "cleaned up").
- **Two meshes per prop** (`SM_x` and `SM_x_Ultra`): levels use `SM_x`; an
  Ultra build swaps in the `_Ultra` mesh by script. Ultra meshes stay out of Git
  (`.gitignore`): they weigh tens of MB each and are regenerable from the masters.
  The swap tool comes with the profile-switching work.
- **Opaque only**: alpha-tested objects (grates seen through, foliage) need a
  masked master material; to add when wave 1 shows which ones need it.
- **Orientation**: assumes Tripo faces +Z. If a model comes out turned,
  set `rotate_y_deg` in the manifest.
- Textures and the M1 meshes go to Git LFS; watch the quota
  ([git-lfs.md](../project/git-lfs.md)).
