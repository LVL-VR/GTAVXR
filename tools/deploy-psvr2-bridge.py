#!/usr/bin/env python3
"""Install the locally built PSVR2 bridge into an installed GTAVXR 1.0.2 Suite (run from WSL).

Before every Play, the Suite checks runtime/<version>/gtavxr_native_openxr.dll against the hash recorded in
versions/<version>.json. This script backs up both files, copies the build in, and records the new hash.
Run it with GTA and the Suite closed. Use --restore to put the official DLL and record back.
"""
import glob, hashlib, json, os, shutil, subprocess, sys, time

VERSION = "gtavxr-1-0-2"
DLL = "gtavxr_native_openxr.dll"
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILT = os.path.join(REPO, "build", "bridge", "Release", DLL)


def sha(p):
    with open(p, "rb") as f:
        return hashlib.sha256(f.read()).hexdigest().upper()


def find_root(argv):
    if argv:
        return argv[0]
    candidates = glob.glob("/mnt/c/Users/*/AppData/Local/GTAVXR/Suite")
    for d in "cdefgh":
        candidates += glob.glob(f"/mnt/{d}/GTAVXR*/Suite") + glob.glob(f"/mnt/{d}/*/GTAVXR*/Suite")
    found = [c for c in candidates if os.path.isfile(os.path.join(c, "versions", VERSION + ".json"))]
    if len(found) != 1:
        sys.exit(f"Suite data with versions/{VERSION}.json not found uniquely: {found}. Pass its path.")
    return found[0]


def running():
    # A killed Suite can linger as a few-KB husk that holds nothing; only count live processes.
    out = subprocess.run(["tasklist.exe", "/FO", "CSV", "/NH"], capture_output=True, text=True).stdout.lower()
    live = []
    for line in out.splitlines():
        cols = [c.strip('"') for c in line.split('","')]
        if len(cols) >= 5 and cols[0] in ("gta5.exe", "gtavxr.exe", "playgtav.exe", "gtavxr_sourcebaseline.exe"):
            kb = int("".join(ch for ch in cols[4] if ch.isdigit()) or 0)
            if kb > 1024:
                live.append(cols[0])
    return live


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    restore = "--restore" in sys.argv
    # --suite-open: the Suite re-reads versions/*.json from disk, so only GTA itself must be closed.
    busy = [n for n in running() if "--suite-open" not in sys.argv or n != "gtavxr.exe"]
    if busy:
        sys.exit(f"Close these first: {', '.join(busy)}")
    root = find_root(args)
    manifest = os.path.join(root, "versions", VERSION + ".json")
    target = os.path.join(root, "runtime", VERSION, DLL)
    backup = os.path.join(root, "psvr2-backup")
    if restore:
        for name, dest in ((DLL, target), (VERSION + ".json", manifest)):
            shutil.copy2(os.path.join(backup, name), dest)
        print(f"Restored official bridge: {sha(target)}")
        return
    if not os.path.isfile(BUILT):
        sys.exit(f"Build missing: {BUILT}")
    with open(manifest, encoding="utf-8-sig") as f:
        data = json.load(f)
    entry = next((e for e in data.get("runtime", []) if e.get("path") == DLL), None)
    if entry is None:
        sys.exit(f"{DLL} not listed in {manifest}")
    os.makedirs(backup, exist_ok=True)
    if not os.path.exists(os.path.join(backup, DLL)):  # keep the first (official) copy only
        shutil.copy2(target, os.path.join(backup, DLL))
        shutil.copy2(manifest, os.path.join(backup, VERSION + ".json"))
    shutil.copy2(BUILT, target)
    entry["sha256"], entry["bytes"] = sha(target), os.path.getsize(target)
    tmp = manifest + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)
    os.replace(tmp, manifest)
    print(f"Installed PSVR2 bridge {entry['sha256']} ({entry['bytes']} bytes)\n  {target}\n  backup: {backup}")


if __name__ == "__main__":
    main()
