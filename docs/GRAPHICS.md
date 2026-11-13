# VR graphics

> [!IMPORTANT]
> DLSS Super Resolution and Neural Rendering require you to supply your own NVIDIA files compatible with your GPU. Choose **Native** to play without those files.

Open your profile under **Play → VR settings → Graphics**. Start with **Native** as a baseline, then compare each change in the same Story Mode location.

## Presets and options

| Preset | What it does |
| --- | --- |
| **Native** | Renders each eye at the view's resolution without DLSS Super Resolution or Neural Rendering. |
| **Performance (SR)** | Enables DLSS Super Resolution with Neural Rendering off. The preset keeps your selected SR level or starts at **Quality**. |
| **Neural (SR+NR)** | Combines Super Resolution with the neural rendering pass. It requires both NVIDIA files. |
| **Custom** | Lets you select the Super Resolution level and Neural Rendering separately. |

**Super Resolution** offers Off, Quality, Balanced, Performance, and Ultra Performance. The more aggressive presets start from a lower render resolution. Compare sharpness, stability, and smoothness on your PC. The SR level applies when GTA starts.

**Neural Rendering (NR)** adds a pass per eye. **Neural intensity** controls the blend from 0 to 100. In-game, use **VR Menu → Settings → Image and performance → Neural graphics (DLSS)** to change NR and its strength live, compare A/B, or check the active mode. Changes are saved to the profile. [VR menu](VR_MENU.md)

## Set up DLSS files

> [!IMPORTANT]
> Before enabling DLSS, obtain NVIDIA files compatible with your GPU. Supply **nvngx_dlss.dll** for Super Resolution and add **nvngx_dlssnr.dll** to use Neural Rendering.

1. Open **Settings → DLSS** and select the folder containing your compatible NVIDIA files. You can also open the Suite's **dlss** folder and place the files there.
2. For **Super Resolution**, the Suite looks for **nvngx_dlss.dll**. For **Neural Rendering**, it also looks for **nvngx_dlssnr.dll**.
3. Check that the Suite lists the files it found. Return to the profile, select the preset, and click **Play**.
4. In VR, open **VR Menu → Settings → Image and performance → Neural graphics (DLSS) → Status** to see what the game could activate with your GPU and driver.

If the Suite cannot use Super Resolution, it offers **Native**. If the neural file is missing, you can continue with **SR** when available. Choosing a fallback for the current session keeps your saved profile setting. If a mode does not initialize in-game, compare with **Native** first.

## OFXR Bridge

OFXR is an optional OpenXR layer that can generate frames. Your headset continues to use the OpenXR runtime selected for its connection. The Suite detects the layer and shows its status. Enable the layer in the OFXR tray app. [OFXR Bridge installation guide](https://github.com/tig3rmast3r/OFXR-Bridge#installation)

1. With GTA closed, follow the OFXR setup guide. Open **OFXRBridgeTray.exe** and enable the layer from the tray before clicking **Play**.
2. In the Suite, **Settings → OFXR (optional)** shows the detected tray app. If the Suite cannot find it, open the tray once or select **OFXRBridgeTray.exe** with the file picker when available.
3. Enter Story Mode and turn VR on with OFXR active so OFXR can create its GTAVXR profile. Once it exists, choose **Create recommended profile** under **VR settings → Graphics → OFXR**. The Suite keeps a backup and disables NR in that OFXR profile if its format and settings are compatible. Control frame generation from the OFXR tray.
4. Check for **generating frames** during play. **Armed**, **paused**, **loaded in GTAVXR**, and **no GTAVXR data** describe different states. The Suite may show no data outside an active session.

When using OFXR frame generation, disable **SSW** in Virtual Desktop or the equivalent interpolation in your connection software. If OFXR uses **F10** for its menu, assign OFXR a different key so **F10** can toggle VR in GTAVXR. If you use GTAVXR's NR, keep OFXR's NR disabled for that profile. The Suite warns you if it detects that OFXR already applies NR. [OFXR Bridge usage guide](https://github.com/tig3rmast3r/OFXR-Bridge#usage)

Available modes depend on the OFXR version, runtime, and GPU. To diagnose a problem, start with **Native** and OFXR off, then enable one option at a time. [Compatibility](COMPATIBILITY.md) · [Troubleshooting](TROUBLESHOOTING.md#mods-and-graphics)
