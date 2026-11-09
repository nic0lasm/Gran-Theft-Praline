# City pipeline

Turns open data (IGN BD TOPO, OpenStreetMap) into engine-ready tables.
Python 3, standard library only. Full plan: [docs/city/plan.md](../../docs/city/plan.md).

```
python3 tools/city/fetch.py --name presqu-ile --bbox 45.7570,4.8250,45.7690,4.8420
python3 tools/city/build.py --name presqu-ile
python3 -I -m unittest discover -s tools/city/tests
```

Raw and processed data live in `assets/geo/` and are not committed.
