# GTAVXR 1.0 compatibility

GTAVXR 1.0 uses **GTA V Legacy 1.0.3889.0** as its base. The Suite checks the files in your selected installation before applying the patch.

## Store support

| Store | Setup | What has been confirmed |
| --- | --- | --- |
| **Steam** | GTA V Legacy 1.0.3889.0, **Virtual Desktop with VDXR** | GTAVXR 1.0 installation verified. VR gameplay and closing through the Suite verified on a prerelease build. |
| **Rockstar Games Launcher** | GTA V Legacy, **Quest 3 and VDXR** | GTAVXR 1.0 launched in VR in the North Yankton prologue. Running the Suite as administrator resolved the installation access error. |
| **Epic Games Store** | GTA V Legacy, **Quest 3 and Virtual Desktop with VDXR** | GTAVXR 1.0 gameplay confirmed in VR, including missions and driving. |

[Installation and permission recovery](INSTALLATION.md) · [Known issues](KNOWN_ISSUES.md)

## Components and runtimes

| Component | What the Suite supports or checks | What matters in-game |
| --- | --- | --- |
| **GTA V Legacy folder** | Detects Steam libraries and Rockstar-registered paths. You can also select a folder manually. Installation checks the base executable and required resources. | Installation, launch, and shutdown with that setup. See the store results above. |
| **OpenXR** | **Automatic** follows the active Windows runtime. You can also select a detected runtime or an installed manifest. | The XR session opens, both eyes display correctly, and tracking, controllers, and reconnection work. |
| **Headset connection** | Virtual Desktop can use VDXR or SteamVR, depending on its configuration. Steam Link uses SteamVR. Meta Link and Air Link use Meta's PC software. Select the runtime for your connection. | VDXR is confirmed in the setups above. SteamVR, Steam Link, and Meta Link or Air Link still need in-game confirmation. |
| **Mods and Script Host** | Inspects the mod package, requirements, and scripting APIs. | The mod loads and its actions or content work in Story Mode. |
| **DLSS, Neural Rendering, and OFXR** | Detects model files or the appropriate layer and offers fallbacks when a component is missing. | The selected mode initializes and runs with your GPU and runtime. |

If your setup has a problem, record the store, version, GPU, headset, connection, and runtime you used. [Troubleshooting](TROUBLESHOOTING.md)

For help connecting your headset, see the vendor documentation. [Steam Link](https://help.steampowered.com/en/faqs/view/0E2C-406B-9135-38A4) · [Meta Horizon Link OpenXR runtime](https://developers.meta.com/vr/documentation/unity/unity-link/#activate-openxr-runtime)
