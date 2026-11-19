---
type: Runbook
title: CheekyFoveatedDLSS standalone hosted by the GTAVXR bridge
description: Load the Cheeky fork's standalone host from the bridge (no dxgi/version proxy) so it foveates GTAVXR's per-eye D3D12 DLSS SR; validated working.
tags: [cheeky, foveation, dlss, bridge, loader]
timestamp: '2026-11-18T05:40:00Z'
---

# Why not the proxy loader

The fork (satyaloka93/CheekyFoveatedDLSS, latest pre-releases v0.5.5-nr-bridge-preview.8 and
v0.5.5-loader-diagnostics.1, source in `CheekyFoveatedDLSS/build/nr-cleanup-release-20261101`, branch
`fix/cyberpunk-sbs-flashing-20261102` @ f60d266) loads through its own `dxgi.dll` or `version.dll` proxy. It does
not use ReShade, and it has an F11 overlay (`CheekyOverlay.ini` MenuKey=122).

For every session the GTAVXR Suite moves each top-level game-folder file that Windows loads by name (dxgi, version,
winmm, dinput8, d3d11/12, *.asi, …) into the session folder (`RetailPlugins.Incompatible`, `Compatible()` = false). A
proxy therefore never loads. **Subfolders are not touched.**

# Contract used (bootstrap/loader.cpp)

* `LoadLibraryExW(<folder>\CheekyFoveatedDLSSHost.dll, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | SYSTEM32)`, then
  `CheekyHost_Start(1)` (standalone), outside DllMain.
* The host loads `CheekyFoveatedDLSSRuntime.dll` from its own folder and uses that folder for its ini files and logs.
* It installs graphics hooks with late-attach Present discovery.

`code/bridge/cheeky_host.h` does exactly this once per process at bridge create. That is after the
[export-directory repair](/fixes/dlss-d3d12-load-fallback.md) and before the eye chain loads nvngx_dlss.dll.

* Opt-in: `<game exe folder>\CheekyFoveatedDLSS\CheekyFoveatedDLSSHost.dll` exists.
* `GTAVXR_CHEEKY_DIR` overrides the folder, and `GTAVXR_CHEEKY=0` disables it.
* Log line: `CHEEKY host_start=ok path=...`.

# Installed set (2026-11-18)

