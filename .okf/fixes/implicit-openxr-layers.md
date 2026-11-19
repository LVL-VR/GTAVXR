---
type: Fix
title: Implicit OpenXR layers disabled for GTAVXR only
description: The Cheeky gaze layer broke GTAVXR's xrCreateInstance (-32) once Virtual Desktop's layer was off; both are now disabled per launch through the version record's disable_layers.
tags: [openxr, layers, cheeky, virtual-desktop, launch]
timestamp: '2026-11-18T05:00:00Z'
---

# Machine layers (HKLM\SOFTWARE\Khronos\OpenXR\1\ApiLayers\Implicit)

| Manifest | Disable variable |
| --- | --- |
| CheekyFoveatedDLSS\OpenXR\XR_APILAYER_CHEEKY_foveated_dlss.json (gaze bridge; injects XR_EXT_eye_gaze_interaction and a gaze action) | `CHEEKY_OPENXR_LAYER_DISABLE` |
| Virtual Desktop Streamer\openxr-oculus-compatibility.json | `DISABLE_XR_APILAYER_VIRTUALDESKTOP_OCULUS_COMPATIBILITY` |

# Observation

* **VD layer on:** GTA's module list contained openxr-oculus-compatibility.dll but **not** CheekyOpenXRLayer.dll,
  and VR started (`XR_INSTANCE result=0 reused=1`).
* **VD layer off** (registry value 1, set with `tools/vd-openxr-layer.ps1`): every activation failed with
  `XR_INSTANCE result=-32 reused=0` and `FAULT CreateInstance:-32` (XR_ERROR_FILE_ACCESS_ERROR).
* A standalone probe program through the same openxr_loader.dll succeeded with Cheeky loaded. So the
  failure needs the game process and was not reproduced outside it.

# Fix

The Suite sets each entry of `launch.disable_layers` to 1 in the game's environment only (`PlaySession.cs:513`). The
version record `Suite\versions\gtavxr-1-0-2.json` now lists:

```text
DISABLE_XR_APILAYER_reshade_1, CHEEKY_OPENXR_LAYER_DISABLE, DISABLE_XR_APILAYER_VIRTUALDESKTOP_OCULUS_COMPATIBILITY
```

The probe with those variables reports `CreateInstance succeeded with 0 layers enabled`. The global registry state
can go back to on for other apps. A backup of the record from before the change is in `Suite\psvr2-backup\`.
`deploy-psvr2-bridge.py --restore` brings back the official record and drops this change.

# Related

The [DLSS d3d12 load fallback](/fixes/dlss-d3d12-load-fallback.md) appeared while the VD layer was loaded. The first
session with neither layer will show whether one of them caused the d3d12.dll init failure.
