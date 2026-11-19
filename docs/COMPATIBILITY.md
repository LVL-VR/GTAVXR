# GTAVXR compatibility

GTAVXR uses **GTA V Legacy 1.0.3889.0** as its base. The Suite checks the files in your selected installation before applying the patch.

## Store support

The table records verified setups from the 1.0 and 1.0.2 releases. Version 1.0.5 was checked on Steam Legacy with Virtual Desktop XR through a Story Mode session and normal shutdown. Headset, controller, runtime and graphics options affect the result on other setups.

| Store | Setup | What has been confirmed |
| --- | --- | --- |
| **Steam** | GTA V Legacy 1.0.3889.0 | Installation verified with the reference files. |
| **Rockstar Games Launcher** | GTA V Legacy, **Quest 3 and VDXR** | VR launch confirmed through the prologue. |
| **Epic Games Store** | GTA V Legacy, **Quest 3 and Virtual Desktop with VDXR** | VR gameplay confirmed, including missions and driving. |

[Installation and permission recovery](INSTALLATION.md) · [Known issues](KNOWN_ISSUES.md)

## Choose a runtime

For Quest over Virtual Desktop, start with **VDXR**. SteamVR is supported, including **Steam Link**, and can have a noticeable frame-rate cost compared with VDXR. The difference depends on resolution, GPU, encoding and runtime settings. With a wired headset or a vendor PCVR connection, choose the headset's compatible **OpenXR runtime**. VDXR and SteamVR are both implementations of OpenXR. [Graphics and performance](GRAPHICS.md)

## Components and runtimes

| Component | What the Suite supports or checks | What matters in-game |
| --- | --- | --- |
| **GTA V Legacy folder** | Detects Steam libraries and Rockstar-registered paths. You can also select a folder manually. Installation checks the base executable and required resources. | Installation, launch, and shutdown with that setup. See the store results above. |
| **OpenXR** | **Automatic** follows the active Windows runtime. You can also select a detected runtime or an installed manifest. | The XR session opens, both eyes display correctly, and tracking, controllers, and reconnection work. |
| **Headset connection** | Virtual Desktop can use VDXR or SteamVR, depending on its configuration. Steam Link uses SteamVR. Meta Link and Air Link use Meta's PC software. Select the runtime for your connection. | VDXR is confirmed in the setups above. SteamVR via Steam Link is confirmed on Quest 3 in Story Mode. Other headset and controller combinations vary. |
| **Mods and Script Host** | Inspects the mod package, requirements, and scripting APIs. | The mod loads and its actions or content work in Story Mode. |
| **DLSS, Neural Rendering, and OFXR** | Detects model files or the appropriate layer and offers fallbacks when a component is missing. | The selected mode initializes and runs with your GPU and runtime. |

## Controller profiles

GTAVXR suggests OpenXR bindings for Touch, Valve Index, Vive wands and the Khronos Simple Controller. The runtime decides which profile and inputs are active. Touch is the reference layout for the controls guide. Index maps the left-hand face buttons to X/Y and the right-hand face buttons to A/B. Vive wands use their trackpads for stick movement and the default wand mapping provides only the left stick click. The simple profile offers basic trigger and menu inputs.

With SteamVR, use both stick clicks for the GTAVXR menu when the active controller profile provides them. Otherwise, use the configured keyboard shortcut, **F9** by default. Outside SteamVR, the two-click recenter also requires both stick clicks. Use **Recenter** in the VR menu when one is missing. A suggested profile does not indicate that every controller, headset or gesture has been tested. [Runtime shortcuts](VR_CONTROLS.md#open-the-vr-menu-and-recenter)

If your setup has a problem, record the store, version, GPU, headset, connection, and runtime you used. [Troubleshooting](TROUBLESHOOTING.md)

For help connecting your headset, see the vendor documentation. [Steam Link](https://help.steampowered.com/en/faqs/view/0E2C-406B-9135-38A4) · [Meta Horizon Link OpenXR runtime](https://developers.meta.com/vr/documentation/unity/unity-link/#activate-openxr-runtime)
