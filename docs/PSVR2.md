# PlayStation VR2 through SteamVR

GTAVXR runs on PSVR2 with the PC adapter through **SteamVR's OpenXR runtime**. In the Suite, choose **SteamVR** as the OpenXR runtime (or leave it on **Automatic** with SteamVR set as the active Windows runtime).

## Install the preview bridge

This fork's release ZIP contains `gtavxr_native_openxr.dll` (the bridge), `CheekyOpenXRLayer.dll` (optional CheekyFoveatedDLSS shim) and `install-psvr2-bridge.ps1`. You need GTAVXR 1.0.2 installed by the official Suite first.

1. Close GTA and the Suite.
2. Extract the ZIP anywhere and run: `powershell -ExecutionPolicy Bypass -File install-psvr2-bridge.ps1` (add `-DryRun` first to see what it will do). No administrator rights are needed.
3. Start SteamVR, then the Suite, then **Play**.

The script replaces the bridge in the Suite's runtime folder and updates the hash the Suite checks before Play. The first time, it backs up the official files to `<Suite data>\psvr2-bridge-backup`. `-Restore` puts them back. Reinstalling or updating GTAVXR through the Suite restores the official bridge. Run the script again afterwards.

**DLSS:** put `nvngx_dlss.dll` (and `nvngx_dlssnr.dll` for Neural Rendering) in **Settings → DLSS**. This build fixes the 1.0.2 issue where DLSS never ran and the image was only stretched (`EYE_CHAIN state=blit_fallback stage=d3d12_dll` in the log).

**Optional CheekyFoveatedDLSS (eye-tracked foveated DLSS):**

1. Put the Cheeky fork's standalone `CheekyFoveatedDLSSHost.dll`, `CheekyFoveatedDLSSRuntime.dll` and its ini files in `<GTA V folder>\CheekyFoveatedDLSS\`. Add `nvngx_dlssnr.dll` there too for Cheeky NR.
2. Run the installer. It places the shim there.
3. Use GTAVXR's **Performance (SR)** preset and let Cheeky handle NR.
4. For eye tracking, PSVR2Toolkit's eye tracking must be running. Set Cheeky's foveation centre to **OpenXR gaze**.
5. Press F11 for Cheeky's menu, which also appears in the headset.



SteamVR presents the Sense controllers to OpenXR apps as **Oculus Touch**, and your **SteamVR binding for GTAVXR decides which physical control drives each action**. The bridge only reads those actions. It never reads the controllers directly. The table shows the default auto-remap plus the two manual additions.

| GTAVXR action | Sense control | Notes |
| --- | --- | --- |
| `stick` | Left / right stick | |
| `trigger` | L2 / R2 | |
| `grip` | L1 / R1 | |
| `stick_click` | L3 / R3 | |
| `primary` | Square (X) / Cross (A) | |
| `secondary` | Triangle click (Y) / Circle (B) | |
| `menu` | **Create**, assigned manually | Auto-remap drops it. Either hand counts. |
| `dpad_shift` ("D-pad Shift Thumbrest Touch") | **Triangle touch**, assigned manually | Left hand. `secondary_touch` (Triangle touch) works the same way. |
| `hand_pose` / `aim_pose` | Controller grip / aim pose | |
| `native_rumble` | Controller haptics | |

The startup log line `XR_INPUT_BOUND` lists exactly what SteamVR bound to each action.

## All controls on PSVR2 (default GTAVXR bindings)

