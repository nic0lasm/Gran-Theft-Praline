#!/usr/bin/env python3
"""Minimal Tripo API (v3) client for the asset pipeline.

Commands:
  balance                       show API credits (free, generates nothing)
  text  "prompt" --name NAME    text -> 3D, downloads the GLB into assets/tripo/api/
  image PATH --name NAME        image -> 3D, downloads the GLB into assets/tripo/api/

These calls spend API credits, which are separate from Tripo Studio (web)
credits. The key is read from env TRIPO_API_KEY, else from .scratch/tripo.env
(line: TRIPO_API_KEY=tsk_...). It is never printed or committed.
Base URL can be overridden with TRIPO_API_BASE (default: international site).
Only the Python standard library is used.
"""
import argparse, datetime, json, mimetypes, os, sys, time, urllib.error, urllib.request, uuid
from pathlib import Path

BASE = os.environ.get("TRIPO_API_BASE", "https://openapi.tripo3d.ai/v3").rstrip("/")
MODEL = os.environ.get("TRIPO_MODEL", "v3.1-20260211")
ROOT = Path(__file__).resolve().parents[2]
ENV_FILES = [ROOT / ".scratch" / "tripo.env", ROOT.parent / ".scratch" / "tripo.env"]
FAILED = ("failed", "cancelled", "banned", "expired", "unknown")


def load_key():
    key = os.environ.get("TRIPO_API_KEY", "").strip()
    if not key:
        for f in ENV_FILES:
            if f.is_file():
                for line in f.read_text().splitlines():
                    if line.strip().startswith("TRIPO_API_KEY="):
                        key = line.split("=", 1)[1].strip().strip("'\"")
                        break
            if key:
                break
    if not key:
        sys.exit("TRIPO_API_KEY not found (env var or .scratch/tripo.env).")
    if not key.startswith("tsk_"):
        sys.exit("The API key must start with 'tsk_' (not the tcli_ client id).")
    return key


def call(method, path, key, body=None, data=None, headers=None):
    h = {"Authorization": "Bearer " + key}
    if body is not None:
        data = json.dumps(body).encode()
        h["Content-Type"] = "application/json"
    h.update(headers or {})
    req = urllib.request.Request(BASE + path, data=data, headers=h, method=method)
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
        ('--%s\r\nContent-Disposition: form-data; name="file"; filename="%s"\r\n'
         "Content-Type: %s\r\n\r\n" % (boundary, p.name, ctype)).encode()
        + p.read_bytes() + ("\r\n--%s--\r\n" % boundary).encode())
    d = call("POST", "/files", key, data=payload,
             headers={"Content-Type": "multipart/form-data; boundary=" + boundary})
    return d["file_token"]


def run_task(key, endpoint, body):
    tid = call("POST", endpoint, key, body=body)["task_id"]
    print("task", tid)
    while True:
        t = call("GET", "/tasks/" + tid, key)
        print(" ", t["status"], t.get("progress", ""))
        if t["status"] == "success":
            return tid, t
        if t["status"] in FAILED:
            sys.exit("task ended: " + t["status"])
        time.sleep(5)


def download(url, dest):
    dest.parent.mkdir(parents=True, exist_ok=True)
    with urllib.request.urlopen(url, timeout=300) as r:
        dest.write_bytes(r.read())


def register(dest, source, task_id):
    row = "| %s | %s | Tripo API v3 (draft/props, upstream only) | %s | Tripo paid plan (commercial) | task %s |\n" % (
        datetime.date.today(), dest.relative_to(ROOT), source.replace("|", "/"), task_id)
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
        b = call("GET", "/account/balance", key)
        print("balance:", b.get("balance"), "frozen:", b.get("frozen"))
        return
    if a.cmd == "text":
        endpoint = "/generation/text-to-model"
        body = {"prompt": a.prompt, "model": MODEL}
        source = "text: " + a.prompt
    else:
        endpoint = "/generation/image-to-model"
        body = {"input": upload_image(a.path, key), "model": MODEL}
        source = "image: " + Path(a.path).name
    tid, task = run_task(key, endpoint, body)
    dest = ROOT / "assets" / "tripo" / "api" / (a.name + ".glb")
    download(task["output"]["model_url"], dest)
    register(dest, source, tid)
    print("saved", dest)


if __name__ == "__main__":
    main()
