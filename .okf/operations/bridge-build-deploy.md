---
type: Runbook
title: Build and deploy the GTAVXR bridge
description: Build gtavxr_native_openxr.dll with MSVC from WSL, install it into the Suite runtime, and update the hash the Suite checks.
tags: [bridge, build, deploy, msvc, suite]
timestamp: '2026-11-18T03:30:00Z'
---

# Build

```bash
"/mnt/c/Program Files/CMake/bin/cmake.exe" -S code/bridge -B build/bridge -G "Visual Studio 17 2022" -A x64 \
  -DOPENXR_INCLUDE_DIR="F:/ai/GITHUB/CheekyFoveatedDLSS/third_party/openxr/include"
"/mnt/c/Program Files/CMake/bin/cmake.exe" --build build/bridge --config Release
```

* The static CRT (`CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded`, pinned in CMakeLists.txt) matches the shipped DLL.
  Imports are only GDI32/USER32/KERNEL32, and all 32 exports are identical to the official build.
* The Windows-side CMake must be used from WSL. Sources are on F:, so the paths work for both sides.

# Deploy

```bash
python3 tools/deploy-psvr2-bridge.py --suite-open   # GTA must be closed; the Suite window may stay open
python3 tools/deploy-psvr2-bridge.py --restore      # official DLL and record back
```

The Suite asserts every runtime file before Play (`EmbeddedInstall.cs:133`, `INSTALL_RUNTIME_CHANGED`). The
script therefore:

1. Copies the DLL to `Suite\runtime\gtavxr-1-0-2\`.
2. Rewrites that entry's `sha256`/`bytes` in `Suite\versions\gtavxr-1-0-2.json`. `contract_sha256` covers only
   suite-launch.json, so it is unaffected.
3. Backs the originals up once to `Suite\psvr2-backup\`.

# Gotchas

* While GTA runs it holds the DLL locked (PermissionError). The script refuses when a live game process exists,
  and it also recognises `gtavxr_sourcebaseline.exe`.
* A reinstall verifies runtime blobs against the version record. Restore the official files **before** a
  reinstall, and redeploy after it.
* Python writes LF. Keep the original CRLF in session.h/actions.h to avoid whole-file diffs.
* A restart is needed for SteamVR to see new actions. A saved personal binding needs the new actions mapped by
  hand; see [PSVR2 through SteamVR](/hardware/psvr2-steamvr.md).
