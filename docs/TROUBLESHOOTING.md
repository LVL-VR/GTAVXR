# Troubleshooting

For problems during missions, cutscenes, or driving, see [Known issues](KNOWN_ISSUES.md).

## Installation

| What you see | What to do |
| --- | --- |
| The Suite cannot find GTA | Select the Legacy folder containing **GTA5.exe**. It can be on another drive or outside Steam. Use **Settings → Game** to check the current folder. |
| **Access is denied** in the game folder | Close GTA and the Suite. Reopen **GTAVXR.exe** with **Run as administrator**, then follow the recovery steps. [Access denied during installation](#access-denied-during-installation) |
| **NOT_LEGACY_3889** | Check that you selected GTA V Legacy and the version required by this release. GTA V Enhanced is a separate edition. |
| **DELTA_BASE_MISMATCH** | The patch expects a different base file. Save the error and note your store and **GTA5.exe** version. If asked, include the file's SHA-256 so the base can be identified. |
| **RETAIL_RESOURCE_MISMATCH** or **RETAIL_SHADER_BASE_MISMATCH** | Note the path shown in the message. Check whether a mod or update changed the corresponding Legacy files. Use your launcher to verify and repair the game files if needed. |
| GTAVXR files are missing or damaged after installation | Close GTA and use **Settings → Check files**. If the error points to base Legacy files, follow the installer message. |

[Installation and maintenance](INSTALLATION.md) · [Store and headset compatibility](COMPATIBILITY.md)

### Access denied during installation

1. Close GTA if it is open, then close the Suite.
2. Right-click **GTAVXR.exe** and select **Run as administrator**.
3. If you see the recovery notice, select **Fix now** and wait for it to clear. The notice can also appear after an interrupted installation.
4. Select **Install GTAVXR** if the button is still available. If Windows denies access again, capture the error with its full path and leave the game folder as it is.

## Headset and controllers

- If Story Mode is in flatscreen, focus the GTA window and press **F10** on its own.
- If you see an OpenXR error, check the headset connection and **Settings → Headset**. Reselect the runtime if you moved or reinstalled its software. **Automatic** follows the active Windows runtime.
- If the headset shows an image and the controllers do not respond, check that both controllers are active and the GTA window has focus. Record the headset, controllers, and runtime you used.
- If the weapon wheel does not open, close the VR menu, release the grip, and press it again with your hand over the shoulder on that side. Keep the grip held while selecting with that hand's thumbstick. [Weapon wheel and holsters](VR_CONTROLS.md#weapon-wheel-and-holsters)
- If you cannot grab an object, free the hand, move it close to the object, then press the grip again. [Physical interaction](VR_CONTROLS.md#physical-interaction)

### Recenter the view

Recentering usually resolves camera, position, and body pose alignment problems. Try it first if the view faces the wrong way, appears displaced, or does not match your posture.

1. Face forward in your usual standing or seated playing posture.
2. Press **Menu** on the left controller and select **Recenter** on the main screen. You can also use **Quick actions → Recenter view**. Close the menu and check your position.
3. If only the height is off, use **Settings → Calibration → Height** in the VR menu to adjust it for the current context. If the problem persists, report whether you were on foot or in a vehicle and what happened when you recentered. [VR controls](VR_CONTROLS.md)

## Mods and graphics

- If a mod is marked incompatible, open its entry and read the reason. Check that you have the Legacy edition, required add-ons, and no conflicts. Launch the same profile with that mod disabled to isolate the issue. [Mods](MODS.md)
- If DLSS or Neural Rendering does not activate, review the files under **Settings → DLSS**, the fallback the Suite offers, and the active mode shown in the VR menu. Try **Native** to check the base experience. [Graphics](GRAPHICS.md)
- If OFXR shows no data, enable its layer before starting GTA and check its status while VR is active. Use one frame generation or interpolation mode at a time. If **F10** opens both menus, change the OFXR key binding. [OFXR Bridge](GRAPHICS.md#ofxr-bridge)

## Closing and recovery

Save in GTA and select **Force GTA to close** in the Suite. Wait for the session to finish closing before you play again. If a GTA process remains and the Suite cannot stop it, follow the [closing instructions](FIRST_SESSION.md#close-the-game-and-play-again).

## Report a problem

Include the GTAVXR version, store and Legacy build, GPU and driver, headset and connection, OpenXR runtime, graphics preset, DLSS or NR, OFXR, and active mods. Describe the mission or action, the steps to reproduce the problem, what happened, and whether it also happens with a base profile. Attach the exact error or a screenshot if you can.
