# Update Log

## 2026-11-18
* **Update**: Bridge publishes Cheeky's gaze/FOV snapshot through a GTAVXR shim; fixes the two-circle stereo misalignment and adds PSVR2 eye gaze — [Cheeky in GTAVXR](/operations/cheeky-standalone-in-gtavxr.md).
* **Update**: Cheeky NR DLL placement and the F11 menu shown in the headset via the bridge — [Cheeky in GTAVXR](/operations/cheeky-standalone-in-gtavxr.md).
* **Creation**: Cheeky standalone host loaded by the bridge — [Cheeky in GTAVXR](/operations/cheeky-standalone-in-gtavxr.md).
* **Update**: Root cause of the DLSS blit fallback: the patched exe's malformed export directory crashes d3d12 DllMain; repaired in memory by the bridge — [DLSS d3d12 load fallback](/fixes/dlss-d3d12-load-fallback.md).
* **Creation**: Cheeky/VD implicit layers disabled per GTAVXR launch after CreateInstance -32 — [Implicit OpenXR layers](/fixes/implicit-openxr-layers.md).
* **Creation**: DLSS SR/NR blit fallback at d3d12_dll and the System32 retry — [DLSS d3d12 load fallback](/fixes/dlss-d3d12-load-fallback.md).
* **Creation**: Bundle for the PSVR2/SteamVR fork of GTAVXR 1.0.2 — [Getting started](/getting-started.md).
* **Creation**: Measured PSVR2 runtime, the maintainer's SteamVR binding and audio override — [PSVR2 through SteamVR](/hardware/psvr2-steamvr.md).
* **Creation**: Bridge → GxInput → game flow and the SteamVR menu and D-pad chords — [Input pipeline](/architecture/input-pipeline.md).
* **Creation**: Triangle-touch D-pad, Create/Triangle+R3 menu chord, diagnostics, other-headset protection — [PSVR2 input remap](/fixes/psvr2-input-remap.md).
* **Creation**: Normal-Suite Play failure from elevated leftover processes and its fix — [Elevated leftover processes](/fixes/suite-elevated-leftover-processes.md).
* **Creation**: Bridge build, deploy script and hash record — [Bridge build and deploy](/operations/bridge-build-deploy.md).
* **Creation**: Killed install, INSTALL_REQUIRED and session recovery — [Install and session recovery](/operations/install-session-recovery.md).
