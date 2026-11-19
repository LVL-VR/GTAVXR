# VR controls

This guide uses the default **Quest Touch** bindings. **L3** and **R3** mean pressing the left or right thumbstick inward. **Tilt** means moving a stick in a direction. The **grip** is the side button, and the **trigger** is the index-finger button. **Menu** is on the left controller. In-game prompts reflect the current context and any bindings you have changed. [Compatibility](COMPATIBILITY.md)

## Open the VR menu and recenter

| Active runtime | Open or close the VR menu | Recenter the view |
| --- | --- | --- |
| **SteamVR** (including Steam Link) | Center both sticks, click **L3 + R3** together and release both. With the game window focused, **F9** is the default keyboard alternative. Change it under **Suite → Settings → SteamVR menu key**. | Open the VR menu and select **Recenter**. |
| **Other OpenXR runtimes**, including VDXR | Press **Menu** on the left controller. | Face forward in your usual posture, center both sticks, click **L3 + R3** together and release both. You can also select **Recenter** in the menu. |

The two-stick shortcut needs both stick clicks. Start the two presses within about 0.1 seconds of each other. If the first stick is still held when you press the second after the 0.1-second window, the second click is ignored until you release it: it does not open the menu, recenter, start the virtual D-pad or reach the game, and the first click keeps its own action, such as stealth or cover on **L3**. Separate clicks without overlap keep their individual actions. Keep both sticks centered for the menu or recenter. For the virtual D-pad, hold both clicks and tilt the right stick. Close the phone and menus before recentering.

## Your first few minutes

