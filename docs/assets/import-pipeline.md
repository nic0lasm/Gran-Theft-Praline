# Tripo to Unreal import pipeline

*French version: [import-pipeline.fr.md](import-pipeline.fr.md). Rules followed: [art bible](../art/art-bible.md) sections 5-8. Catalog: [asset-catalog.md](../../production/asset-catalog.md).*

Goal: turn a Tripo master GLB (about 1 M vertices, 55-88 MB) into one game-ready
Unreal static mesh per prop, valid for both rendering profiles, with no manual step.

Status: the preparation half (Python, outside Unreal) is tested in CI on a procedural
sample. The Unreal half (`import_props.py`) is written against the 5.8 Python API but
**not yet run in the editor**: first run on the Mac is the next step, on the
coordinator's go (one editor session at a time).

## Flow

```
assets/tripo/w1/W1-07*.glb            masters, out of Git
        |  tools/assets/prepare_props.py   (any machine, Python only)
        v
assets/staging/w1/W1-07/              out of Git, regenerable
   asset.json  W1-07_ultra.glb  textures/T_*_BC|ORM|N.png
        |  tools/assets/import-props-mac.sh   (Unreal editor closed)
        v
Content/Environment/Props/<Family>/<Asset>/
   SM_<Asset>_A   T_<Asset>_BC / _N / _ORM   MI_<Asset>
```

## What the preparation does

1. **Normalise**: scale so the longest dimension equals the catalog size (`size_m`, the
   largest number of the Size column), pivot at the bottom centre (`back_center` for
   wall-mounted objects: centred in X/Y, flush with the back face), optional
   `rotate_y_deg`. glTF conventions are kept (Y up, +Z front, metres): Unreal's
   importer converts to Z up and centimetres. The art bible wants the front on +X in
   Unreal: to check on the first import, then fix with a global `rotate_y_deg`.
2. **Cap the Nanite source** to the art bible 5.1 budget by size class (small under
   1 m: 300 k triangles; medium 1-3 m: 1 M; large: 2 M) with attribute-aware quadric
   decimation (meshoptimizer; UVs and normals steer the collapses). Masters under the
   cap are kept whole.
3. **Textures** next to the mesh: `T_<Asset>_BC` (sRGB), `_ORM` (R occlusion, G roughness,
   B metallic, linear, same packing as glTF) and `_N`. A missing map becomes a 4x4
   constant from the material factors. Never downscaled (cap 4096 px): the M1 tier
   saves memory through streaming and texture LOD groups (ADR-0001, art bible 5.2).
4. **Collision recipe**: none for flat objects (under 3 cm) or by override; a box for
   small props; otherwise a convex decomposition whose hull count (1, 4 or 8) follows
   how far the object is from convex.
5. **LOD plan** (art bible 5.1 and 6): small props 3 LODs, others 4; M1 LOD0 budget
   3 k / 10 k / 30 k triangles; screen sizes 1.0 / 0.5 / 0.25 / 0.1; about 50 % fewer
   triangles per LOD.
6. Names come from `assets/manifests/wave-01.json` (family, `Kind_Descriptor`), generated
   from the catalog by `catalog_to_manifest.py`.

Re-running is incremental: an asset whose master and settings are unchanged is skipped
(`--force` to redo).

## What the Unreal script does

One `SM_<Asset>_A` per prop (art bible 6.1): imports the three textures with the right
flags, creates `MI_<Asset>` from `M_Master_Surface` (created at
`/Game/Materials/Masters/` if absent, parameters `BaseColor`, `ORM`, `Normal`), imports
the mesh, builds collision from the recipe, enables Nanite with the fallback set to the
M1 LOD0 budget, generates the LOD chain from the plan, tags `CatalogId`, and deletes the
importer's placeholder materials. Re-running replaces assets. One failing asset does not
stop the batch; the exit code tells.

## Usage

```bash
pip install -r tools/assets/requirements.txt
python3 tools/assets/prepare_props.py --manifest assets/manifests/wave-01.json \
    --masters assets/tripo/w1 --staging assets/staging/w1 [--ids W1-01 W1-07] [--force]
tools/assets/import-props-mac.sh w1 [W1-01 W1-07]      # Unreal editor closed
```

Masters are matched by id prefix (`W1-07.glb`). Missing ones are listed in
`assets/staging/w1/report.json` (`no_master_yet`).

## Open points

- **To verify on the first Mac run**: front axis (+X), that M1 (Nanite off) really falls
  back to LOD0 fallback plus the chain, and the exact Python API names of 5.8
  (`StaticMeshEditorSubsystem`). The script is written to fail loudly per asset.
- **Not covered**: texture rebake to the section 4.4 density and albedo recalibration
  (de-lit, varied roughness): the master's textures are used as they come; a later
  Blender pass handles hand cleanup. Alpha-tested objects need a masked master material.
- Imported `.uasset` files and textures go to Git LFS; watch the quota
  ([git-lfs.md](../project/git-lfs.md)).
- No Blender dependency: trimesh and meshoptimizer run in CI and on the Mac alike.
