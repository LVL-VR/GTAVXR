# Install and maintain GTAVXR 1.0

You need 64-bit Windows, a legal copy of GTA V Legacy, and a compatible headset and OpenXR runtime for VR. **Legacy 1.0.3889.0** is the reference version. The Suite checks the game files it needs. [Compatibility](COMPATIBILITY.md)

1. Download **GTAVXR.exe** from Releases. Right-click the file and select **Run as administrator**. The Suite needs write access to the Legacy folder during installation.
2. Check the game folder on the first screen. If the Suite cannot find it, click **Choose folder** and select the folder that contains **GTA5.exe**. It can be on another drive. Click **Continue**.
3. On the **Play** screen, click **Install GTAVXR** and wait for it to finish. The Suite checks your Legacy files, applies the included patch, and prepares the VR components.
4. Initial setup creates **VR Story** with a copy of your Legacy saves. In the profile editor, open **Save** to copy your normal GTA saves again or select **Start a new story**. [Profiles and saves](SAVES.md)
5. Connect your headset and open **Settings → Headset → Change**. Select **Automatic** to use the active Windows runtime at launch or choose the runtime for your connection. This setting applies to the Suite. [Suite settings](SUITE.md)
6. Return to **Play**, select your profile, and enter Story Mode. If the game opens in flatscreen, focus the GTA window and press **F10**. [VR controls](VR_CONTROLS.md)
7. Save in GTA and select **Force GTA to close** in the Suite. [First session and closing the game](FIRST_SESSION.md)

If a previous installation stopped with **Access is denied**, close the Suite and reopen it as administrator. With GTA closed, click **Fix now** if the recovery notice appears. After the notice clears, click **Install GTAVXR** if the button is still available. [Installation recovery](TROUBLESHOOTING.md#access-denied-during-installation)

## Update, repair, or uninstall

- **Update GTAVXR** by installing the new release. Check its release notes for the required Legacy version. Updates to the Suite itself appear separately under **Settings → Update** when available.
- **Check files** under **Settings** with GTA closed to repair the files the Suite manages. If an error points to a base Legacy file, use the corresponding recovery steps. [Troubleshooting](TROUBLESHOOTING.md)
- **Uninstall** under **Settings → Uninstall** with GTA closed. The Suite removes the files it manages from the game folder and keeps your profiles, saves, and mod library. Uninstall GTAVXR from the current folder before switching to a different GTA V Legacy folder.
