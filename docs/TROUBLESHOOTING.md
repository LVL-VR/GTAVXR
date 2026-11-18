# Troubleshooting

For problems during missions, cutscenes, or driving, see [Known issues](KNOWN_ISSUES.md).


## Installation

If Microsoft Defender flags **GTAVXR.exe**, see [Microsoft Defender reports](KNOWN_ISSUES.md#microsoft-defender-reports).

| What you see | What to do |
| --- | --- |
| The Suite cannot find GTA | Select the Legacy folder containing **GTA5.exe**. It can be on another drive or outside Steam. Use **Settings → Game** to check the current folder. |
| **Access is denied** in the game folder | Close GTA and the Suite. Reopen **GTAVXR.exe** with **Run as administrator**, then follow the recovery steps. [Access denied during installation](#access-denied-during-installation) |
| **NOT_LEGACY_3889** | Check that you selected GTA V Legacy and the version required by this release. GTA V Enhanced is a separate edition. |
| **DELTA_BASE_MISMATCH** | The patch expects a different base file. Save the error and note your store and **GTA5.exe** version. If asked, include the file's SHA-256 so the base can be identified. |
| **RETAIL_RESOURCE_MISMATCH** or **RETAIL_SHADER_BASE_MISMATCH** | Note the path shown in the message. Check whether a mod or update changed the corresponding Legacy files. Use your launcher to verify and repair the game files if needed. |
| GTAVXR files are missing or damaged after installation | Close GTA and use **Settings → Check files**. If the error points to base Legacy files, follow the installer message. |
| **There is not enough space on the disk** | Check the free space on both the GTA V Legacy drive and the drive shown under **Settings → Suite data**. Active mods can add temporary and installed files. |

[Installation and maintenance](INSTALLATION.md) · [Store and headset compatibility](COMPATIBILITY.md)

### Access denied during installation

1. Close GTA if it is open, then close the Suite.
2. Right-click **GTAVXR.exe** and select **Run as administrator**.
3. If you see the recovery notice, select **Fix now** and wait for it to clear. The notice can also appear after an interrupted installation.
4. Select **Install GTAVXR** if the button is still available. If Windows denies access again, capture the error with its full path and leave the game folder as it is.

### Windows drive fills when you select Play

The Suite stores working files, profiles, and session recovery data under **%LOCALAPPDATA%\GTAVXR\Suite** by default, even when GTA V Legacy is installed on another drive. If the Windows drive runs out of space and stays full after a failed launch, stop retrying Play.

Open **Settings → Suite data → Open folder**. For the default location, you can also paste **%LOCALAPPDATA%\GTAVXR\Suite** into File Explorer. Use **Properties** to check the total size and identify the largest folder or file. Keep your profiles and session files for recovery. Share the name and size of the largest item privately, along with the free space on the Windows drive and the GTA drive. If the Suite folder is small or missing, report that too. If you already uninstalled GTA V Legacy and want the space back, see [Remove GTAVXR completely](INSTALLATION.md#remove-gtavxr-completely).

## Crashes and blocked relaunches

**Crash around an interior.** Version 1.0.5 fixes a crash when adjacent interiors share a portal and one of them unloads. If a crash still occurs, note the location, whether you were entering or leaving an interior, and which mods were enabled. Keep the session logs and the local minidump if one was written. [What to include in a report](#report-a-problem)

**Access is denied when you press Play after quitting GTA.** Update to 1.0.5. The Suite now ignores confirmed exited processes that Windows still lists. If it says GTA is running, wait for the session to finish closing before you retry. File permission errors that name a path still follow the [installation recovery steps](#access-denied-during-installation).

## Headset and controllers

- Wait until you can control your character in Story Mode. If the headset still shows no game image, focus the GTA window and press **F10** once. If it stays in flatscreen, record the runtime selected in the Suite and any OpenXR error in the session log.
- On the **Play** screen, the green SteamVR indicator means the Suite detected the runtime and found the SteamVR server process running. The game still has to open an OpenXR session and send frames to the headset.
- If you connect through **SteamVR**, select **SteamVR** under **Suite → Settings → Headset**. If Story Mode is playable on the monitor and the headset stays in the default SteamVR environment or shows only the GTAVXR HUD, share the existing session's OpenXR log privately. Note whether you could see the world in either eye and whether the HUD was still visible. Include the headset, controllers, and connection method, such as Steam Link or a wired connection. [Find the session log](#startup-and-mission-errors)
- If you used **Run as administrator** for installation, try reopening the Suite normally before selecting a runtime for Play. In an elevated launch, the OpenXR loader uses the active Windows runtime. If you need to play elevated, set the intended runtime as active in its own app before launching.
- If you already use **Virtual Desktop** and its SteamVR mode stays in flatscreen, you can try **VDXR** under **OpenXR Runtime** in Virtual Desktop Streamer. Select the corresponding runtime under **Suite → Settings → Headset** and relaunch through the Suite.
- If you see an OpenXR error, check the headset connection and **Settings → Headset**. Reselect the runtime if you moved or reinstalled its software. **Automatic** follows the active Windows runtime.
- If the headset shows an image and the controllers do not respond, check that both controllers are active and the GTA window has focus. Record the headset, controllers, and runtime you used.
- If the weapon wheel does not open, close the VR menu, release the grip, and press it again with your hand over the shoulder on that side. Keep the grip held while selecting with that hand's thumbstick. [Weapon wheel and holsters](VR_CONTROLS.md#weapon-wheel-and-holsters)
- If you cannot grab an object, free the hand, move it close to the object, then press the grip again. [Physical interaction](VR_CONTROLS.md#physical-interaction)

### Recenter the view

Recentering usually resolves camera, position, and body pose alignment problems. Try it first if the view faces the wrong way, appears displaced, or does not match your posture.

1. Face forward in your usual standing or seated playing posture.
2. Open the VR menu with the [shortcut for your runtime](VR_CONTROLS.md#open-the-vr-menu-and-recenter) and select **Recenter** on the main screen. Outside SteamVR, press and release **L3 + R3** with the sticks centered to recenter directly. You can also use **Quick actions → Recenter view**. Close the menu and check your position.
3. If only the height is off, use **Settings → Calibration → Height** in the VR menu to adjust it for the current context. If the problem persists, report whether you were on foot or in a vehicle and what happened when you recentered. [VR controls](VR_CONTROLS.md)

## Mods and graphics

- If a mod is marked incompatible, open its entry and read the reason. Check that you have the Legacy edition, required add-ons, and no conflicts. Launch the same profile with that mod disabled to isolate the issue. [Mods](MODS.md)
- If DLSS or Neural Rendering does not activate, review **Settings → DLSS** and the active mode shown in the VR menu. Try **Native** to check the base experience. If the image changes when returning from pause, the scope or Stereo Theatre, report which transition triggered it. Keep the existing session OpenXR log so both eyes and any fallback can be checked. [Graphics](GRAPHICS.md)
- If OFXR shows no data, enable its layer and **Export status.json** before starting GTA, then check its status while VR is active. The Suite shows the reason when a status does not belong to the running game. Use one frame generation or interpolation mode at a time. If **F10** opens OFXR too, change its menu key. In SteamVR, if **F9** also toggles OFXR diagnostic logging, configure a different GTAVXR menu key or turn OFXR logging on from its tray before launching. [OFXR Bridge](GRAPHICS.md#ofxr-bridge)

- If a sound pack imports but has no effect in game, check that it is enabled in the active profile and report the exact pack. [Sound mods](MODS.md#sound-mods)

## Closing and recovery

Save in GTA and select **Force GTA to close** in the Suite. Wait for the session to finish closing before you play again. If a GTA process remains and the Suite cannot stop it, follow the [closing instructions](FIRST_SESSION.md#close-the-game-and-play-again).

If **Fix now** fails, close GTA and the Suite and keep the active Suite data folder in place. It holds your profiles, recovery records, and backup of the normal GTA user files for the interrupted session. Share the error and the affected session's `session.json` privately so recovery can be checked before a clean install. [Uninstall GTAVXR](INSTALLATION.md#repair-and-recover)

## Startup and mission errors

If the game closes or shows a fatal error, record whether it happened during startup or while playing. Include the exact error text and any resource path shown. For a mission failure, include the mission, location or objective, and the mods active in that profile. Share logs or crash dumps privately if they contain personal paths or other sensitive data.

| What you see | What to include |
| --- | --- |
| The game runs on the monitor after Story Mode loads, but the headset stays blank | The headset, PC connection, selected OpenXR runtime, and the first OpenXR error in the session log, if present. |
| The game crashes before Story Mode loads | The exact error and any crash dump or launch log already available. Include the store, GTA Legacy build, and active mods. If the log mentions gameconfig.xml or an RPF archive, say whether OpenIV or another mod previously modified the original game files and whether the store launcher has already verified them. |
| Roads or scenery are missing while driving | The mission or objective, location, vehicle, camera view, and active mods. Mention whether the camera was displaced too. |
| A mission stops with an `InflateBegin` or decompression error | The full error text, including the resource filename at the end, and the exact versions of the mods enabled in that profile. |

Open the active library from **Settings → Suite data → Open folder**, then open its `sessions` folder. Look for the folder dated closest to the incident. For a blank headset after Story Mode loads, share the newest `native\activation-NNNN\xr.log` privately. Older sessions may place it directly under `native`. If it is missing, share the same session's `native\native-runtime.log` or `session.json`.

If GTAVXR crashes, look for `GTAVXR_CRASH` in the affected session's `native\native-runtime.log`, or check `native\native-stdout.log` if the runtime log is empty. When available, `native\gtavxr-crash.dmp` is a local minidump. It may contain process data. Share it privately only if support requests it. Keep the matching `session.json` and note the last screen or action before the crash.

### Default shader error or crash after a graphics change

In GTAVXR v1.0.0, **Shader Quality: Low** can trigger **Unable to create default effect 'common:/shaders/im'** at startup. That setting selects a shader variant unavailable in this release.

1. Open normal GTA V Legacy and go to **Settings → Graphics**.
2. Set **DirectX Version** to **DirectX 11** and **Shader Quality** to **High** or **Very High**. Apply the settings and close GTA.
3. In the Suite, create a separate profile using **Copy from normal GTA** or **New story** and launch it. Keep the affected profile and your original saves.
4. If the same error persists, share the existing session's **native\native-runtime.log**, if present, and the settings you applied. See the session folder described above.

If the game crashed when applying graphics changes, record the settings you changed and their previous values. **MSAA** and **Post FX** can reload shaders. For other settings that need a graphics reset, see [GTA graphics settings](GRAPHICS.md#gta-graphics-settings).

## Report a problem

Include the GTAVXR version, store and Legacy build, GPU and driver, headset and connection, OpenXR runtime, graphics preset, DLSS or NR, OFXR, and active mods. Describe the mission or action, the steps to reproduce the problem, what happened, and whether it also happens with a base profile. Attach the exact error or a screenshot if you can.
