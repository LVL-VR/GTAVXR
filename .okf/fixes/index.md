# Fixes

* [PSVR2 input remap in the bridge](psvr2-input-remap.md) - Triangle-touch D-pad shift, the Create/Triangle+R3 menu as the SteamVR chord, binding diagnostics, and protection for other headsets.
* [DLSS chain fell back to a bilinear blit (d3d12_dll)](dlss-d3d12-load-fallback.md) - d3d12.dll crashed in DllMain on the patched exe's malformed export directory; repaired in memory by the bridge.
* [Suite "Access is denied" from elevated leftover processes](suite-elevated-leftover-processes.md) - GameRunning() threw on zero-thread leftovers of an admin-run game; fixed by a snapshot thread-count check.
* [Implicit OpenXR layers disabled for GTAVXR only](implicit-openxr-layers.md) - Cheeky/VD layers broke or complicated xrCreateInstance; disabled per launch via disable_layers.
