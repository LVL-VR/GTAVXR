# GTAVXR changelog

## 1.0.5

1.0.4 was a test candidate and was not published. Its changes are listed here with the 1.0.5 fixes.

### Added

- **Stereo Theatre** presents supported in-engine Story cutscenes on a fixed screen in stereo. **Stereo Theatre** is the default. A saved choice is preserved and can be changed under **VR Menu → Settings → Camera → Cutscenes**. [Cutscene camera guide](docs/VR_MENU.md#watch-cutscenes-in-stereo-theatre)
- **Quick recenter** outside SteamVR. Center both sticks, click **L3 + R3** and release both. SteamVR uses the same gesture for the VR menu and retains its configurable keyboard shortcut. [Runtime shortcuts](docs/VR_CONTROLS.md#open-the-vr-menu-and-recenter)
- Each free tracked hand uses the game's closed-fist pose when you squeeze its trigger while unarmed. The physical strike and guard controls keep their existing behavior. [Physical interaction](docs/VR_CONTROLS.md#physical-interaction)

### Fixed

- Fixed a crash when adjacent interiors reused the same entrance or exit portal and one of them unloaded.
- The Suite handles **Access is denied** when Windows still lists an exited GTA process. A live process still blocks launch and maintenance.
- Preserved stick-click timing when routing single clicks, double clicks and two-stick gestures. With the default controls, double-click **R3** from the driver's seat to switch between cockpit and third person. Click once and release to take out the phone. [Driving camera and phone bindings](docs/VR_CONTROLS.md#vr-action-bindings)
- Stereo Theatre falls back to the regular VR camera if its stereo frame cannot be presented. A failed pair is discarded together, and stale depth references are released.
- Added per-eye SR/NR diagnostics around presentation changes to help identify active rendering paths and fallbacks. [Graphics troubleshooting](docs/TROUBLESHOOTING.md#mods-and-graphics)
- If the first stick is still held when you press the second after the 0.1-second window, the second click is ignored until you release it. It no longer forms the **L3 + R3** gesture or the virtual D-pad, and it does not reach the game. The first click keeps its own action. [Runtime shortcuts](docs/VR_CONTROLS.md#open-the-vr-menu-and-recenter)
- **Check files** now also checks the runtime files the game loads from the Suite library, restores them, and reports success only when every file is correct. The Suite can rebuild a missing runtime record and damaged stored copies from this launcher's verified package. It reports files that need further attention. Files held by another program are named for a retry. [Repair and recover](docs/INSTALLATION.md#repair-and-recover)
- **Update GTAVXR** finishes an earlier update to another version that stopped half way, instead of refusing it, including the old files that update still had to remove. A stored file of an old version that no longer matches its record is kept in **reports\damaged-store** and reported, not deleted. [Update and keep your data](docs/INSTALLATION.md#update-and-keep-your-data)
- The Suite shows OFXR as generating frames only when OFXR's status belongs to the running game. When the status does not say whether OFXR is generating, the Suite says so instead of reporting no generation. The status is also shown on the Play screen while the game runs. Values OFXR has not measured show as **not measured**. **Create recommended profile** changes one setting in one OFXR profile. **Undo OFXR change** restores only that setting and keeps other edits, the encoding, and line endings. [OFXR Bridge](docs/GRAPHICS.md#ofxr-bridge)
- **Remove GTAVXR completely** lists libraries and launchers separately and says which DLSS models are removed and which are kept. [Remove GTAVXR completely](docs/INSTALLATION.md#remove-gtavxr-completely)
- Extended Suite validation for in-place updates from 1.0.0, 1.0.2 and the 1.0.4 candidates, and for portable launcher replacement. [Update and keep your data](docs/INSTALLATION.md#update-and-keep-your-data)

## 1.0.3 — development changes included in 1.0.5

- Corrected a Windows executable header issue in the game image that could prevent the OFXR layer from loading.
- Kept existing HUD images when the runtime refuses to grow a panel. A panel can retry or be omitted temporarily without dropping the game projection. Session or device failures still follow the normal recovery path.
- The Suite can offer **Update GTAVXR** when a newer package replaces an earlier candidate with the same version number. It recognizes eligible earlier launchers by their exact identity.

- Update names an occupied file and tells you when to retry. Check files distinguishes a blocked repair from files without a verified replacement.
- Choosing the recommended OFXR profile again makes Undo available after an editor changes the spelling of the off value.
- A crash can leave a size-limited local minidump and its exception signature in the session logs.

## 1.0.2

- Added SteamVR support through OpenXR. Quest 3 via Steam Link verified in Story Mode. Fixed world presentation and duplicate SteamVR startup waits.
- Restored subtitles and objective text in VR and added movable subtitles with a placement guide. HUD changes keep notifications and wanted stars visible in more cases.
- Select the exact Story save before launching a profile. Start a new story or import saves into the existing profile.
- Migrate 1.0.0 profiles in place, choose where the Suite stores its data, and remove GTAVXR completely from managed locations.
- Expanded remapping for special ability, cover, stealth, phone, vehicle camera, aircraft fire and aircraft weapon selection. Added a choice of grip or trigger for weapon support and reload interaction with the free hand.
- Improved graphics and content compatibility checks, disk-space messages, and interrupted-session recovery.

## 1.0

First public release of GTAVXR by LVL.

- Install, configure, and launch GTAVXR from the Suite.
- Explore the open world and play Story Mode missions in VR with OpenXR, head tracking, and motion controllers.
- Manage saves by profile, copy your GTA V Legacy progress, or start a new story.
- Use the integrated Trainer and Script Host for compatible mods.
- Import content packs and choose graphics options by profile.
- Browse code and tests for individual components.

[Compatibility](docs/COMPATIBILITY.md) · [Known issues](docs/KNOWN_ISSUES.md)
