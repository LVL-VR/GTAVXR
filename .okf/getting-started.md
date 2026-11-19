---
type: Reference
title: GTAVXR on PSVR2 — getting started
description: Entry point for the PSVR2/SteamVR adaptation of GTAVXR 1.0.2, its install, and the known Suite issues.
tags: [gtavxr, psvr2, steamvr, openxr, overview]
timestamp: '2026-11-18T03:30:00Z'
---

# Overview

This fork of [LVL-VR/GTAVXR](https://github.com/LVL-VR/GTAVXR) 1.0.2 makes the OpenXR bridge work well with
**PSVR2 Sense through SteamVR**. GTAVXR already used OpenXR and supported SteamVR. The work here is on the
PSVR2 input and on getting the Suite to install and launch reliably.

Local branch: `psvr2-steamvr`. Nothing has been pushed upstream.

# What only the fork can change

| Part | Rebuildable here? | Notes |
| --- | --- | --- |
| `code/bridge` (gtavxr_native_openxr.dll) | Yes | CMake + MSVC; all PSVR2 input work lives here. |
| `code/vr` (game-side VR logic) | No | Compiled into the delta-patched `GTA5.exe` (`GTAVXR_SourceBaseline.exe`). No build files. |
| `code/suite` (launcher/installer) | Not as a playable build | The public source builds a test Suite only (stub content provider); Suite fixes are proposed as source changes for LVL. |

# Map

* [PSVR2 through SteamVR](/hardware/psvr2-steamvr.md) — measured runtime identity, bindings, and audio.
* [Input pipeline](/architecture/input-pipeline.md) — bridge → GxInput → game, plus the SteamVR menu and D-pad chords.
* [PSVR2 input remap](/fixes/psvr2-input-remap.md) — Triangle-touch D-pad, the Create menu, and other headsets left unchanged.
* [Suite elevated leftover processes](/fixes/suite-elevated-leftover-processes.md) — "Access is denied" on Play.
* [Bridge build and deploy](/operations/bridge-build-deploy.md) — build, install into the Suite, hash record.
* [Install and session recovery](/operations/install-session-recovery.md) — killed install, INSTALL_REQUIRED, admin vs normal.

The player-facing controls and setup are in `docs/PSVR2.md` (repo root).

# Status (2026-11-18)

* Install complete on the maintainer machine. Game E:\SteamLibrary\steamapps\common\Grand Theft Auto V (Legacy
  1.0.3889.0, GTA5.exe SHA256 677E4E35…). Suite data at %LOCALAPPDATA%\GTAVXR\Suite.
* Bridge with the PSVR2 remap is deployed. In the headset, the D-pad shift is confirmed, and Create reaches the game.
* Pending headset checks:
  * Create opens the VR menu through the L3+R3 chord.
  * Whether the phone comes up on D-pad up only, or also from a stray R3 tap.
  * Game audio.
  * The leftover-process fix once a Suite build includes it.
