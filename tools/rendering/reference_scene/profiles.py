"""Resolve a rendering profile from profiles.json.

Usage: profiles.py <profile>   prints the console variables as "name value, ..."
"""

import json
import os
import sys

PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "profiles.json")


def load(name):
    """Return the ordered dict of console variables of a profile (base first)."""
    with open(PATH) as f:
        profiles = json.load(f)
    if name not in profiles or name.startswith("_"):
        raise KeyError("unknown profile %r (known: %s)" % (
            name, ", ".join(k for k in profiles if not k.startswith("_"))))
    profile = profiles[name]
    cvars = dict(load(profile["base"])) if "base" in profile else {}
    cvars.update(profile["cvars"])
    return cvars


if __name__ == "__main__":
    try:
        print(", ".join("%s %s" % kv for kv in load(sys.argv[1]).items()))
    except (IndexError, KeyError) as exc:
        sys.exit("profiles.py: %s" % exc)
