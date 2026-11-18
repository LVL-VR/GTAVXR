# Content mods and scripts

Import mods from the **Mods** screen, then enable them in the profile you plan to play. A separate test profile makes it easier to try combinations of content packs.

## Import and enable mods

1. Download the **GTA V Legacy** edition of each mod. Keep the original archive and folder structure.
2. Select **Mods** in the Suite’s top navigation, next to **Play** and **Settings**. In **Mod Library**, select **Add mod** and choose the mod file. You can also drop the file or an extracted mod folder onto the **Mods** screen. Review the mod entry, requirements, and compatibility messages.
3. From **Play**, open your profile and select its **Mods** tab. Enable the mod and use **Customize** when the Suite offers options for the pack.
4. Resolve any conflicts listed for the profile. Import required add-ons too, then select **Play** to apply your choices.
5. To return to the base experience, disable the mods in that profile and launch again. Imported mods remain in your library for other profiles.

### Existing OpenIV mods and split downloads

The Suite applies mods selected in the active profile. To use content you previously installed through OpenIV, import its original supported package into the Suite and enable it in that profile. You can import compatible `.oiv` packages directly. **For one archive split into multiple volumes**, put every volume in the same folder and extract the first one with an archive tool that supports the format. The tool reads the other volumes automatically. Add the extracted `.oiv` or supported mod folder through **Mods → Add mod**, then enable it in your profile. **For separate `.oiv` installers**, add each required installer as its own package. Follow the mod author's instructions for required components and install order. Enable the required entries in the profile you intend to play.

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

On a mod card, **Works with** or **Available for** lists the Suite versions eligible to use the package. The Suite shows whether the mod was passed from a test profile to a regular profile. See the table above for the exact versions verified in-game.

The Suite prepares content changes when you select **Play**. Some packs modify the same resources. Read any conflict or incompatibility message and try one mod at a time to isolate the cause.

### Sound mods

A sound pack can import successfully and still have no effect in game. Check that the pack is enabled in the profile you launched. If it still has no effect, share the exact mod name and version or download link, any message shown by the Suite, and which sound you expected to change. Use the compatibility table above for packages with recorded results.

## In-game world settings

Open **VR Menu → Mods**, select **Pedestrians** or **Traffic and parked cars**, and tilt the thumbstick left or right to adjust density. **100%** preserves the profile's density. **Online maps in Story** enables map content such as the aircraft carrier or submarine within Story Mode. **Reset mods** restores both densities to 100% and turns those maps off.

## Script Host and plugins

- GTAVXR Script Host loads compiled scripts that use the ScriptHookVDotNet API supported by the selected version. The Suite inspects imported files and explains why a script cannot load.
- Use the integrated Host for supported Script Hook V and ScriptHookVDotNet mods. Import their original packages through the Suite. The Suite checks **.asi** plugins that use Script Hook V for compatibility with the Host. A plugin that relies directly on the retail executable, calls an unsupported Host function, or targets Enhanced may require a different version of the mod.
- Import the mod through the Suite so its Script Host can manage the files for each session. Source-only **.cs** or **.vb** scripts that require compilation and the older ScriptHookVDotNet 1 API are outside this workflow.
- If the Suite flags a script as incompatible, read the reason on its mod entry and select a suitable Legacy version or disable it for that profile. Compatibility with a library such as NativeUI depends on the specific mod, its dependencies and the APIs it calls.

If a mod imports but fails in-game, record its name, version, and exact error. Launch the same profile with that mod disabled to compare. [Troubleshooting](TROUBLESHOOTING.md#mods-and-graphics)
