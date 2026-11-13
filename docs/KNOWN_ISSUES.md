# Known issues in GTAVXR 1.0

GTAVXR focuses on open-world exploration. If a Story Mode sequence gives you trouble in VR, follow the steps below.

## Microsoft Defender reports

Some users have reported that Microsoft Defender flagged `GTAVXR.exe`. We scanned the exact installer published with v1.0.0 using Microsoft Defender, and our scan reported no threats.

The installer unpacks the bundled VR components and uses your existing GTA V Legacy files to build the VR executable locally. Antivirus checks can flag legitimate installers that unpack executable files.

To verify your download, compare its SHA-256 with the value listed under **Release assets**. You can also scan the installer with your preferred antivirus or [VirusTotal](https://www.virustotal.com/).

## Story Mode, camera, and cutscenes

| Issue | What to do |
| --- | --- |
| **The camera is out of place after a mission.** You may appear below the map, see only the ocean, or face an unexpected direction. | Face forward in your usual playing posture and select **VR Menu → Recenter**. Do the same if your position or body pose is out of alignment. [How to recenter](TROUBLESHOOTING.md#recenter-the-view) |
| **Cutscenes can be disorienting.** They retain their original framing, and some perspective changes are abrupt in VR. | If a cutscene makes you feel motion sick, skip it when the game allows. Recenter when you regain control if the camera is out of place. |
| **The first switch from Michael to Trevor can fail in North Yankton.** The character wheel may close when you try to select Trevor with motion controllers. | When the guard grabs Michael, press **F10**. Hold **Left Alt**, move the mouse toward Trevor, then release **Alt**. You can also switch with a standard gamepad. Press **F10** again to return to VR. [Prologue walkthrough](FIRST_SESSION.md#starting-a-new-game) |
| **A mission step does not advance with motion controllers.** | Press **F10**, finish that step in flatscreen with a keyboard and mouse or a standard gamepad, then press **F10** again to return to VR. |

For your first session, load a save at **Franklin and Lamar**. If you start a new game, you can complete the entire prologue with VR off. [First session](FIRST_SESSION.md)

## Driving

If the steering wheel stops following your hands, **release the grip button and grab the wheel again**. Use **VR Menu → Settings → Vehicles** to review driving and camera settings by vehicle type. [Vehicle controls](VR_CONTROLS.md#vehicles-flight-and-parachutes)

## Installation and shutdown

- **Access is denied during installation.** Close GTA and the Suite. Right-click **GTAVXR.exe** and select **Run as administrator**. If a recovery notice appears, select **Fix now** and wait for it to finish. [Recover an interrupted installation](TROUBLESHOOTING.md#access-denied-during-installation)
- **GTA may remain running when you exit from the pause menu.** Save your progress and use **Force GTA to close** in the Suite. If the process remains active, follow the closing instructions. [Close the game and play again](FIRST_SESSION.md#close-the-game-and-play-again)

## OFXR and Virtual Desktop

- If **F10** also opens OFXR, assign a different key to OFXR in its tray app. **F10** switches between VR and flatscreen in GTAVXR.
- If you enable OFXR frame generation, disable **SSW** in Virtual Desktop to avoid running two interpolation modes at once. [Set up OFXR](GRAPHICS.md#ofxr-bridge)

If a problem persists, include the mission or action, headset model, and runtime in your report. [What to include in a report](TROUBLESHOOTING.md#report-a-problem)
