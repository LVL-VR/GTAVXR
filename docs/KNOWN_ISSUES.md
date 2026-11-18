# Known issues

GTAVXR focuses on open-world exploration. If a Story Mode sequence gives you trouble in VR, follow the steps below.

## Microsoft Defender reports

Some users report that Microsoft Defender flags GTAVXR.exe. Scan results apply to a particular version and file hash. Use the SHA-256 from the release you downloaded.

The installer unpacks the bundled VR components and uses your existing GTA V Legacy files to build the VR executable locally. Antivirus tools may flag installers that unpack executables.

Check your download's SHA-256 against the value in the release notes for the version you downloaded. You can also scan the installer with your preferred antivirus or [VirusTotal](https://www.virustotal.com/).

## Story Mode, camera, and cutscenes

| Issue | What to do |
| --- | --- |
| **The camera is out of place after a mission.** You may appear below the map, see only the ocean, or face an unexpected direction. | Face forward in your usual playing posture and select **VR Menu → Recenter**. Do the same if your position or body pose is out of alignment. [How to recenter](TROUBLESHOOTING.md#recenter-the-view) |
| **Cutscenes can be disorienting.** Some perspective changes are abrupt in Current VR Camera. | Use **Stereo Theatre**, the default for supported cutscenes. For other sequences, skip the cutscene when the game allows. Recenter after returning to gameplay if the view is out of place. [Cutscene presentation](VR_MENU.md#watch-cutscenes-in-stereo-theatre) |
| **The first switch from Michael to Trevor can fail in North Yankton.** The character wheel may close when you try to select Trevor with motion controllers. | When the guard grabs Michael, press **F10**. Hold **Left Alt**, move the mouse toward Trevor, then release **Alt**. You can also switch with a standard gamepad. Press **F10** again to return to VR. [Prologue walkthrough](FIRST_SESSION.md#starting-a-new-game) |
| **A mission step does not advance with motion controllers.** | Press **F10**, finish that step in flatscreen with a keyboard and mouse or a standard gamepad, then press **F10** again to return to VR. |

For your first session, load a save at **Franklin and Lamar**. If you start a new game, you can complete the entire prologue with VR off. [First session](FIRST_SESSION.md)

## Driving

If the steering wheel stops following your hands, **release the grip button and grab the wheel again**. Use **VR Menu → Settings → Vehicles** to review driving and camera settings by vehicle type. [Vehicle controls](VR_CONTROLS.md#vehicles-flight-and-parachutes)

## Installation and shutdown

- **Access is denied during installation.** Close GTA and the Suite. Right-click **GTAVXR.exe** and select **Run as administrator**. If a recovery notice appears, select **Fix now** and wait for it to finish. [Recover an interrupted installation](TROUBLESHOOTING.md#access-denied-during-installation)
- **GTA may remain running when you exit from the pause menu.** Save your progress and use **Force GTA to close** in the Suite. If the process remains active, follow the closing instructions. [Close the game and play again](FIRST_SESSION.md#close-the-game-and-play-again)

If the game crashes or you cannot relaunch after shutdown, see [Crashes and blocked relaunches](TROUBLESHOOTING.md#crashes-and-blocked-relaunches).

## OFXR, DLSS, and HUD

- If OFXR did not start in 1.0.2, update GTAVXR from within the new Suite so the game executable is updated too. On the current version, a detected layer or armed status only confirms setup. Check whether OFXR is generating frames while you are in VR.
- DLSS and Neural Rendering can fall back when the required files, inputs or GPU support are unavailable. Report the preset and active path if the quality changes after pause, scope or a cutscene.
- A HUD panel can be temporarily absent if the runtime refuses its image allocation. If an element stays missing, record which element, whether it appears on the desktop mirror, and what transition preceded it.
- If **F10** also opens OFXR, assign a different key to OFXR in its tray app. **F10** switches between VR and flatscreen in GTAVXR.
- If you enable OFXR frame generation, disable **SSW** in Virtual Desktop to avoid running two interpolation modes at once. [Set up OFXR](GRAPHICS.md#ofxr-bridge)

Use a different SteamVR menu key in **Suite → Settings → SteamVR menu key** if your OFXR release uses **F9** for logging, or enable OFXR diagnostic logging in its tray before play. [Graphics and OFXR](GRAPHICS.md#ofxr-bridge)

## SteamVR

- SteamVR headset and controller support depends on the active OpenXR runtime and controller profile. If GTA remains in flatscreen after you can play in Story Mode, try **F10** with the GTA window focused and keep the session log if VR still fails to start. [Headset troubleshooting](TROUBLESHOOTING.md#headset-and-controllers)
- If a Steam Link session stays in the default SteamVR environment with the GTAVXR HUD visible while the world is visible only in GTA's desktop window, finish updating with **Install GTAVXR** or **Update GTAVXR**. If the default SteamVR environment still covers the game, keep the session log and the SteamVR client log for GTAVXR. [Headset troubleshooting](TROUBLESHOOTING.md#headset-and-controllers)
- If your controllers do not expose both stick clicks, use the configured SteamVR menu key, **F9** by default. [VR controls](VR_CONTROLS.md)

If a problem persists, include the mission or action, headset model, and runtime in your report. [What to include in a report](TROUBLESHOOTING.md#report-a-problem)
