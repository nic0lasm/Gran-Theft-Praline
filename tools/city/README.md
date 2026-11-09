# City pipeline

Turns open data (IGN BD TOPO and LiDAR HD, Metropole de Lyon open data,
OpenStreetMap) into engine-ready tables, a LiDAR-fitted greybox with facade
layouts and real shop signs, and a terrain heightmap.
Plans: [docs/city/plan.md](../../docs/city/plan.md) (sources) and
[docs/city/generation.md](../../docs/city/generation.md) (roofs, facades, measurements).

`fetch.py --skip-lidar --skip-grandlyon` and `build.py` use the standard library only;
the rest needs `pip install -r tools/city/requirements.txt`.

```
python3 tools/city/fetch.py --name presqu-ile --bbox 45.7570,4.8250,45.7690,4.8420
python3 tools/city/build.py --name presqu-ile
python3 tools/city/generate.py --name presqu-ile --focus "Rue de la République" --buffer 45
python3 tools/city/terrain.py --name presqu-ile
python3 tools/city/render.py --name presqu-ile --view street:176,-60,1.8,179,150,9
python3 -I -m unittest discover -s tools/city/tests
```

Facade kit (Blender 4.2+ or the `bpy` module, Python 3.11):
`python tools/city/facade_kit/assemble.py --view street:176,-60,1.8,179,150,9`
(or `blender -b -P tools/city/facade_kit/assemble.py -- --view ...`).

Editor (one Unreal process at a time on the Mac): `unreal/import_city.py` then
`unreal/place_facades.py`, see their docstrings.

Raw and processed data live in `assets/geo/` and are not committed.
