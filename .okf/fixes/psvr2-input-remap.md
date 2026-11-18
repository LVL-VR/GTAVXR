---
type: Fix
title: PSVR2 input remap in the bridge
description: Triangle-touch D-pad shift, the Create/Triangle+R3 menu as the SteamVR chord, binding diagnostics, and protection for other headsets.
tags: [psvr2, input, dpad, menu, bridge, tests]
timestamp: '2026-11-18T03:30:00Z'
---

# Behaviour (only when systemName contains playstation_vr2)

All of it lives in `code/bridge/psvr2_input.h` (`gx_psvr2::Remap::apply`). It is a pure function called once
per read from `actions.h`.

| Input | Output |
| --- | --- |
| Modifier = left `secondary_touch` (y/touch) **or** `dpad_shift` ("D-pad Shift Thumbrest Touch", thumbrest/touch suggestion) | — |
| Modifier + right stick ≤ 0.5 | Right stick zeroed (no turning) |
| Modifier + right stick > 0.5 | L3+R3 held, stick passed through, **no** BOTH_CLICKS flag, latched until the modifier lifts → game D-pad |
| Modifier + R3 edge (not shifted) | Menu |
| `menu` action (Create, either hand) or the above | L3+R3 + BOTH_CLICKS with all sticks zeroed while held → SteamVR VR-menu chord |
| Menu during an active shift | Dropped |
| R3 already held when the modifier starts | Stays R3 |

Why: on SteamVR the game ignores GX_MENU (see [input pipeline](/architecture/input-pipeline.md)), and there
are no D-pad bits in the ABI. The UEVR/Cyberpunk Triangle-touch convention comes from the
Cyberpunk PSVR2 fix (`F:\ai\GITHUB\cyberpunk-vr-port\.okf\fixes\psvr2-triangle-dpad.md`).

# Other headsets

* With `psvr2=false` the remap only derives BOTH_CLICKS exactly as upstream did. A unit test checks every button
  and stick combination for passthrough.
* The new touch actions are read only on PSVR2. They are suggested only on the Oculus profile (left `y/touch`,
  left `thumbrest/touch`).
* If a runtime rejects the profile with them, the bridge retries the suggestion without them, so the original
  bindings are kept.
* `menu` is read on both hands. Nothing suggests right-hand menu by default, so it stays inert unless the user
  binds it.

# Diagnostics (xr.log)

```text
XR_INPUT_BOUND menu=[...] primary=[...] secondary=[...] stick_click=[...] secondary_touch=[...] dpad_shift=[...]
XR_INPUT first_press buttons=0x40 (A1 B2 X4 Y8 L3=10 R3=20 MENU=40)
XR_PSVR2 modifier_touch first_seen
XR_PSVR2 dpad_shift confirmed
XR_PSVR2 triangle_touch_r3_menu confirmed
```

# Validation

* `g++ -std=c++17 -I code/bridge code/bridge/tests/psvr2_input_test.cpp` → all passed.
* MSVC Release build is clean; the only warnings are upstream ones in native_ui_layer.inl.
* Headset, 2026-11-18:
  * The D-pad shift works (Up = phone) and every face button and stick click reached the game.
  * Create reached the game as 0x40 but did not open the menu. That led to the chord replay, which is not yet
    headset-tested.
* Open question: is the phone D-pad up, or a stray R3 tap when the synthesized chord releases?

# Citations

[1] Commits 6a021e4, a163daf, c103e7b on branch psvr2-steamvr
