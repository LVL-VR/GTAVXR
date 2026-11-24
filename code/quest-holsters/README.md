# Quest VR holsters and weapon wheel

An optional Story Mode prototype for GTAVXR 1.0.5. The OpenXR layer tracks gestures, renders the eight-category wheel and provides haptics. The Suite script requests a draw or stow from the native hand-specific equipment system. The request depends on the executable fingerprint checked in QueuedDraw.cs.

Tap grip at either hip or shoulder to draw or stow with the reaching hand. Hold shoulder grip to slow time and open the wheel. Hover and release with the same hand to select, or release over empty space to cancel. Click the stick to cycle a category. Hold the right stick downward at a slot to assign the weapon in the reaching hand. One pulse signals range and two confirm a successful save. Hold the right stick downward at the wheel center to enter placement mode. Four small orbs show the slot centers during calibration. Drag with grip and hold the stick down again to save and exit.

## Build and try

Run Build.ps1 from an x64 Visual Studio developer shell with the Windows SDK and .NET Framework compiler. Supply -OpenXRInclude (the SDK folder containing openxr/openxr.h), -Runtime (the Suite gtavxr-1-0-5 runtime folder), and -Output (a fresh build directory). The build runs wheel, gesture, haptics, persistence and OpenXR discovery checks.

Copy the resulting layer folder to %LOCALAPPDATA%\GTAVXR-QuestHolsters\layer. Import QuestFourSlots.zip into the Suite as a script mod and enable it in a separate 1.0.5 profile. Enable only one holster mod per profile. Launch through the Suite. Profiles using this script share settings on the Windows account.

The source-only wheel displays weapon names. The headset-tested personal build uses HUD images from the player's copy of GTA V. Build.ps1 accepts an optional -HudArt path to a prepared GWH1 icon pack. Game artwork is excluded from the contribution. For recording with the installed OpenXR OBS Mirror, add -ObsCapture to the same build command. This places the mirror after the holster layer so it can capture the wheel.

## Headset review

Try cross-body draws and assignments, two copies of the same pistol, dual reload followed by a single-hand reload after stowing, and a shotgun-to-pistol swap. Verify wheel cancellation, normal time after closing, two save pulses on the assigning hand, and saved placement across a restart. Dual-wield sprint remains a known limitation. The source-only package passed its headset checklist on Quest 3 with Virtual Desktop. In the final combined build, the player confirmed weapon artwork in the wheel, orbs visible only during calibration, and wheel capture in OBS.
