"""Set the usage flags the cooked game needs on the city materials already in the project,
without regenerating anything (Windows package of 11/11: 280 materials saved without
"used with Nanite" drew with the default material under DX12 with Nanite on).

The generation scripts set these flags when they create a material (apply_city_materials.py,
place_facades.py: MATERIAL_USAGES); this script fixes the assets saved before that, in place:
for every Material (not instance: instances use their parent's shaders) under the folders of
GTP_USAGE_ROOTS (default /Game and the brand signs plugin: an instance takes its parent's usage, and the parents of the city instances are not all under the city folders), it sets the missing
flags, recompiles and saves. Maps are not opened or changed.

Report: Saved/CityImport/usage_report.json {"roots", "materials", "fixed", "already", "errors"}.
Run headless through gtp_run (see tools/city/unreal/editor_run.sh):
  GTP_PY_SCRIPT=tools/city/unreal/fix_material_usage.py UnrealEditor GranTheftPraline.uproject -nullrhi \
      -unattended "-ExecutePythonScript=__import__('gtp_run').main()"
"""

import json
import os
import time

import unreal

USAGES = ("used_with_nanite", "used_with_instanced_static_meshes")
ROOTS = os.environ.get("GTP_USAGE_ROOTS", "/Game /CityBrands").split()   # parents of MI_City_* live elsewhere too

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def main():
    t0 = time.time()
    report = {"roots": ROOTS, "materials": 0, "fixed": [], "already": 0, "errors": []}
    for root in ROOTS:
        if not eal.does_directory_exist(root):
            continue
        for path in eal.list_assets(root, recursive=True):
            try:
                data = eal.find_asset_data(path)
                if str(data.asset_class_path.asset_name) != "Material":
                    continue
                mat = eal.load_asset(path)
                report["materials"] += 1
                missing = [f for f in USAGES if not mat.get_editor_property(f)]
                if not missing:
                    report["already"] += 1
                    continue
                for f in missing:
                    mat.set_editor_property(f, True)
                mel.recompile_material(mat)
                if eal.save_loaded_asset(mat, False):
                    report["fixed"].append(mat.get_path_name())
                else:
                    report["errors"].append("not saved: " + path)
            except Exception as e:  # keep going, report it
                report["errors"].append("%s: %r" % (path, e))
    report["seconds"] = round(time.time() - t0, 1)
    out = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "CityImport")
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, "usage_report.json"), "w") as f:
        json.dump(report, f, indent=1)
    unreal.log_warning("[MaterialUsage] %d materials, %d fixed, %d errors"
                       % (report["materials"], len(report["fixed"]), len(report["errors"])))


main()
