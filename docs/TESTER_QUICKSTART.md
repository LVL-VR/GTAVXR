# Tester quick start

Use the exact **GTAVXR.exe** supplied for the test. The coordinator records the build, runtime, headset, controller profile, GPU, graphics preset and any active OpenXR layer. For ordinary play, use the [installation guide](INSTALLATION.md).

## Prepare the session

1. For an update check, open the existing library and verify that the expected profiles and saves are available. Duplicate the profile before changing its controls or graphics for a test. [Suite and profiles](SUITE.md)
2. Pick the runtime for your headset connection under **Suite → Settings → Headset** and prepare a Story save at **Franklin and Lamar** or later. Agree on the start with the headset wearer before launching. [Compatibility](COMPATIBILITY.md) · [Profiles and saves](SAVES.md)
3. Prepare the required resolution, graphics preset and optional OFXR settings. The coordinator handles the logs and any keyboard or desktop-window steps. [Graphics](GRAPHICS.md)
4. Open the VR menu using the [shortcut for the active runtime](VR_CONTROLS.md#open-the-vr-menu-and-recenter). With SteamVR, center both sticks, click **L3 + R3** and release both. Outside SteamVR, use the left **Menu** button and use the centered stick-click gesture to recenter.

## During and after the test

- Start with the agreed test blocks. Check that the world, HUD and controls are visible and usable in the headset. Report **pass**, **fail**, or **not tested** for each block. The wearer can give the result aloud or after removing the headset.
- The [controls guide](VR_CONTROLS.md) covers default actions and bindings, including double R3 for third-person driving, the phone, free-hand weapon inputs and physical reloads. The [VR menu guide](VR_MENU.md) covers Stereo Theatre, HUD placement and the Trainer.
- Save in GTA, select **Force GTA to close** in the Suite and wait for the session to finish. Follow the [closing instructions](FIRST_SESSION.md#close-the-game-and-play-again) if the process remains.
- The coordinator correlates the observation with the exact session. Use the [report checklist](TROUBLESHOOTING.md#report-a-problem) and share logs privately when they contain local paths.
