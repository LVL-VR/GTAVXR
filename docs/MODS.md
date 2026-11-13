# Content mods and scripts

Import mods from the **Mods** screen, then enable them in the profile you plan to play. A separate test profile makes it easier to try combinations of content packs.

## Import and enable mods

1. Download the **GTA V Legacy** edition of each mod. Keep the original archive and folder structure.
2. In the Suite, open **Mods → Add mod** and select the mod file. You can also drop the file or an extracted mod folder onto the **Mods** screen. Review the mod entry, requirements, and compatibility messages.
3. From **Play**, open your profile and select its **Mods** tab. Enable the mod and use **Customize** when the Suite offers options for the pack.
4. Resolve any conflicts listed for the profile. Import required add-ons too, then select **Play** to apply your choices.
5. To return to the base experience, disable the mods in that profile and launch again. Imported mods remain in your library for other profiles.

## Mod versions and compatibility

The in-game results below come from GTAVXR prerelease builds on Steam Legacy with Virtual Desktop and VDXR. See [Compatibility](COMPATIBILITY.md) for the GTAVXR 1.0 reports from Epic and Rockstar Games Launcher.

| Mod | Exact version | How to add and enable it in the Suite | What was verified |
| --- | --- | --- | --- |
| **Make Visuals Great Again (MVGA)** | **2.65** | Use **Mods → Add mod** and select the original MVGA ZIP. In **Play**, open your profile's **Mods** tab and enable **Make Visuals Great Again**. | **Played in VR.** The GTAVXR 1.0 installer also reproduced the 31 files used by its MVGA profile from the original ZIP in an offline check. |
| **Open World Interiors** | **0.2** for Legacy | Add the original ZIP with its script and bundled content together. Enable **Open World Interiors** in the profile. The Suite starts Script Host and enables **Online maps in Story** for the session. | **Tested in VR.** Interior entry worked. Exiting an interior can leave the view out of place. Use **VR Menu → Recenter** if it happens. |
| **World of Variety (WoV)** | **10.1** by Cass | Add the original download containing the main OIV. Enable **World of Variety** in the profile and use **Customize** to choose its options. | **Played in a combined VR profile** with WoV Plus and V4EVER. Individual options were checked offline. |
| **World of Variety Plus 2026** | **1.5** download. The Suite may display **10.1** from the OIV metadata. | Add the original RAR or its extracted OIV. Import and enable **World of Variety 10.1** in the same profile, then enable **World of Variety Plus**. | **Played in the combined VR profile.** Individual gameplay changes were not checked separately. |
| **V4EVER** | **1.2 Legacy** | Add the Legacy archive or select the `dlc.rpf` inside its `v4evermod` folder. Keep that folder name. Enable **V4EVER** in the profile. | **Played in the combined VR profile.** The standalone profile was prepared offline. |
| **No Umbrellas** | **Bundled with WoV 10.1.** The OIV reports **0.0**. | Add the full WoV archive or separately add its bundled `no umbrellas.oiv`. Enable **World of Variety** in the profile, open **Customize**, and turn **Umbrellas when it rains** off. | **Recognized and prepared offline** as a WoV option. Its effect in the rain remains unverified in-game. |

The Suite prepares content changes when you select **Play**. Some packs modify the same resources. Read any conflict or incompatibility message and try one mod at a time to isolate the cause.

## In-game world settings

Open **VR Menu → Mods**, select **Pedestrians** or **Traffic and parked cars**, and tilt the thumbstick left or right to adjust density. **100%** preserves the profile's density. **Online maps in Story** enables map content such as the aircraft carrier or submarine within Story Mode. **Reset mods** restores both densities to 100% and turns those maps off.

## Script Host and plugins

- GTAVXR Script Host loads compiled scripts that use the ScriptHookVDotNet API supported by the selected version. The Suite inspects imported files and explains why a script cannot load.
- The Suite checks **.asi** plugins that use Script Hook V for compatibility with the Host. A plugin that relies directly on the retail executable, calls an unsupported Host function, or targets Enhanced may require a different version of the mod.
- Import the mod through the Suite so its Script Host can manage the files for each session. Source-only **.cs** or **.vb** scripts that require compilation and the older ScriptHookVDotNet 1 API are outside this workflow.
- **Untested** means the mod has not been confirmed in play. If the Suite flags a script as incompatible, read the reason on its mod entry. Select the appropriate Legacy version of the mod or disable it for that profile.

If a mod imports but fails in-game, record its name, version, and exact error. Launch the same profile with that mod disabled to compare. [Troubleshooting](TROUBLESHOOTING.md#mods-and-graphics)
