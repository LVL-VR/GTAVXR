---
type: Fix
title: DLSS chain fell back to a bilinear blit (d3d12_dll)
description: SR and NR never ran because d3d12.dll crashed in DllMain on the patched exe's malformed export directory; the bridge repairs the entry in memory.
tags: [dlss, neural-rendering, d3d12, bridge, blur]
timestamp: '2026-11-18T05:00:00Z'
---

# Symptom

With DLSS Balanced and NR 70%, the image was blurrier than native, and changing NR made no visible difference.
xr.log for session 20261118T042625 showed:

```text
EYE_CHAIN state=blit_fallback stage=d3d12_dll code=0x00000000
```

The game rendered each eye at the Balanced input size, and the bridge stretched it to 3400x3464 with a plain
bilinear blit. **No DLSS SR or NR ran.** Under the user-play preset, compact logging hides the chain timing lines
but still prints this one.

# Cause (confirmed 2026-11-18)

* The System32 retry still failed, now with `code=0x45A` (ERROR_DLL_INIT_FAILED). d3d12.dll loads fine in an
  ordinary process.
* Ruled out: TouchEn nxWeb (uninstalled), and the Cheeky and Virtual Desktop implicit layers (disabled per launch).
* A Process Monitor capture showed `Load Image C:\Windows\System32\D3D12.dll SUCCESS`, one ETW registration, and
  then the game's own `WerFault -u`.
* Application log: c0000005 at `ntdll+0x3d3e6`. With Microsoft's public PDB (dbghelp) that resolves to
  **`LdrpResolveProcedureAddress+0x1c6`**, which is GetProcAddress. The same bucket repeats at every game exit,
  which probably explains the zero-thread leftover processes.
* The patched `GTAVXR_SourceBaseline.exe` (SHA256 E721EBFF…, equal to the Suite's hard-coded `TargetHash`) has
  export data directory **RVA 0x277c0fc, size 4**. That points 4 bytes before its import directory. Read as
  IMAGE_EXPORT_DIRECTORY, it gives NumberOfNames 0x277cf08 and AddressOfNames 0. Retail GTA5.exe has 0/0.
* At DllMain, d3d12.dll looks up the Agility `D3D12SDKVersion` export on the process exe, so the name search faults.
  This is deterministic and a defect in the 1.0.2 exe.

# Fix

`code/bridge/exe_export_fix.h` runs at bridge create, before anything loads d3d12.dll. An exe export entry smaller
than `sizeof(IMAGE_EXPORT_DIRECTORY)` is cleared to 0/0 **in memory only** (VirtualProtect on the header page). The
exe file on disk keeps its hash (the Suite asserts it and restores it from its blob store), and valid export
directories are untouched. Log line: `EXE_EXPORT_DIR_REPAIRED rva=0x277c0fc size=4`.

The earlier System32 retry stays in place, and its log line now reports the real Win32 code.

# Files and setup

The Suite dlss folder (`%LOCALAPPDATA%\GTAVXR\Suite\dlss`) holds:

* nvngx_dlss.dll 310.9.16, from Cyberpunk bin\x64;
* nvngx_dlssnr.dll 310.9.16, the compat-fp16 copy, identical to the OFXR-bundled one.

The 310.8.0 NR copy in Downloads is the fallback.

# Validation

Confirmed in session 20261118T051246:

```text
EXE_EXPORT_DIR_REPAIRED rva=0x277c0fc size=4
EYE_CHAIN state=chain render=2320x2368 output=4008x4096 sr=1 nr=1
```

* The same `ntdll+0x3d3e6` APPCRASH still appears at 14:12:47/50, about one second after the exe starts. That is
  **before** the bridge loads, so other early code hits the same broken export directory. GTA's handler continues
  past it. The bridge-side repair cannot reach that time; fixing it needs either a Suite-side header fix (with its
  hash bookkeeping) or an upstream fix.
* The burst of LiveKernelEvent 141/193 and BlueScreen 139 reports at a single second (14:02:55, 14:12:48) is WER
  flushing its old queue. The System log shows no new display-driver events, and the last boot was 2026-11-16. See [bridge deploy](/operations/bridge-build-deploy.md).

# OFXR follow-up: repair at DLL load (2026-11-18)

* **Symptom with OFXR enabled:** everything blurry, and Cheeky inactive.
* **Evidence:**
  * stderr: `OpenXR-Loader: Failed loading layer information` / `xrCreateInstance failed`;
  * `EYE_SIZE_FALLBACK status=3 ... recommended=0x0 ui=1920x1080` and `EYE_VIEWS ... chosen=1920x1080`;
  * no `EYE_CHAIN` line, and Cheeky `mapping=0 out=0x0`.
* **Cause:** the host's eye-size query (a bridge export) runs **before** `create()`. OFXR's layer DLL imports
  `d3d12.dll`, which crashed on the still-unrepaired export directory, so the loader rejected the layer for that
  first instance. A standalone probe with the same layers, extensions and app name succeeded outside the game.
* **Fix:** the repair moved to a static initializer at bridge DLL load (kernel32 only). It is logged at the first
  create as `EXE_EXPORT_DIR_REPAIRED ... at=load`.
* **Verified (session 20261118T083737, OFXR v773 enabled):** `EYE_SIZE_QUERY_END status=0`, `reused=1`,
  `chosen=3400x3464`, `EYE_CHAIN state=chain`, Cheeky `mapping=1`. OFXR flight log: 3708 `synthesis_pair`,
  1876 `composition_publish`.
* The earlier OFXR session that "kicked out of VR" ended in an orderly way: game exit code 0, no WER entry, normal
  STOPPING→IDLE→EXITING. Its cause is not established.
