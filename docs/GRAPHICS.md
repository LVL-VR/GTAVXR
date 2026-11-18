# VR graphics

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

## GTA graphics settings

Keep **DirectX 11** and **Shader Quality: High** or **Very High** as the baseline. If outdoor shadows flicker between the eyes, switch **Soft Shadows** away from **NVIDIA PCSS**. If the game crashes while applying changes, record the setting and value. **MSAA** and **Post FX** can reload render resources. Apply major graphics changes outside VR and restart the game when GTA asks. [Startup and graphics troubleshooting](TROUBLESHOOTING.md)

## Set up DLSS files

1. Open **Settings → DLSS** and select the folder containing your compatible NVIDIA files. You can also open the Suite's **dlss** folder and place the files there.
2. For **Super Resolution**, the Suite looks for **nvngx_dlss.dll**. For **Neural Rendering**, it also looks for **nvngx_dlssnr.dll**.
3. Check that the Suite lists the files it found. Return to the profile, select the preset, and click **Play**.
4. In VR, open **VR Menu → Settings → Image and performance → Neural graphics (DLSS) → Status** to see what the game could activate with your GPU and driver.

If the Suite cannot use Super Resolution, it offers **Native**. If the neural file is missing, you can continue with **SR** when available. Choosing a fallback for the current session keeps your saved profile setting. If a mode does not initialize in-game, compare with **Native** first.

## OFXR Bridge

OFXR is an optional OpenXR layer that can generate frames. Your headset continues to use the OpenXR runtime selected for its connection. The Suite detects the layer and shows its status. Enable the layer in the OFXR tray app. [OFXR Bridge installation guide](https://github.com/tig3rmast3r/OFXR-Bridge#installation)

1. With GTA closed, follow the OFXR setup guide. Open **OFXRBridgeTray.exe** and enable the layer from the tray before clicking **Play**.
2. In the Suite, **Settings → OFXR (optional)** shows the detected tray app. If the Suite cannot find it, open the tray once or select **OFXRBridgeTray.exe** with the file picker when available.
3. Enter Story Mode and turn VR on with OFXR active so OFXR can create its GTAVXR profile. Once it exists, choose **Create recommended profile** under **VR settings → Graphics → OFXR**. The Suite turns off NR in that one OFXR profile, changing only that setting, and keeps an exact copy of the profile beside it. **Undo OFXR change** sets only that NR value back and keeps any other change you made to the profile. It preserves the file encoding and line endings. If you turn NR back on or an external editor changes the way the off value is written, use **Create recommended profile** again to make **Undo OFXR change** work with the original backup. If you choose Undo while NR is still changed, the Suite reports the conflict and keeps any other change and the backup. If the backup cannot be used to undo the recommendation, move the backup to another folder for safekeeping before trying the recommendation again. ANSI profiles with non-UTF-8 bytes are rejected as an unsupported format and left byte for byte as they were. If OFXR has the profile open, close the OFXR tray and try again. If more than one OFXR profile could belong to GTAVXR, the Suite changes none and says so. Control frame generation from the OFXR tray.
4. To see OFXR's state in the Suite during play, turn on **Export status.json** in the OFXR tray. Its location in the tray depends on the OFXR version. The Suite shows **generating frames in this game** only when OFXR's status is recent and names the running GTAVXR process. Otherwise it shows **active in this game, not generating frames** when OFXR reports that it is not generating, **active in this game; OFXR does not say whether it generates frames** when the status leaves it out, **paused**, **not enough data about this game**, or the reason the status does not apply. A value OFXR has not measured shows as **not measured**, never as 0. While the game runs, the Play screen shows the same status with the game and output FPS and refreshes it every two seconds. **Export status.json** and OFXR's **Diagnostic logging** are separate options.

When using OFXR frame generation, disable **SSW** in Virtual Desktop or the equivalent interpolation in your connection software. If OFXR uses **F10** for its menu, assign OFXR a different key so **F10** can toggle VR in GTAVXR. If you use GTAVXR's NR, keep OFXR's NR disabled for that profile. The Suite warns you if it detects that OFXR already applies NR. [OFXR Bridge usage guide](https://github.com/tig3rmast3r/OFXR-Bridge#usage)

The fix for OFXR failing to start on some 1.0.2 installations is in the game executable of 1.0.4 and later. After opening the new launcher, complete **Update GTAVXR** before trying OFXR again. In SteamVR, use a different **SteamVR menu key** if F9 conflicts with OFXR diagnostic logging. OFXR frame generation and GTAVXR super resolution are separate settings.

Available modes depend on the OFXR version, runtime, and GPU. To diagnose a problem, start with **Native** and OFXR off, then enable one option at a time. [Compatibility](COMPATIBILITY.md) · [Troubleshooting](TROUBLESHOOTING.md#mods-and-graphics)
