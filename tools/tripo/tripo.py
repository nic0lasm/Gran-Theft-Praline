#!/usr/bin/env python3
"""Minimal Tripo (https://platform.tripo3d.ai) API client for the asset pipeline.

Commands:
  balance                       show remaining credits (free, no generation)
  text  "prompt" --name NAME    text -> 3D, downloads GLB into assets/tripo/
  image PATH --name NAME        image -> 3D, downloads GLB into assets/tripo/

The key is read from env TRIPO_API_KEY, else from .scratch/tripo.env
(line: TRIPO_API_KEY=tsk_...). The key is never printed or committed.
Only the Python standard library is used.
"""
import argparse, datetime, json, mimetypes, os, sys, time, urllib.request, urllib.error, uuid
from pathlib import Path

API = "https://api.tripo3d.ai/v2/openapi"
ROOT = Path(__file__).resolve().parents[2]
ENV_FILES = [ROOT.parent / ".scratch" / "tripo.env", ROOT / ".scratch" / "tripo.env"]


def load_key():
    key = os.environ.get("TRIPO_API_KEY", "").strip()
    if key:
        return key
    for f in ENV_FILES:
        if f.is_file():
            for line in f.read_text().splitlines():
                line = line.strip()
                if line.startswith("TRIPO_API_KEY="):
                    return line.split("=", 1)[1].strip().strip("'\"")
    sys.exit("TRIPO_API_KEY not found (env var or .scratch/tripo.env).")


def call(method, path, key, body=None, data=None, headers=None):
    h = {"Authorization": "Bearer " + key}
    if body is not None:
        data = json.dumps(body).encode()
        h["Content-Type"] = "application/json"
    h.update(headers or {})
    req = urllib.request.Request(API + path, data=data, headers=h, method=method)
    try:
        with urllib.request.urlopen(req, timeout=60) as r:
            out = json.load(r)
    except urllib.error.HTTPError as e:
        sys.exit("HTTP %d: %s" % (e.code, e.read().decode(errors="replace")[:300]))
    if out.get("code") != 0:
        sys.exit("API error: %s" % json.dumps(out)[:300])
    return out["data"]


def upload_image(path, key):
    p = Path(path)
    boundary = uuid.uuid4().hex
    ctype = mimetypes.guess_type(p.name)[0] or "image/png"
    payload = (
        ("--%s\r\nContent-Disposition: form-data; name=\"file\"; filename=\"%s\"\r\n"
         "Content-Type: %s\r\n\r\n" % (boundary, p.name, ctype)).encode()
        + p.read_bytes() + ("\r\n--%s--\r\n" % boundary).encode())
    d = call("POST", "/upload", key, data=payload,
             headers={"Content-Type": "multipart/form-data; boundary=" + boundary})
    return d["image_token"], p.suffix.lstrip(".").lower().replace("jpg", "jpeg")


def run_task(key, body):
    tid = call("POST", "/task", key, body=body)["task_id"]
    print("task", tid)
    while True:
        t = call("GET", "/task/" + tid, key)
        st = t["status"]
        print(" ", st, t.get("progress", ""))
        if st == "success":
            return t
        if st in ("failed", "cancelled", "unknown", "banned", "expired"):
            sys.exit("task ended: " + st)
        time.sleep(5)


def download(url, dest):
    dest.parent.mkdir(parents=True, exist_ok=True)
    with urllib.request.urlopen(url, timeout=300) as r:
        dest.write_bytes(r.read())


def register(dest, origin, source, notes):
    row = "| %s | %s | %s | %s | Tripo paid plan (commercial) | %s |\n" % (
        datetime.date.today(), dest.relative_to(ROOT), origin,
        source.replace("|", "/"), notes)
    with open(ROOT / "assets" / "SOURCES.md", "a") as f:
        f.write(row)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("balance")
    t = sub.add_parser("text"); t.add_argument("prompt"); t.add_argument("--name", required=True)
    i = sub.add_parser("image"); i.add_argument("path"); i.add_argument("--name", required=True)
    a = ap.parse_args()
    key = load_key()

    if a.cmd == "balance":
        b = call("GET", "/user/balance", key)
        print("balance:", b.get("balance"), "frozen:", b.get("frozen"))
        return
    if a.cmd == "text":
        body = {"type": "text_to_model", "prompt": a.prompt}
        source = "text: " + a.prompt
    else:
        token, ext = upload_image(a.path, key)
        body = {"type": "image_to_model", "file": {"type": ext, "file_token": token}}
        source = "image: " + Path(a.path).name
    task = run_task(key, body)
    out = task["output"]
    url = out.get("pbr_model") or out.get("model")
    dest = ROOT / "assets" / "tripo" / (a.name + ".glb")
    download(url, dest)
    register(dest, "Tripo API (draft/props, upstream only)", source, "task " + task["task_id"])
    print("saved", dest)


if __name__ == "__main__":
    main()