1. Enter Story Mode. If VR is off, focus the GTA window and press **F10**. Press **F10** by itself to switch between flatscreen and VR.
2. Face forward and [recenter the view](#open-the-vr-menu-and-recenter). To adjust standing and seated height separately, open **Settings → Calibration → Height** in the VR menu. [Calibration help](TROUBLESHOOTING.md#recenter-the-view)
3. In the VR menu, open **Settings → Movement** to choose smooth or snap turning and whether movement follows your headset or hand. Use **Return to Game** or the same menu shortcut to close the menu.
4. On foot, tilt the left thumbstick to move and the right thumbstick to turn. To check head tracking, look around and move slightly within your play area. [VR menu and Trainer](VR_MENU.md)

## Default controls

| Action | Control |
| --- | --- |
| Open or close the VR menu | Use the [menu shortcut for your runtime](#open-the-vr-menu-and-recenter). **Game Menu** in the VR menu opens the GTA pause menu. |
| Recenter quickly | See the [runtime shortcuts](#open-the-vr-menu-and-recenter). Outside SteamVR, quick recenter works standing or seated. A menu or phone that takes control during the gesture cancels it. |
| Navigate a menu | Use a thumbstick to highlight a row or change a value, **A** to select, and **B** to go back. Release the buttons and center the thumbstick when opening the menu. |
| Move and turn on foot | Use the left thumbstick to move and the right thumbstick to turn. Adjust turning and movement direction under **Movement**. |
| Context actions | Hold **L3** and press **A** for the primary action or **B** for the secondary action. Follow the in-game prompt for the current situation. |
| Enter or leave stealth mode | On foot, briefly click **L3** once. Close the phone, weapon wheel, and menus first. |
| Take cover or use a special ability | Stand near a cover surface with both controllers active. Close the phone, weapon wheel, and menus, then hold **L3** by itself for about half a second. Click **L3** twice to use a special ability when available. |
| Use the phone | Click and release **R3** when the phone is available. At the wheel, a single click may wait briefly for the second click. Navigate with the right thumbstick, use **A** to select, and use **B** to go back. |
| Use the virtual D-pad | Outside the phone, press **L3 and R3**, keep them pressed and tilt the right thumbstick in the desired direction. Release both clicks to exit D-pad mode. In SteamVR, keep the sticks centered throughout the gesture to open the VR menu instead. In other runtimes, the centered gesture recenters the view. |
| Enter or exit a vehicle | Press **Y** with the standard bindings when the game allows it. |
| Switch the driving camera | While driving, click **R3** twice within about 0.3 seconds to switch between the cockpit and third-person views. Close the phone, weapon or radio wheel, and any open menu first. Keep **L3** and **Menu** released. |
| Switch protagonists | Open **VR Menu → Story Character** and choose an available protagonist. For the first switch to Trevor in the prologue, use a standard gamepad or keyboard and mouse. [First session](FIRST_SESSION.md#starting-a-new-game) |
| Remap buttons | Open **VR Menu → Settings → Controller buttons** and choose the on-foot, vehicle, or aircraft layout. For gestures and aircraft weapons, use [VR action bindings](#vr-action-bindings). |

## Remap GTA pad buttons

Open **VR Menu → Settings → Controller buttons**. Choose **On foot**, **Vehicle**, or **Aircraft** with left or right. Highlight **A, B, X, Y, Left stick click**, or **Right stick click** and use left or right to set the pad input GTA receives. **Capture button** jumps to the row for the next supported button you press.

The pad targets include the original input, A/B/X/Y, the left and right bumpers, L3/R3, Back, the four D-pad directions, or Off. **Copy to the other contexts** copies the current layout to both other contexts. **Reset this context** restores only the current layout. The layout is saved with the profile.

Use **VR action bindings** for the seven GTAVXR shortcuts below. The controller maps leave the axes, physical grip gestures, weapon-hand firing, phone navigation, and the VR menu and quick-recenter shortcuts on their original inputs. Use **Free hand buttons** for [weapon support and reload parts](#using-either-hand-and-dual-wielding). Use **Suite → Settings → SteamVR menu key** for the SteamVR keyboard shortcut.

## VR action bindings

Open **VR Menu → Settings → Controller buttons → VR action bindings**. Highlight an action, then use left or right to cycle the available button and gesture combinations. The menu offers **A, B, X, Y, L3, and R3**. These settings are saved per profile. **Reset VR actions** restores the defaults in the table. The separate GTA pad maps remain available from **Controller buttons**.

| Action | Default | What you can change |
| --- | --- | --- |
| Special ability | Double-click L3 | Tap or double-click. Works whenever the character's ability is available, including Franklin's while driving. |
| Take cover | Hold L3 | Hold the selected button for about 0.5 seconds on foot near cover. |
| Stealth | Tap L3 | Tap the selected button on foot. |
| Phone | Tap R3 | Tap and release the selected button to take out the phone. Once open, use the right stick to navigate, **A** to select, and **B** to go back and put the phone away. |
| Driving camera | Double-click R3 | Tap or double-click from the driver's seat to switch between cockpit and third-person views. |
| Aircraft fire | A | Press the selected button in an armed aircraft. While holding the physical throttle, fire with the right trigger. |
| Cycle aircraft weapon | X | Press the selected button in an armed aircraft. |

A double-click requires two presses within 0.3 seconds with a release between them. If the phone and camera share a button, a single press waits for that window while driving. A double-click changes the camera and leaves the phone closed. Assign them to different buttons if you want immediate phone takeout on release.

The menu skips conflicting choices in overlapping contexts. Tap and double-tap can share a button. An aircraft Press action reserves the button while it is held and cannot share it with another action in that context. An active VR action can reserve its button before GTA receives the normal pad input. For example, moving **Phone** to A removes its normal gameplay use as a solo press while the phone shortcut is available. The **L3+A** and **L3+B** context chords keep priority. The change does not affect A or B inside the phone.

## Weapon wheel and holsters

1. On foot, **reach over either shoulder with the hand on that side**. Once your hand is in position, press and hold its **grip** button.
2. While holding the grip, tilt **that hand's thumbstick** toward the weapon category you want. Click the stick inward to cycle through the weapons in the highlighted category.
3. Release the grip to equip the selected weapon in the hand that opened the wheel.

To **holster or draw the last weapon used in that hand**, reach over the same shoulder, then give the grip a quick press and release with the thumbstick centered. In a vehicle, use your **right shoulder** to open the weapon wheel and your **left shoulder** to open the radio wheel.

If the wheel does not open, close the VR menu, check that motion controls are enabled, then release the grip and press it again once your hand is over your shoulder.

## Using either hand and dual-wielding

- **Choose a hand.** Open the wheel with your left hand to equip a weapon on the left or with your right hand to equip it on the right. Aim with the hand holding the weapon and fire with that hand's **trigger**.
- **Equip two weapons.** On foot, equip a compatible weapon in one hand, then equip a second compatible weapon using the other hand's wheel. Each trigger fires the weapon in its own hand.
- **Pass a compatible weapon to your other hand.** Hold the grip on the weapon hand. Bring your empty hand to the weapon's grip and squeeze its grip button, hold both grips briefly, then release the original hand's grip while keeping the receiving grip held. Keep the giving hand's trigger and the receiving hand's support button released. With **Grip for support**, squeeze the receiving hand's trigger instead of its grip. Use the weapon wheel to equip either hand if the transfer is unavailable.
- **Support one weapon.** Bring your free hand to the support point and hold that hand's **trigger**. For compatible pistols and compact SMGs, enable **VR Menu → Settings → Weapons → Support on one-handed guns**.
- **Adjust the weapon or support position.** Open **VR Menu → Settings → Calibration** and choose the weapon or support grip for the relevant side. The left-hand settings follow the right-hand settings until you set separate values. [Calibration options](VR_MENU.md#vr-settings)

Under **Settings → Weapons → Free hand buttons**, you can exchange the free hand's support and reload-part buttons. The instructions below use the default controls. With **Grip for support**, use the free-hand trigger for the steps that use its grip to hold a reload part or take ammunition. Keep using the usual weapon-hand latch, hammer and fire controls. World-object grabs, holsters, driving and melee keep their own controls. [Free-hand weapon buttons](VR_MENU.md#choose-the-free-hand-weapon-buttons)

### Reloading

Under **VR Menu → Settings → Weapons → Reload per family**, select **Quick** or **Realistic** from the modes available for the family. Press **B** for Quick reload with the default bindings. With two weapons equipped, use Quick reload. For physical reload with a single weapon in your left hand, enable **Realistic in the left hand**. Turn on **Reload help glow** to locate the interactive parts and ammo.

#### Unlock a cylinder or hinged action

For a **Heavy Revolver**, **Heavy Revolver Mk II**, **Double-Action Revolver**, or supported break-action weapon, hold the **weapon hand's grip** and hold **A** with the right hand or **X** with the left. **Keep both held while you move the part.** You can use your free hand's grip to move the highlighted part, or move the weapon controller to swing it open. If you release either button midway, the hinge stays where it is. Hold both again to close it, and release them once it seats.

- **Swing-out revolvers.** With the cylinder fully open, point the virtual muzzle up to empty it. Pick up a speedloader at your torso with the free hand's **grip** and seat it in an empty cylinder. To load one round, use **trigger + grip** on the free hand at your torso and bring the round to an empty chamber. Use the free hand to turn the open cylinder while the latch is held if you need to reach another chamber.
- **Break-action weapons.** Open the action fully to eject spent cases. Use the free hand's **grip** at your torso to take one round at a time, then seat it in an empty chamber. Hold the latch controls again and swing the action shut. This applies to the Double Barrel Shotgun, Flare Gun, Marksman Pistol, Compact Grenade Launcher, and Compact EMP Launcher. Cock the Marksman Pistol before firing.

#### Other moving parts

| In-game mechanism | What to do in Realistic mode |
| --- | --- |
| **Navy Revolver** | Use the free hand's **grip** to slide the wedge forward and remove the cylinder. Pick up the replacement cylinder at your torso, seat it, push the wedge back into place, and cock the hammer before firing. |
| **Perico Pistol** | With the free hand, grip and pull the rolling hammer back to open the chamber. Use grip at your torso to take a round and insert it. Push the hammer home to close the action and cock it. |
| **Detachable magazine** | Briefly press and release the reload button to eject the magazine. Use the free hand's **grip** to catch it or take a replacement at your torso. Align the replacement at the opening and seat it fully. If the slide is locked back with a loaded magazine inserted, tap reload to release it. You can also move the highlighted slide or charging handle with your free hand. On this reload route, holding the reload button for about 0.6 seconds requests a Quick reload when available. |
| **Bolt-action rifle** | Use the free hand's grip to lift the bolt handle, pull it back, push it forward, and lower it. Cycle it again after a shot on supported rifles. |
| **MG and Combat MG** | Grip and open the feed cover before removing or inserting the ammo box. Close the cover before firing. |
| **Pump-action shotgun** | Grip the fore-end with your free hand, pull it all the way back, and push it forward after each shot. If you release it at the back, grip it again and push it forward. To load the tube, grip a shell at your torso and insert it at the highlighted loading port. |
| **Combat Shotgun and Sweeper Shotgun** | Load the tube one shell at a time. After loading from empty, pull back the highlighted charging part and let it return. |
| **RPG, Homing Launcher, Firework Launcher, and Musket** | Grip the ammo at your torso and bring it to the highlighted muzzle. Seat it in the direction of the highlighted opening. Cock the Musket before firing. |
| **Grenade Launcher and Snowball Launcher** | Swing the highlighted action open with the free hand. Grip ammo at your torso and load it into the open action. The grenade launcher takes individual rounds and the snowball launcher takes one pack. Close the action before firing. |

**Cock a supported hammer.** Hold the weapon hand's **grip**, let its thumbstick return to center, then tilt the stick down. On supported parts you can also pull the highlighted hammer back with the free hand. The Navy Revolver, Marksman Pistol, and Musket require cocking before the next shot in Realistic mode.

### Aiming aids and throwables

- **Scope zoom.** While looking through a compatible scope, tilt the **right thumbstick** up or down to change magnification. Set the scope view and eye under **VR Menu → Settings → Weapons → Sniper scope**.
- **Laser sight.** Open **VR Menu → Trainer → Weapon Options → Laser Sight** and enable **Toggle**. [Laser sight settings](VR_MENU.md#enable-the-laser-sight)
- **Throw an equipped item.** On foot, equip a supported throwable, hold the trigger on the hand holding it, then release the trigger while making the throw. This uses the held item's controls. Use grip to release loose objects picked up from the world.
- **Two-hand throwable gesture.** Some equipped items, such as the GTA grenade or flare, also respond to the empty hand. Bring that controller next to the virtual item, squeeze its **grip**, and pull it away. Then use the holding hand's trigger or grip to release the virtual item during the throw.
- **Manual detonation.** On foot with a compatible remote-detonated weapon equipped, squeeze the **left grip** to detonate a placed charge when the game allows it. Close the VR menu and keep your hand away from the holster gesture.

## Physical interaction

| Action | How to do it |
| --- | --- |
| **Grab or release an object** | Bring an empty hand near an object you can grab. Hold that hand's **grip** to pick it up. Release the grip to let go. |
| **Throw a held object** | Move your hand in the direction you want to throw and release the **grip** during the motion. The object carries the momentum it gained as you moved it. |
| **Push an object** | Touch it with an empty hand and move your hand in the direction you want it to go. Adjust **Contact push** if you want a different amount of force. |
| **Close your fist** | With no weapon equipped, squeeze the **trigger** of an empty hand. That hand blends into the game's closed-fist pose and opens again when you release the trigger. Each hand works on its own. This changes the hand pose. A hand that holds a weapon, an object, the phone or the steering wheel keeps its own pose, and the driver's triggers stay the throttle and the brake. |
| **Guard against a melee attack** | On foot and unarmed, raise both free tracked hands in front of your face and hold both triggers. Keep the guard up briefly. It can block supported melee attacks from in front of you. |
| **Punch with an empty hand** | Hold that hand's **grip and trigger**, then make the punching motion. The trigger closes the fist you see and the grip with the trigger arms the strike. Adjust **Strike speed** under **Physics → Advanced**. |
| **Strike with a held weapon** | Enable **Gun-butt strikes** under **Settings → Weapons** to strike with a held pistol or rifle. **Weapons vs world** controls how held weapons collide with the environment and push loose objects. |
| **Adjust force** | Open **VR Menu → Settings → Physics**. **Grab / hold** controls grip strength, **Contact push** controls pushing force, and **Body effort** controls the force your body can apply. |
| **Reach for a light object while crouching** | With **Soft legs** enabled, lower your head and reach for the object. To adjust how far you need to lower your head for the soft-leg contact rule, use **Physics → Advanced → Crouch (soft legs)**. The default is 30 cm. |
| **Reduce contact from your legs** | Enable **Soft legs** under **Physics** to keep your feet from pushing loose objects or bodies on the ground. |

Physical interactions depend on the hand and the object. If you cannot grab something, use an empty hand, release the grip, then press it again near the object. [Physics settings](VR_MENU.md#vr-settings)

## Vehicles, flight, and parachutes

- **Driving.** Turn on **VR Menu → Settings → Vehicles → Tracked steering**. Reach for the wheel, hold the **grip** button, and move your hand to steer. If it stops tracking your hand, release and re-grab the wheel. With standard bindings, use the **right trigger** to accelerate and the **left trigger** to brake.
- **Aim from the driver's seat.** In a supported vehicle, hold the **right grip** to aim and use the **right trigger** to fire a supported weapon. With **Stick throttle while aiming** enabled, push the left stick up to accelerate or down to brake and reverse.
- **Third-person driving.** Double-click **R3** from the driver's seat. Click it twice again to return to the cockpit. Use the thumbsticks and triggers for vehicle input in third person. Tracked steering and cockpit hand interactions are available in first person.
- **Vehicle doors and hatches.** With an empty tracked hand, reach for a supported door handle or movable panel. Squeeze the **grip**, move the part with your hand, and release to let go. This depends on the vehicle and whether the part is accessible. Use **Y** for standard entry or exit.
- **Break a side window.** In GTA, make a quick strike into supported side glass with a free tracked hand. Release any door, panel, or object before striking.
- **Front windows.** From the cockpit, use an empty hand to grip an interior door panel. While holding it, center the sticks, then tilt the left stick for the left front window or the right stick for the right front window. Tilt down to lower and up to raise the glass on supported vehicles.
- **Radio.** Reach over your left shoulder and hold the **left grip** in a vehicle to open the radio wheel. Tilt that thumbstick to select a station and release the grip to confirm. See **Settings → Vehicles** for additional stations.
- **Planes and helicopters.** With **Tracked steering** enabled, reach for the cockpit control and hold the **grip** to move it with your hand in supported aircraft. If the cockpit has a separate throttle, use the left hand to grip and move it while steering with the right hand. Use **Vehicles → Planes and helicopters** to adjust sensitivity and stick pitch inversion. In armed aircraft, **X** cycles weapons. Fire with **A**, or with the **right trigger** while holding the physical throttle. You can remap the **Aircraft** layout separately.
- **Parachute.** Select **Normal** or **Immersive** under **VR Menu → Settings → Movement → Parachute**. In **Immersive**, bring an empty hand to the visible ripcord during freefall, hold the **grip**, and pull. Once the chute is open, grip the steering handles and pull down on the side you want to turn toward. In **Normal**, **A** deploys the parachute with standard bindings. The same menu offers camera and automatic deployment settings.

If a mission action does not work with motion controllers, press **F10**, complete it with a keyboard and mouse or a standard gamepad, then press **F10** again to return to VR. [North Yankton prologue](FIRST_SESSION.md#starting-a-new-game)