`E:\SteamLibrary\steamapps\common\Grand Theft Auto V\CheekyFoveatedDLSS\`, copied from the working Cyberpunk install:

* Host (2026-11-05) and Runtime (2026-11-07);
* CheekyOverlay.ini (F11);
* the Cyberpunk CheekyFoveatedDLSS.ini with **NrEnabled=0** for the first test (Enabled=1, D3D12LowerHook=1,
  CenterMode=0 fixed).

Remove the folder to disable it.

# Constraints

* Gaze: Cheeky's OpenXR gaze layer is disabled for GTAVXR ([implicit layers](/fixes/implicit-openxr-layers.md)), so
  only fixed foveation is available. Bridge-published gaze is the planned step 2.
* GTAVXR graphics should be **Performance (SR)**, with GTAVXR NR off, so Cheeky owns NR if it is wanted.
* F11 menu in the headset: see below. Without it, the overlay only draws on the game's desktop swap chain.
* Cheeky NR searches `nvngx_dlssnr.dll` beside its runtime, then beside the game exe (error 126 otherwise). The Suite's
  `dlss` folder is not searched, so the 310.9.16 copy now sits in `CheekyFoveatedDLSS\`. The DLL is selected at
  game start only.

# Validation (session 20261118T053623, 2026-11-18)

GTAVXR log:

```text
EXE_EXPORT_DIR_REPAIRED rva=0x277c0fc size=4
CHEEKY host_start=ok path=E:\...\Grand Theft Auto V\CheekyFoveatedDLSS\CheekyFoveatedDLSSHost.dll
EYE_CHAIN state=chain render=2000x2048 output=4008x4096 sr=1 nr=0
```

Cheeky standalone log:

* `DLL LOAD nvngx.dll_gtavxr_sr.dll`, then `nvngx_dlss.dll`. `NGX runtime stabilized ... scans=3/3; direct hooks allowed`.
* `D3D12 NGX evaluation route=Public nvngx_dlss.dll`, with two handles (one per eye).
* `PERIPHERAL METHOD method=DLSS-SR input=780x799 output=2000x2048` (per eye).
* Centre `crop=2958x3024@1062,1086` per eye view.
* **Verification counters:** `foveated_sr` non-zero and rising (688 at 14:38:53), `native_sr=0`,
  `peripheral_dlaa` 689, `pipeline_after` 689. There were no reject, fail, fallback or exception lines.
* GPU cost per evaluation: about 1.19 ms for the foveated SR, 0.65 ms for the periphery, 2.09 ms for the whole
  pipeline.

GTAVXR PERF145 pacing, average frame time over the paced windows:

| Session | Average |
| --- | --- |
| Blit fallback | 11.20 ms |
| GTAVXR SR+NR Balanced | 17.37 ms |
| GTAVXR SR Performance + Cheeky foveated (Cheeky NR off) | **10.73 ms** (about 90 Hz, 0.91 paced) |

This is confounded: NR off and Performance vs Balanced changed at the same time. A run with GTAVXR SR only and no
Cheeky (rename the folder) is still needed to isolate Cheeky's gain. Image quality has not been judged yet.

# F11 menu in the headset (bridge as Cheeky's menu consumer)

* Cheeky's overlay publishes a headset texture only when `GetModuleHandle("CheekyOpenXRLayer.dll")` succeeds and a
  consumer called `CheekyOpenXRMenuAcquireFrame` within the last 3 s (`standalone/menu_backend.hpp`). The OpenVR
  path needs the game to have initialised `openvr_api.dll`, which GTAVXR never does.
* `cheeky_host.h` loads `CheekyFoveatedDLSS\CheekyOpenXRLayer.dll` (copied from Program Files) as a **plain
  library**. Its DllMain only initialises in-process gaze slots, and the OpenXR loader still has the layer disabled.
  Log suffix: `menu_module=loaded`.
* `cheeky_menu_layer.inl` runs on every world frame:
  * calls AcquireFrame; the texture is D3D11 on the game device, the same as the bridge's, and anything else is
    reported as a device mismatch;
  * creates a quad swapchain in the same or a copy-compatible format;
  * `CheekyOpenXRMenuCopy11` copies on Cheeky's context under its lock;
  * submits a premultiplied quad 1.4 m wide, 1.3 m in front (yaw only), anchored when the menu opens;
  * the controller aim ray (right hand first) hits the quad, which goes to `CheekyOpenXRMenuSetPointer(x, y, down,
    active)` in Cheeky display pixels, with the trigger above 0.5 as click.
* While the menu shows, `cheeky_menu_input` zeroes all controller input to the game.
* Logs: `CHEEKY_MENU consumer=ready`, `CHEEKY_MENU swapchain WxH source_format= swap_format=`,
  `CHEEKY_MENU first headset frame submitted`.
* No change to the Cheeky fork was needed. Its worktree has uncommitted bootstrap work, which was left untouched.
* Pending: headset test of visibility, pointer accuracy and click.

# Gaze, FOV and stereo alignment (bridge as Cheeky's gaze provider)

**Symptom (2026-11-18 test):** two foveation circles, one per eye, even in fixed mode, and eye tracking "not
recognized". Cheeky NR worked once `nvngx_dlssnr.dll` sat beside the runtime.

**Cause:** Cheeky reads per-eye FOV, forward and gaze only from `CheekyOpenXR_GetGazeSnapshot`, exported by a module
named `CheekyOpenXRLayer.dll`, normally its OpenXR layer, which is disabled for GTAVXR. Without the per-eye FOV,
AutoStereoAlignment cannot place the centre.

PSVR2's views are strongly asymmetric. From the bridge's `EYE_FOV` log line:

* left eye: L -61.5°, R 43.4°, U/D ±53°;
* right eye: mirrored.

Straight ahead is therefore at **u = 0.660 in the left image and u = 0.340 in the right**, so circles at u = 0.5 land
on different directions.

Loading the real layer DLL as a plain library (the first headset-menu build) would have exposed an empty "layer
active" snapshot. It was withheld and never deployed.

**Fix:**

* `cheeky_gaze_shim.cpp` builds `CheekyOpenXRLayer.dll`, installed in `<game>\CheekyFoveatedDLSS\` in place of the
  real layer copy. It exports `CheekyOpenXR_GetGazeSnapshot` (returns 1), the three simulation setters (recorded only)
  and `GtavxrCheeky_PublishGazeSnapshot`.
* `cheeky_gaze_abi.h` gives a layout-compatible ABI v4. It was checked with offsetof against the fork's header:
  view 88 bytes, snapshot 368 bytes.
* `cheeky_host.h` loads the shim before the host and resolves the publisher. `instance_extensions()` adds
  `XR_EXT_eye_gaze_interaction` on both instance paths (eye_query SteamVR keep, session create), but only when the
  Cheeky host is installed and the runtime advertises it. SteamVR on this PC advertises v2.
* The session chains `XrSystemEyeGazeInteractionPropertiesEXT` only when the extension is enabled. `actions.h` then
  adds an `eye_gaze` pose action bound to `/interaction_profiles/ext/eye_gaze_interaction`
  `/user/eyes_ext/input/gaze_ext/pose`.
* `cheeky_gaze_publish.inl` publishes once per submitted frame, so Cheeky sees it at the next frame's DLSS evaluations:
  * flags: LayerActive, SessionFocused, ExtensionEnabled/SystemSupported, ActionActive, GazeValid, MappingReady;
  * per eye: rendered pose and FOV (radians), forward (normalised sum of eye orientations projected per eye, as
    Cheeky's stereo_forward_pose), gaze projected with Cheeky's project_gaze_to_view math (centre = forward when no
    gaze);
  * `resource_identity` = COM IUnknown of eye chain `out12`, the DLSS SR Output, with rect 0,0 and the output size.
    That satisfies Cheeky's `exact_view_match`.
* Logs: `XR_EYE_GAZE extension=1 system_supported=`, `CHEEKY host_start=ok shim=gtavxr gaze_ext=1`,
  `CHEEKY_GAZE first_snapshot mapping= ... forward=...`, `CHEEKY_GAZE first valid eye gaze`.
* Settings: CenterMode set to 1 (openxr_gaze). Backup: `CheekyFoveatedDLSS.ini.before-gaze`. AutoStereoAlignment=1.
* Without Cheeky installed, nothing changes: same extensions, same action set, no snapshot.

## Validation (session 20261118T060054)

Bridge log:

```text
CHEEKY host_start=ok shim=gtavxr gaze_ext=1
XR_EYE_GAZE extension=1 system_supported=1
EYE_CHAIN state=chain render=1696x1728 output=3400x3464 sr=1 nr=0
CHEEKY_GAZE first_snapshot mapping=1 eye_gaze_ext=1 bound=1 forward=0.6604,0.5 | 0.3396,0.5 out=3400x3464
CHEEKY_GAZE first valid eye gaze
CHEEKY_MENU swapchain 1920x1080 source_format=87 swap_format=91
CHEEKY_MENU first headset frame submitted
```

Cheeky log:

* `VR gaze mapping established ... eye=0 route=exact-resource` and `... eye=1 route=exact-resource`. The two earlier
  `Gaze mapping rejected ... views=0` lines come before the first publication.
* Normal reason 5 (jump) and reason 2 (reacquire) history resets.
* GPU stages: `foveated_sr` 793, `nr_after_foveated` 793, `native_sr=0`.

Crop positions (input space 3400x3464, crop 1614x1644):

| Eye | Example crop | Centre | x range | y range |
| --- | --- | --- | --- | --- |
| Left | @1443,914 | x 0.66 | 902..1780 | 689..1363 |
| Right | @352,914 | x 0.34 | 160..902 | 689..1363 |

The centres sit on each eye's forward point. Over 70 samples per eye the crop moved together in both eyes, with
identical y ranges, so it is driven by eye gaze and stereo-consistent.

Pending: the user's visual confirmation of one fused circle, the menu pointer and click, and image quality.
