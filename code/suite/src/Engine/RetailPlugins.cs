using System;
using System.IO;
using System.Linq;
using System.Collections.Generic;

namespace GtavxrSuite {
    // Mods the player keeps in the game folder for the store GTA (ASI plugins, ASI loaders, the official ScriptHookV, ReShade-style
    // proxies). GTAVXR's own EXE lives in the same folder, so Windows would load them into it and they do not support it. During a VR
    // session they wait in the session folder and go back when the game closes (also through Recover). They never block playing.
    // Only what GTAVXR cannot run is moved: the store game's own files, GTAVXR's files, mods/ and scripts/ stay where they are
    // (a script host session puts the player's scripts/ aside itself, PlaySession.StageScripts).
    public static class RetailPlugins {
        // Files the store Legacy game ships in its folder: never moved.
        private static readonly string[] Shipped={"bink2w64.dll","d3dcompiler_46.dll","d3dcsx_46.dll","fvad.dll","GFSDK_ShadowLib.win64.dll",
            "GFSDK_TXAA.win64.dll","GFSDK_TXAA_AlphaResolve.win64.dll","GPUPerfAPIDX11-x64.dll","libcurl.dll","libtox.dll","NvPmApi.Core.win64.dll",
            "opus.dll","opusenc.dll","steam_api64.dll","XCurl.dll","zlib1.dll"};
        // Names Windows takes from the game folder first when GTAVXR's EXE (or Direct3D inside it) loads them: the EXE's own imports,
        // the Direct3D / DXGI proxies and the usual ASI-loader names. Plus the official ScriptHookV, which only runs on the store EXE.
        private static readonly string[] LoadedByName={"dinput8.dll","dsound.dll","winmm.dll","version.dll","d3d9.dll","xinput1_3.dll",
            "dbghelp.dll","psapi.dll","imm32.dll","dwmapi.dll","dxgi.dll","d3d10.dll","d3d11.dll","d3d12.dll","dinput.dll","ddraw.dll",
            "wininet.dll","winhttp.dll","msacm32.dll","msvfw32.dll","xinput1_4.dll","xinput9_1_0.dll","ScriptHookV.dll"};

        // What GTAVXR runs itself stays in place. Nothing in the game folder: the GTAVXR script host (work/SCRIPT_HOST_PLAN.md) loads
        // GTAVXRHost.dll beside the bridge and its own ScriptHookVDotNet build from the session's scripthost/ folder.
        public static bool Compatible(string name){return false;}
        // During a script host session the store game's ScriptHookVDotNet API files in the game folder wait aside too: the game folder
        // is where .NET looks first, and the scripts must bind to GTAVXR's ScriptHookVDotNet build.
        static readonly System.Text.RegularExpressions.Regex ShvdnApi=new System.Text.RegularExpressions.Regex(@"^ScriptHookVDotNet\d*\.dll$",System.Text.RegularExpressions.RegexOptions.IgnoreCase);

        // Top-level files of the game folder that would end up inside GTAVXR's EXE and are not GTAVXR's own.
        public static List<string> Incompatible(string gameDir,ICollection<string> owned,bool scriptHost=false){
            string root=SafeFs.Root(gameDir);var mine=new HashSet<string>(owned??new string[0],StringComparer.OrdinalIgnoreCase);
            var list=new List<string>();
            foreach(string file in Directory.GetFiles(root,"*",SearchOption.TopDirectoryOnly)){
                string name=Path.GetFileName(file);
                if(mine.Contains(name)||Shipped.Contains(name,StringComparer.OrdinalIgnoreCase)||Compatible(name))continue;
                if(Path.GetExtension(name).Equals(".asi",StringComparison.OrdinalIgnoreCase)||LoadedByName.Contains(name,StringComparer.OrdinalIgnoreCase)||(scriptHost&&ShvdnApi.IsMatch(name)))list.Add(name);
            }
            list.Sort(StringComparer.OrdinalIgnoreCase);return list;
        }
    }
}
