# Suite and profiles

The **Play** screen lists your profiles. Select a profile to review its version, saves, mods, and settings before you launch. You can use another profile to try a different setup while keeping your main campaign.

For a first install, an update from an earlier version, or a clean reset, use the step-by-step guide. [Install, update, or remove GTAVXR](INSTALLATION.md)

## Profile editor

| Tab | What you can do |
| --- | --- |
| **Version** | View or select an installed version that works with the profile and review any conflicts the Suite detects. |
| **Save** | Choose the exact save to launch, copy your normal GTA saves, or start a new story. [Profiles and saves](SAVES.md) |
| **Mods** | Enable or disable library mods for this profile, configure the options they offer, and adjust population or traffic when supported. Changes apply the next time you click **Play**. [Mods](MODS.md) |
| **VR settings** | See whether the profile uses defaults, select **Copy from another profile**, or select **Back to defaults**. Copying settings replaces the destination profile's VR settings. Graphics options also appear here when available. [Graphics](GRAPHICS.md) |
| **Profile** | Rename the profile, choose its color and image, duplicate it, or delete it. Use **Duplicate profile** to keep a separate copy for a different setup. You can also turn a profile into a test profile. |

Use the main **Mods** screen to import mods into your library. To use a mod in-game, enable it in the profile you plan to launch. [Import and enable mods](MODS.md#import-and-enable-mods)

## Try a mod in a test profile

From the main **Mods** screen, select **Try it** on a mod when that option is available. Choose an existing test profile or create one. In the profile's **Profile** tab, use **Set the starting point here** to capture its current saves and VR settings. You can restore that starting point between runs. Once you have verified the setup in-game, choose **Pass to my profile…** to apply the version and mod selection to a regular profile. The target profile keeps its own saves and VR settings. [Mods and compatibility](MODS.md)

## Suite settings

| Control | What it does |
| --- | --- |
| **Language** | Switches the Suite between English and Spanish. The VR menu has a separate language setting under **Menu appearance**. |
| **Game → Change** | Selects the Legacy folder containing **GTA5.exe**. If GTAVXR is already installed in a different folder, the Suite offers to uninstall it before switching. |
| **Headset → Change** | Selects an OpenXR runtime. **Automatic** uses the active Windows runtime each time you click **Play**. You can also select a detected runtime or its installed JSON manifest. Reselect the runtime if it was moved or uninstalled. |
| **DLSS** | Supply your own NVIDIA files compatible with your GPU. Set their location here, then choose the graphics preset and intensity in each profile. [DLSS setup](GRAPHICS.md#set-up-dlss-files) |
| **OFXR (optional)** | Shows whether the layer is registered and lets you locate the OFXR tray app when available. [Set up OFXR](GRAPHICS.md#ofxr-bridge) |
| **GTAVXR in the game** | Shows the installed version and offers **Check files** and **Uninstall**. **Check files** covers the game folder and the Suite's runtime files. [Repair](INSTALLATION.md#repair-and-recover) · [Uninstall](INSTALLATION.md#uninstall-from-the-game) |
| **This app / Update** | Installs the Suite for its shortcuts, checks official GitHub Releases, and opens the available release for download. It offers to verify the downloaded EXE when a SHA-256 is available. The downloaded EXE then installs itself with **Update GTAVXR**. [Update and keep your data](INSTALLATION.md#update-and-keep-your-data) |
| **Previous portable copy** | Selects an earlier standalone launcher that the Suite recognizes so you can replace or remove that copy. Use **Update GTAVXR** for the managed launcher used by your shortcuts. [Update and keep your data](INSTALLATION.md#update-and-keep-your-data) |
| **SteamVR menu key** | Selects F7, F9, F11, or Off for the keyboard shortcut used when SteamVR is active. F9 is the default. |
| **Remove GTAVXR completely** | Removes the identified Suite libraries, the installed launcher and its shortcuts. Review both lists before confirming. [What is kept](INSTALLATION.md#remove-gtavxr-completely) |
| **Developer mode** | Shows additional development tools. The standard workflow covers installation, updates, and play. |

## Choose where the Suite stores its data

| Location | What's there |
| --- | --- |
| **%LOCALAPPDATA%\GTAVXR\Suite** by default | Profiles, VR saves and settings, imported mods, reusable game and runtime files, and session backups and logs. Location and size can be checked with **Open folder**. The library can be on a different drive from GTA. |
| **%LOCALAPPDATA%\GTAVXR\suite-location.json** when using a custom library | Records which library the Suite opens and tracks known library locations. |
| **%LOCALAPPDATA%\Programs\GTAVXR** when you install the launcher for its shortcuts | Managed launcher. Your original download may also be in Downloads or any other folder you chose. |
| **Your GTA V Legacy install** | Managed GTAVXR game files plus your existing game. **Uninstall** removes the managed files. |
| **Documents → Rockstar Games → GTA V** | Regular GTA user data. The Suite temporarily stages profile saves for play and restores regular GTA saves on closure or recovery. |

The launcher can also create a short-lived update or removal helper in the Windows temporary folder. The helper cleans up after the Suite exits.

Installing is more than downloading the launcher. Profiles and imported content occupy disk space, and patching and recovery can temporarily need additional room. Use the Suite's required-space and drive message to identify where space is needed.


1. Close GTA. If the Suite shows **Fix now**, finish recovery first.
2. On the first setup screen, use **Change** under **Suite data**. After setup, open **Settings → Suite data → Change**. Choose a parent folder on a local drive. The Suite creates a new **GTAVXR Suite** folder there.
3. Select **Copy and use**. Wait for the Suite to copy and verify your profiles, saves, mods, settings, and recovery files. Reopen the Suite when prompted.

Use **Open folder** to find the active library. The preceding library stays available as a backup. To reclaim space, first confirm your profiles and saves open from the new location, then archive the old library or use the documented removal flow when you no longer want the data. Keep the new drive connected while using the Suite.

## After playing

Save before closing GTA and wait for the Suite to finish syncing the session. Select **Play** with the same profile to resume. If a recovery notice appears, follow it before starting another session. [Closing and recovery](TROUBLESHOOTING.md#closing-and-recovery)