| Action | Sense |
| --- | --- |
| **VR menu** open/close | **Create** (tap), or **Triangle touch + R3**, or **L3 + R3** with the sticks centred, or **F9** |
| GTA pause menu (map, settings) | VR menu → **Game Menu** |
| Recenter | VR menu → **Recenter** |
| Menu navigation | Stick to move, **Cross** to select, **Circle** to go back |
| Move / turn | Left stick / right stick |
| **D-pad** | **Touch Triangle (don't click) + push the right stick** (Up = phone in GTA). Turning is blocked while you touch Triangle. |
| Context action (primary / secondary) | Hold **L3** + **Cross** / **Circle** |
| Stealth | Tap **L3** |
| Take cover | Hold **L3** about 0.5 s near cover |
| Special ability | Double-click **L3** |
| Phone | Tap **R3**. In the phone: right stick, **Cross** to select, **Circle** to go back. |
| Enter / exit vehicle | **Triangle** |
| Driving camera (cockpit / third person) | Double-click **R3** in the driver's seat |
| Quick reload | **Circle** |
| Weapon wheel | Reach over a shoulder, hold that hand's **L1/R1**, tilt that stick, click it to cycle, release to equip |
| Holster / draw last weapon | Over the shoulder, quick tap of **L1/R1** |
| Fire | **L2 / R2** on the hand holding the weapon |
| Support a weapon | Free hand at the support point, hold its **L2/R2** |
| Revolver / break-action latch | Weapon-hand grip + hold **Cross** (right) / **Square** (left) |
| Cock hammer | Weapon-hand grip, centre the stick, tilt it down |
| Scope zoom | Right stick up / down |
| Grab / throw objects | Hold **L1/R1** near an object; release while moving to throw |
| Punch | Hold **L1/R1 + L2/R2** (fist), then swing |
| Driving | Tracked steering: hold **L1/R1** on the wheel. **R2** accelerates, **L2** brakes. |
| Aim from the driver's seat | Hold **R1**, fire with **R2** |
| Radio wheel in vehicle | Left shoulder, hold **L1**, tilt the left stick |
| Aircraft | **Cross** fires, **Square** cycles weapons, grip the cockpit controls |
| Parachute | **Cross** (Normal), or grip the ripcord (Immersive) |
| VR on / off | **F10** |

Square, Triangle, Cross, Circle, L3 and R3 can be remapped per context under **VR Menu → Settings → Controller buttons**, and gestures under **VR action bindings**.

## Opening the GTAVXR menu

On SteamVR, GTAVXR ignores the Menu button and opens its VR menu with **L3 + R3** pressed and released with both sticks centred. On PSVR2 the bridge replays these as that chord while you hold them:

- **Create**, after you assign left **Create** to the `menu` action in **SteamVR → Settings → Controllers → Manage Controller Bindings → GTAVXR**. SteamVR's automatic Touch→Sense remap drops Create, so you have to assign it yourself.
- **Triangle touch (without clicking) + R3.** Bare R3 is unchanged.
- **L3 + R3** with the sticks centred, or **F9** on the keyboard.

For the GTA pause menu (map, game settings), open the VR menu and choose **Game Menu**.

## D-pad (Triangle touch + right stick)

Rest a finger on **Triangle** without clicking it, then push the **right stick** in a direction to press D-pad up, down, left or right. Turning is blocked while your finger is on Triangle. Clicking Triangle is still Y.

The bridge exposes this modifier as the **D-pad Shift Thumbrest Touch** action, bound to left `thumbrest/touch`. In **SteamVR → Manage Controller Bindings → GTAVXR**, assign left **Triangle touch** to it. That's the same setup as UEVR's `LEFT_TOUCH` method. The default Triangle touch (`y/touch`) binding also works without editing.

Under the hood, the bridge holds L3 + R3 once the stick passes halfway and keeps them held until you lift your finger. The game's own D-pad chord then picks the direction. Because the stick is already tilted when the clicks appear, it never triggers the centred-sticks menu chord. Map D-pad directions to actions per context in **VR Menu → Settings → Controller buttons**.

## Hand pose

The Touch-emulated Sense grip pose tends to tilt the hands downward. If your hands or weapons look pitched down, use **VR Menu → Settings → hand and weapon calibration**. In another title, about **+26° wrist pitch** corrected it. Use that as a starting point, not a fixed value.

## Diagnostics

The bridge log shows:

```text
XR_SYSTEM name=SteamVR/OpenXR : playstation_vr2 psvr2=1
XR_INPUT_PROFILE left=/interaction_profiles/oculus/touch_controller right=/interaction_profiles/oculus/touch_controller
XR_PSVR2 triangle_touch_r3_menu confirmed
XR_PSVR2 dpad_shift confirmed
```

If the controllers drop out or freeze, check SteamVR's `vrserver.txt` for `[CONT_L/R] Lost` or `The frame is stuck` before you debug bindings. Those point to tracking, battery, lighting or Bluetooth.

## Resolution

At 100% SteamVR scale, PSVR2 recommends about **3400×3468 per eye**. GTAVXR uses the runtime's recommendation, so lower SteamVR's per-app resolution if performance is short.
