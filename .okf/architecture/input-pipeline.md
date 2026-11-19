---
type: Architecture
title: GTAVXR input pipeline
description: How OpenXR actions become GTA pad input, and which gestures the closed game side requires on SteamVR.
tags: [input, openxr, abi, steamvr, gestures]
timestamp: '2026-11-18T03:30:00Z'
---

# Flow

1. **Bridge** (`code/bridge/actions.h`) creates the action set `gtavxr_controls`, suggests per-profile
   bindings (`interaction_profiles.h`), syncs, and fills the 184-byte `GxInput` POD (`input.h`).
   * buttons: `GX_A=1 B=2 X=4 Y=8 LCLICK=0x10 RCLICK=0x20 MENU=0x40`
   * `reserved` flags: `GX_INPUT_STEAMVR=1`, `GX_INPUT_BOTH_CLICKS=2`
   * There are **no D-pad bits** in the ABI.
2. **Game side** (`code/vr`, compiled into the patched exe, not rebuildable): `xr/locomotion.h Mapping::map`
   builds a virtual pad, and `input/button_map.h` remaps A/B/X/Y/L3/R3 per context (foot, vehicle, aircraft),
   including to D-pad targets.

# Gestures the game side defines

| Gesture | Effect |
| --- | --- |
| L3+R3 held + right stick past ±64 | Virtual D-pad (`shiftLatch` in locomotion.h) |
| L3+R3 pressed/released with centred sticks, **SteamVR** | Opens the VR menu (closed code, documented in docs/VR_CONTROLS.md) |
| Menu, **non-SteamVR runtimes** | Opens the VR menu. On SteamVR the Menu button is ignored by design. |
| Short Menu / long Menu / Menu+X (locomotion.h) | Start / recenter / Select |

So on SteamVR, getting a working menu button means replaying it as the centred L3+R3 chord in the bridge. See
[PSVR2 input remap](/fixes/psvr2-input-remap.md).

# Binding authority

The bridge never reads controllers directly. SteamVR's binding for the app decides which physical control
drives each action. The suggestions in `interaction_profiles.h` are only defaults; see
[PSVR2 through SteamVR](/hardware/psvr2-steamvr.md).

# Citations

[1] code/bridge/input.h, actions.h, psvr2_input.h
[2] code/vr/core/xr/locomotion.h:115 (stick-click D-pad), docs/VR_CONTROLS.md:16 (SteamVR menu chord)
