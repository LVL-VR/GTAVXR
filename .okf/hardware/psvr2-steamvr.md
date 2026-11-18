---
type: Hardware Compatibility Profile
title: PSVR2 through SteamVR in GTAVXR
description: Measured runtime identity, controller profile, SteamVR binding, and audio routing for GTAVXR on PSVR2.
tags: [psvr2, steamvr, openxr, controllers, bindings, audio]
timestamp: '2026-11-18T03:30:00Z'
---

# Runtime identity (bridge log, session 20261118T025216)

```text
XR_SYSTEM name=SteamVR/OpenXR : playstation_vr2 psvr2=1
INITIALIZED width=3400 height=3464 format=91 runtime=SteamVR/OpenXR runtime_version=563022967865354
XR_INPUT_PROFILE left=/interaction_profiles/oculus/touch_controller right=/interaction_profiles/oculus/touch_controller
```

Detection is by `strstr(systemName,"playstation_vr2")` in `code/bridge/session.h`. The profile is `none` until
the first `XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED`, which is normal.

# Controller profile

SteamVR exposes Sense **only** as Oculus Touch, so the Touch suggestions in `code/bridge/interaction_profiles.h`
apply. The Cyberpunk work found that `sony/playstation_vr2` is not offered; see its
hardware profile (`F:\ai\GITHUB\cyberpunk-vr-port-upstream-psvr2\.okf\hardware\psvr2-steamvr.md`).

The automatic Touch→Sense remap drops `menu/click`, so Create must be assigned manually.

# Maintainer SteamVR binding

The app key is `system.generated.openxr.gtavxr native.gtavxr_sourcebaseline.exe`. The current binding is
`vr-input-workshop://3841741866`, cached under
`Steam\steamapps\workshop\content\250820\3841741866\*_legacy.bin`, with
`interaction_profile=/interaction_profiles/oculus/touch_controller` and `controller_type=playstation_vr2_sense`.

| Action | Bound source |
| --- | --- |
| stick / trigger / grip / stick_click | sticks, L2/R2, L1/R1, L3/R3 |
| primary / secondary | Square, Cross / Triangle click, Circle |
| menu | left **create** and right **create** (manual) |
| dpad_shift | left **triangle touch** (manual) |
| secondary_touch | unbound |
| hand_pose / aim_pose | handgrip / openxr_aim |
| native_rumble | both haptics |

A saved personal binding does **not** pick up actions added later: new actions show `[]` in `XR_INPUT_BOUND`
until they are mapped in the editor. The bridge logs the real binding at every profile change:

```text
XR_INPUT_BOUND menu=[Left Hand Create Button] ... dpad_shift=[]
```

# Audio

SteamVR settings `audio.enablePlaybackDeviceOverride_SIE_PS VR2=true` and override device
"PS VR2 (NVIDIA High Definition Audio)" are mirrored to "Speakers (Realtek(R) Audio)". GTA V binds its output
device at startup (`settings.xml` `OutputIndex=0`, the default device), so start SteamVR **before** Play. It is
not yet confirmed that this fixed the silent first session.

# Citations

[1] Bridge session logs: %LOCALAPPDATA%\GTAVXR\Suite\sessions\20261118T025216-e896b33bb063\native\activation-0002\xr.log
[2] Steam\config\steamvr.vrsettings (audio and per-app binding URLs)
