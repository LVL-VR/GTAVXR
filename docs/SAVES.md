# Profiles and saves

Each profile has its own GTAVXR version, mod selection, VR settings, and saves.

The Suite can copy your GTA V Legacy saves into a GTAVXR profile. Your normal GTA saves remain separate.

Open **Play → profile → Save**. Select a specific slot, then click **Play**. Rows show the mission, progress, and source path so you can distinguish accounts with the same slot number. Use **Copy saves from normal GTA** to add slots while keeping the profile's VR progress. **Start a new story** starts from the prologue in the same profile and keeps its other saves available for a later launch.

When creating a profile, choose **Copy from normal GTA** or **New story**. You can keep one profile for your current campaign and another for a fresh start.

Save your progress in GTA and close it through the Suite. The next launch selects the newest slot written during that session. If starting from a different save displaced an existing slot, the previous save appears in the list under **StoryArchive**. To select a different saved campaign from the Suite, finish closing the current session first.

When you select an exact save, GTA starts with that slot available. Other archived and imported slots remain in the Suite. **GTA automatic selection** uses the profile's original loading rules and is useful when you want GTA to see the locally staged slots and choose among them in **Game → Load Game**.

To play without optional mods, turn them off in the same profile's **Mods** tab before clicking **Play**. The profile keeps its saves. A save that depends on assets or scripts from a mod may still need that mod to load or play correctly.

### Import saves from another Suite library

With GTA closed, open the destination profile's **Save** tab and select **Import from another library**. Choose the source library's `profiles/<profile>/profile.json`. The Suite adds the source's Story saves and keeps distinct versions of the same slot. The original library and the destination profile's mods and VR settings are preserved. Select the imported save before clicking **Play**.

### Recover after moving Documents

For new sessions, **Fix now** uses the GTA saves folder recorded when the session began. If the folder is missing or has been replaced, put the original folder back at the displayed path and retry. Leave the recovery files in the Suite library until recovery completes.

## Missing imported saves in v1.0.0

Update with your existing Suite library to use the exact Story save selector. [Update and keep your data](INSTALLATION.md#update-and-keep-your-data)

The Suite may display an imported save even when Story Mode starts the prologue or the save is missing from **Pause Menu → Game → Load Game**. Once you can control your character, check that menu. If your save is listed, select it there. If it is missing, keep your original GTA V Legacy save and report whether it still loads in normal GTA. You can use a separate **New story** profile in the meantime.

## Where the Suite stores profiles

Use **Settings → Suite data → Open folder** to locate the active library. The default profiles folder is `%LOCALAPPDATA%\GTAVXR\Suite\profiles`. Each profile has its own `user` folder. Use the Suite’s **Save** tab to copy saves so the profile’s file list is updated as well.

[Start at Franklin and Lamar or play through North Yankton](FIRST_SESSION.md)
