# VR menu and Trainer

In Story Mode with VR active, use the [menu shortcut for your runtime](VR_CONTROLS.md#open-the-vr-menu-and-recenter). Center the sticks before navigating. Use a thumbstick to highlight a row or change a value, **A** to open or apply, and **B** to go back. Use **Return to Game** or the menu shortcut to close the menu. Exit the GTA pause menu first if it is open.

## Main menu

| Entry | What it does |
| --- | --- |
| **Quick actions** | Recenter, raise or lower your height in 2 cm steps, set the time to noon or advance it an hour, restore automatic weather, repair or clean a vehicle, adjust your wanted level, and restore health or armor. |
| **Trainer** | Opens the Story Mode tools for characters, weapons, vehicles, weather, and time. |
| **Game Menu** | Opens the GTA pause menu, including the map and game settings. |
| **Settings** | Opens the VR-specific settings. |
| **Mods** | Adjusts pedestrians, traffic, and Online maps in Story for the active profile. [In-game world settings](MODS.md#in-game-world-settings) |
| **Story Character** | Opens the Story Mode protagonist selector. |
| **Character Menu** | Opens GTA's character interaction menu when available. |
| **Recenter** | Centers the view in the direction you are looking and resets the height reference. Also available under **Quick actions → Recenter view**. With a runtime other than SteamVR, **L3 + R3** with both sticks centered sends the same request without opening the menu. [Runtime shortcuts](VR_CONTROLS.md#open-the-vr-menu-and-recenter) |
| **Return to Game** | Closes the VR menu. |

## VR settings

| Submenu | Options |
| --- | --- |
| **Calibration → Hands** | Choose **Automatic (OpenXR)** to use the controller's palm reference or **Custom** to use your own values. Adjust pitch and position separately for each hand. Position uses X, Y, and Z offsets in millimeters. You can reset pitch for both hands or position for each hand. |
| **Calibration → Weapons and support grips** | Adjust weapon position and rotation for each hand and set where the free hand supports the weapon. The left-hand values follow the right until you set them separately. When available, choose **Class / family** or **This weapon** before editing. |
| **Calibration → Height** | Face forward and select **Recenter view and height**. Use left or right to adjust standing or seated height in 2 cm steps. **Measure height (3 s)** measures standing height without changing your view direction or manual offset. **Eye height** adjusts your distance from the floor in 1 cm steps and applies at launch. |
| **Calibration → Current vehicle** | Adjust your head position for the current vehicle and seat. Make the adjustment from your usual seated posture. |
| **Movement** | Choose smooth or snap turning and set the turn speed or angle. **Walking direction** follows either your headset or the direction of your left controller. **Darken inside walls** covers the view when your head enters a wall or vehicle surface. Under **Parachute**, choose Normal or Immersive mode and adjust camera behavior, dive view, or automatic deployment. |
| **Body view** | Choose how your first-person body appears in each situation. **Motion controls off** keeps the buttons active and returns aiming to your head direction and steering to the thumbsticks. You can apply the same setting to all contexts or reset it. |
| **Weapons** | Choose a fixed or sliding support grip, one-handed weapon support, realistic or reduced recoil, reload mode by family, realistic left-hand reload, and reload highlights. You can adjust weapon strikes and collisions with the world. Under **Sniper scope**, choose the scope view, eye, and angular size. [Weapons and dual-wielding](VR_CONTROLS.md#using-either-hand-and-dual-wielding) · [Realistic reloads](VR_CONTROLS.md#reloading) |
| **Physics** | Adjust grab strength, contact push, and body effort. **Soft legs** changes contact with loose objects and bodies on the ground. **Advanced** sets the crouch threshold and the hand speed needed for a strike. You can reset forces and advanced settings from their respective screens. [Physical interaction](VR_CONTROLS.md#physical-interaction) |
| **Vehicles** | Enable tracked steering or additional radio stations, choose camera and tracking by vehicle type, and adjust sensitivity and pitch for planes and helicopters. **Level** follows heading. **Full tilt** follows banking and pitch. **Horizon lock** follows pitch while keeping the horizon level. **Smoothed** follow filters bumps and impacts. **Controls in the vehicle frame** keeps banking from moving a control under a still hand. **Hardened level view** stabilizes the view during steep maneuvers. **Stick throttle while aiming** uses the left stick for acceleration, braking, and reverse while aiming from the driver's seat. |
| **Controller buttons** | Choose **On foot**, **Vehicle**, or **Aircraft** and remap A, B, X, Y, L3, and R3. **Capture button** jumps to the next button you press. Copy a layout to the other contexts or reset only the current one. Use **VR action bindings** on this page to change the separate action assignments. Menu and recenter controls are available as usual. |
| **HUD and interfaces** | Configure the wrist HUD, hide the HUD while aiming, show or hide the map, wanted stars, notifications, subtitles, and other elements, and adjust wrist panels and buttons, hand indicators, button prompts, native menu and app size, and pause screen size. **HUD visibility → Show all** restores visibility for every HUD element. |
| **Haptics** | Adjust vibration strength and soft engine rumble. |
| **Image and performance** | Choose **Two Frames** for default CPU/GPU synchronization, **Overlap** as an alternative, or **Serial** for diagnosis. The headset sets the pace with the **Game FPS limiter** off. **Damage flash** and **Game screen filters** control the visual effects of hits and scripted events. In **Neural graphics (DLSS)** you can check status, change Neural Rendering and its strength, or compare A/B. Choose the Super Resolution level in the Suite for your next launch. [Graphics](GRAPHICS.md) |
| **Menu appearance** | Choose the menu language. Under **Colours**, adjust red, green, blue, and transparency for each element. Reset an individual color or the entire appearance. |
| **Camera** | **Cutscenes** chooses how cutscenes are shown. **Current VR Camera** keeps you inside the scene and your head moves the view. **Stereo Theatre**, the default for supported cutscenes, plays the cutscene in 3D on a large screen fixed in front of you, with its original framing and field of view, and you can look around the screen without moving the camera that films the scene. The choice is saved with the profile and can be changed during a cutscene. **Change native camera** switches the GTA camera from the VR menu. [Stereo Theatre](#watch-cutscenes-in-stereo-theatre) |

### Watch cutscenes in Stereo Theatre

**Stereo Theatre** starts by default on profiles without a saved choice. Open **Settings → Camera → Cutscenes** to change it. When a cutscene starts, the screen appears about three metres in front of the direction you are facing and stays there while you move your head. Each eye receives its own image, so the scene has depth behind the screen. Recenter to bring the screen in front of you again. The screen is also placed in front of you again after more than two seconds between theatre frames. A runtime session stop or restart, a change to the OpenXR tracking space, or a recenter can also place the screen again after a shorter interruption. Subtitles, the VR menu and the wrist panels stay where you placed them.

Stereo Theatre applies to supported 3D cutscenes rendered by the GTA cutscene director, including a cutscene that starts with the character seated in a vehicle. Other scripted cameras, ordinary driving, pre-rendered video, the scope and the pause menu use their usual presentation. If the theatre cannot display a complete stereo pair, the cutscene resumes with the current VR camera. Select **Current VR Camera** to use that view for all cutscenes.

### Set the VR action bindings

Open **Settings → Controller buttons → VR action bindings**. Set the buttons for special ability, cover, stealth, opening the phone, switching the driving camera, and firing or cycling weapons in an armed aircraft. Use the [VR action bindings table](VR_CONTROLS.md#vr-action-bindings) for available gestures and phone navigation.

### Choose the free-hand weapon buttons

Open **Settings → Weapons → Free hand buttons** and use left or right to select a mode.

| Mode | Support the weapon | Handle reload parts and ammunition |
| --- | --- | --- |
| **Trigger for support** (default) | Free-hand trigger | Free-hand grip |
| **Grip for support** | Free-hand grip | Free-hand trigger |

The trigger on the hand holding the weapon still fires it. The weapon-hand latch and hammer controls, holsters and physical grabs keep their own inputs. **Support grip → Fixed / Sliding** changes how the supporting hand follows the weapon. [Weapon-specific reloads](VR_CONTROLS.md#reloading)

### Find HUD elements in VR

Open **VR Menu → Settings → HUD and interfaces** and enable **Wrist HUD**. Under **HUD visibility**, enable **Subtitles (panel)** and turn subtitles on in GTA's own settings. Close the VR menu. Dialogue subtitles appear on a panel below the centre of your view. The subtitle panel can be moved, anchored or resized from **Panels by hand**.

Mission objective text that GTA sends through its dialogue and print queue appears on the front panel. Help and activity prompts normally appear on the right wrist, alongside notifications. Native notifications may appear in front when the right wrist or the notification's live bounds are unavailable. Wanted stars normally sit below the minimap. If the map is absent, check the left wrist or the front HUD when its controller is untracked.

In **HUD visibility**, use the individual switches for **Money**, **Notifications**, **Phone messages**, **Help / activities**, **Subtitles (panel)**, **Location**, **Status**, **Other HUD**, **Wanted stars**, **Map** and **Big messages**. **Hide HUD while aiming** hides the wrist map, money and notices while the free hand supports the weapon. Use **VR button prompts** to switch between VR input icons and standard Xbox prompts. If a specific element is missing, note the mission, what text or icon you expected, and whether it showed in the desktop mirror.

### Position HUD panels with your hands

1. Open **Settings → HUD and interfaces**. Under **Wrist panels**, choose the left wrist for the map and wanted stars or the right wrist for money and notifications. Adjust the whole panel or, where available, individual elements.
2. Under **Panels by hand**, select **Move panels by hand**. After the menu closes, reach for the panel and hold the grip button. Release it where you want it, then press **B** to save and finish. Use **Anchor**, **Face the head**, and **Size** to adjust how a panel follows you. **Copy setting**, **Paste setting**, and **Reset panel** let you reuse or restore placements.
3. In a vehicle, enable **Vehicle panels** to place the map on the dashboard. **This model only** applies the placement to your current vehicle model. Turn it off to share the placement across that type of vehicle.
4. Under **Wrist buttons**, assign an action to each slot. Select **None** to hide a slot. Touch the button with a finger from your empty hand. You can hide GTAVXR hand indicators and keep the gameplay actions available.

### Move or resize subtitles

1. Open **VR Menu → Settings → HUD and interfaces → Panels by hand**. Set **Panel** to **Subtitles**.
2. Select **Move panels by hand**. A guide appears at the subtitle position if nobody is speaking. Hold either controller's side grip and move your hand. With **Subtitles** selected you can start the move from a comfortable position.
3. Release the grip where you want the subtitles, then press **B** to save. Use **Size** to resize it and **Anchor → Head** to keep it in front of you. **Reset panel** restores the default. The game uses the placement for the current on-foot or vehicle layout.

### Graphics quality options

**FXAA** and **Tessellation** apply immediately. **VR render scale** applies after restarting the game. The panel also shows shadows, soft shadows, reflections, MSAA, ambient occlusion, water, Post FX, grass, particles, and textures. Read-only rows and restart notices show the current values and when they apply. Check the graphics guide before changing settings in GTA's pause menu. [GTA graphics settings](GRAPHICS.md#gta-graphics-settings)

## Use the Trainer

1. Open **VR Menu → Trainer** in Story Mode. Choose a category with the thumbstick and confirm with **A**. Press **B** to go back. In lists with favorites, use **Y** to add or remove the selected item.
2. Open the category for the change you want to make. The current character, vehicle, and installed content determine which choices are available.

| Trainer page | What you can do |
| --- | --- |
| **Player Options** | Change health, armor, cash, wanted level, and abilities. Use **Model Changer** or **Wardrobe** to adjust appearance and **Cloning Options** to create or manage character copies. **Max All Stats (SP)** applies to the three Story protagonists and persists when you save. |
| **Vehicle Options** | Spawn vehicles by category. Repair or clean your current vehicle, and use **Menyoo Customs** to change supported parts, paint, wheels, or lights. **Auto Drive** drives to a waypoint or a random destination. |
| **Teleport Options** | Move to a map waypoint, mission objective, map blip, preset location, or custom coordinates. Some destinations and interiors depend on game state. |
| **Weapon Options** | Manage weapons, ammo, attachments, tints, and parachutes. Use **Loadouts → Save Current Loadout** to save your equipment. Applying a loadout replaces the character's current weapons. |
| **Weather Options** and **Time Options** | Change the weather or set and freeze the clock. |
| **Settings** | Change the Trainer's language, theme, and colors. Use **Reset Toggles** to stop ongoing effects or **Reset All Cheats** to restore reversible values controlled by the Trainer. Cash, stats, spawns, and progress can persist after a reset. |

### Enable the laser sight

Open **VR Menu → Trainer → Weapon Options → Laser Sight**, then enable **Toggle**. Use **Set Colour** to change the line and dot. The effect follows the active weapon when the Trainer can read its aim. It is unavailable on melee weapons and throwables and pauses during reloading.

Reset active cheats before resuming a mission that depends on normal game rules.

Catalogs depend on installed content and the current context. If you want to preserve your main campaign, try changes to the game world in a separate save.

## Save or restore settings

VR settings you save in-game return to the active Suite profile when the session closes. Some settings apply when GTA starts. In the Suite, use **VR settings** to copy settings from another profile or restore the defaults. Trainer actions, VR settings, and Story Mode saves are managed separately. [Suite and profiles](SUITE.md) · [Profiles and saves](SAVES.md)
