# Install, update, or remove GTAVXR

Use **GTA V Legacy 1.0.3889.0** on 64-bit Windows. Download **GTAVXR.exe** from the official Releases page. One Suite handles setup, profiles, maintenance, and launch. [Download GTAVXR](https://github.com/LVL-VR/GTAVXR/releases/latest) · [Check compatibility](COMPATIBILITY.md)

| What you want to do | Where to start |
| --- | --- |
| Play GTAVXR for the first time | [Install and play](#install-and-play) |
| Update an existing installation and keep your progress | [Update and keep your data](#update-and-keep-your-data) |
| Take GTAVXR out of the game and keep your VR profiles | [Uninstall from the game](#uninstall-from-the-game) |
| Delete the Suite and its data or start over | [Remove GTAVXR completely](#remove-gtavxr-completely) |
| Fix a moved game, missing files, or an interrupted install | [Repair and recover](#repair-and-recover) |

## Install and play

1. Run **GTAVXR.exe** as administrator for installation. If prompted, choose the Legacy folder that contains **GTA5.exe**. You can choose a different drive for **Suite data** before continuing.
2. Select **Install GTAVXR**. The Suite checks Legacy files and applies the included patch locally. It creates a **VR Story** profile and copies the eligible regular GTA saves it finds. In the profile's **Save** tab, select a save or **Start a new story**.
3. Close the elevated Suite and reopen it normally. Use **Settings → Headset → Change** to select the runtime for your connection, or use **Automatic** to follow Windows. With the headset connected, select your profile and press **Play**.
4. Start at **Franklin and Lamar** if you have a save after the prologue. Save inside GTA when you finish and follow the closing instructions. [First session](FIRST_SESSION.md) · [Choose a save](SAVES.md) · [Controls](VR_CONTROLS.md)

## Update and keep your data

1. Close GTA and let the Suite finish the session. Get the new **GTAVXR.exe** the way your current version allows:
   - **From 1.0.0:** this version has no update button. Download **GTAVXR.exe** from the release's **Assets** on the Releases page, then close the old Suite.
   - **From 1.0.2 or later:** use **Settings → Check for updates → Open release** and download **GTAVXR.exe** from **Assets**. When the Suite offers **Verify download**, select the downloaded EXE to check its size and SHA-256. Then close the old Suite.
2. Run the new EXE as administrator for the update, using the same Windows account. Open the existing library. If it was moved, choose **Locate Suite data** and select the library folder containing its settings. If two libraries contain different progress, review both and import the missing profiles when prompted.
3. Select **Update GTAVXR** and wait for it to finish. The Suite upgrades eligible older profiles in place and keeps their saves, settings and imported mods. It checks mod compatibility for the new version when preparing to play. If recovery is offered after an interruption, finish **Fix now** before retrying the update. If an earlier update to another version stopped half way, the new Suite finishes it, including the old files it still had to remove, and keeps the old update record in the library's **reports** folder. A stored file of an old version that no longer matches its record is moved to **reports\damaged-store** and reported in the message at the end, never deleted. If that record belongs to a newer version, open that newer launcher instead.
4. Reopen the new launcher normally. Verify your profile and selected save before launching.

What the update changes:

| Item | Where | What happens |
| --- | --- | --- |
| Installed launcher | **%LOCALAPPDATA%\Programs\GTAVXR\GTAVXR.exe**, opened by your Start menu and Desktop shortcuts | **Update GTAVXR** replaces it with the new version. |
| Portable copies | Any earlier **GTAVXR.exe** you saved in another folder | Left as they are. Open **Settings → Previous portable copy**, choose **Replace** or **Remove copy**, then select that EXE. Only unchanged published GTAVXR launchers are accepted. |
| Library | The **Suite data** folder with profiles, VR saves, mods and settings | Kept and moved to the new version. |

If the update stops at a file it did not create inside the library's **runtime** folder, it names that file and leaves it in place. Move the file out of the GTAVXR folder, for example to **Documents**, then select **Update GTAVXR** again. The update continues from where it stopped.

If you uninstalled GTAVXR from the game and kept the library, or lost the earlier EXE, open the current launcher and point it at that same library. Use **Install GTAVXR** if the game is no longer installed with GTAVXR. [Suite and profiles](SUITE.md)

## Uninstall from the game

Close GTA and allow the Suite to finish restoring normal gameplay files. Open **Settings → GTAVXR in the game → Uninstall**. This removes the managed GTAVXR game files and keeps your Suite profiles, VR saves, settings, and imported mods for a later reinstall.

## Remove GTAVXR completely

If the Suite reports an unfinished update, complete **Update GTAVXR** before using **Remove GTAVXR completely**. Keep the update records in the Suite library so the update can resume.

1. Close GTA and finish any **Fix now** recovery or pending update. If the game folder still needs administrator access, reopen the Suite as administrator.
2. In **Settings**, select **Remove GTAVXR completely**. Review the listed libraries and launchers. Confirm only when you are ready to delete the VR saves, profiles, settings, mod library, session files and backups, DLSS models you copied into the Suite's own **dlss** folder, and the installed launcher and shortcuts.
3. Wait for the launcher to close and complete its cleanup. It keeps regular GTA saves and settings in **Documents → Rockstar Games → GTA V**, DLSS models in any other folder you chose, OFXR and its profiles, and files you downloaded or copied to unrelated folders. Portable launcher copies are not removed; use **Previous portable copy → Remove copy** before removal for each one you no longer want.
4. To start over, download the current **GTAVXR.exe** again and follow [Install and play](#install-and-play). The initial profile can import regular GTA saves. Select **Start a new story** if you want a fresh campaign too.

If you already deleted GTA, complete removal can still clean the Suite when it can verify the game is gone. Reconnect a missing drive if asked. If the Suite lists unfamiliar files or recovered player files, move the named files to a location you choose before retrying. Use **Uninstall** above when you want to retain VR progress.

## Repair and recover

- **Interrupted install or session.** With GTA closed, select **Fix now**. Finish recovery before retrying installation or removing the Suite library.
- **Access is denied.** Open the Suite as administrator for writes to the game folder. [Installation recovery](TROUBLESHOOTING.md#access-denied-during-installation)
- **GTAVXR files are missing or changed.** Use **Settings → Check files** with GTA closed. It checks the GTAVXR files in the game folder and the runtime files in the Suite library, restores changed ones from the library and checks them again. Profiles, saves, mods, DLSS models and OFXR are not touched. It reports success only when every file is correct. It also checks that the installed version's record lists every runtime file that version requires; if not, it rebuilds the record from your Legacy game and this launcher's package, or names the missing files when it cannot. **Play** refuses to start with an incomplete record and asks you to run **Check files**. A file held by another program is named; close that program and run **Check files** again. If the library's own copy of the installed release is damaged too, **Check files** rebuilds it from your Legacy game and this launcher's package. When no verified copy is left, use **Update GTAVXR** for GTAVXR's own files or import the named mod again. The report lists any copies actually moved to **reports\damaged-store**. If **Update GTAVXR** names a file in use, close the program using the file and select **Update GTAVXR** again. If the report names a required Legacy file, follow the troubleshooting instructions for that file.
- **GTA moved.** Select the new complete Legacy folder under **Settings → Game → Change**. The Suite checks the base game and any files it previously installed before adopting that folder. A second, still-present Legacy installation can be uninstalled separately. Reconnect the old drive if recovery needs it.
- **Suite data moved.** Use **Locate Suite data** when the stored path is unavailable. Keep the library intact until recovery finishes. [Find where Suite data is stored](SUITE.md#choose-where-the-suite-stores-its-data)
