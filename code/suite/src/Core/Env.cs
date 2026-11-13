using System;
using System.IO;
using System.Linq;
using System.Collections.Generic;
using System.Text.RegularExpressions;
using Microsoft.Win32;

namespace GtavxrSuite {
    // Everything the Suite remembers between runs. Small, human-readable, written atomically.
    public class SuiteSettings {
        public string schema {get;set;}
        public string language {get;set;}          // "es" | "en"
        public string game_dir {get;set;}
        public string user_data_dir {get;set;}     // Documents\Rockstar Games\GTA V
        public string openxr_manifest {get;set;}   // before 2.4.0: the manifest found at first run (now only "auto" or openxr_runtime)
        public string openxr_runtime {get;set;}    // 2.4.0: "auto" (Windows' active runtime at every Play) or a chosen manifest
        public string dlss_dir {get;set;}          // 2.4.0: the DLSS folder the player chose (null = the Suite looks for one)
        public string ofxr_tray {get;set;}         // 2.4.0: OFXRBridgeTray.exe the player pointed at (null = only a running tray)
        public bool dev_mode {get;set;}
        public string dev_repo {get;set;}          // development host only (GTAVXR repository)
        public string selected_profile {get;set;}
        public bool first_run_done {get;set;}
    }

    // The machine the Suite runs on: where its data lives and where the game, saves and headset runtime are.
    // Tests build an Env over owned fixture folders; the product builds it from detection.
    public sealed class Env {
        public string DataRoot {get;private set;}
        public string GameDir {get;set;}
        public string UserDataDir {get;set;}
        public string OpenXrManifest {get;set;}
        public string DevRepo {get;set;}
        // Legacy native paths compiled into versions 125-155 (the repository on this PC).
        string legacySessionsRoot,legacySettingsPath;
        public string LegacySessionsRoot {get{return legacySessionsRoot??Path.Combine(DevRepo??DefaultRepo,"local-cache","play","sessions");}set{legacySessionsRoot=value;}}
        public string LegacySettingsPath {get{return legacySettingsPath??Path.Combine(DevRepo??DefaultRepo,"local-cache","play","vr-settings.ini");}set{legacySettingsPath=value;}}
        public bool Fixture {get;private set;}
        public string[] GameProcessNames {get;set;}
        public IDictionary<string,string> RetailOverride {get;set;}  // fixture-only retail hashes
        // Rockstar launcher handshake folder (game/core/app.cpp WriteExitFile: in-<pid> / out-<pid>) and how long a game that
        // already wrote out-<pid> (saves finished, ExitProcess next) may stay alive before the Suite ends it.
        public string ExitMarkerDir {get;set;}
        public int ExitGraceMs {get;set;}
        // 2.4.0: what this PC has, read when needed (the product reads Windows; fixtures give owned folders)
        public Func<string> XrActive {get;set;}                 // Windows' ActiveRuntime manifest
        public Func<List<string>> XrAvailable {get;set;}        // AvailableRuntimes (enabled)
        public Func<List<string>> DlssSearchRoots {get;set;}    // where a player keeps DLSS files
        public string OfxrRoot {get;set;}                       // %LOCALAPPDATA%\ofxr-bridge
        public Func<string> OfxrLayer {get;set;}                // the enabled OFXR implicit layer manifest, null = none

        public static string DefaultRepo {get{return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments),"GTAVXR");}}

        private Env(){}
        public static string DefaultDataRoot(){return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"GTAVXR","Suite");}
        public static Env Product(string dataRoot) {
            var e=new Env{DataRoot=SafeFs.Root(dataRoot)};Directory.CreateDirectory(e.DataRoot);
            var s=e.LoadSettings();
            e.GameDir=s.game_dir;e.UserDataDir=s.user_data_dir??Detect.UserData();e.OpenXrManifest=s.openxr_manifest;e.DevRepo=s.dev_repo;
            e.GameProcessNames=new[]{"GTA5","GTA5_BE","PlayGTAV","GTAVXR_SourceBaseline","game_win64_bankrelease","game_win64_release"};
            e.ExitMarkerDir=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"Rockstar Games","GTA V");e.ExitGraceMs=15000;
            e.XrActive=XrRuntimes.RegistryActive;e.XrAvailable=XrRuntimes.RegistryAvailable;e.DlssSearchRoots=Dlss.DefaultSearchRoots;
            e.OfxrRoot=Ofxr.Root();e.OfxrLayer=Ofxr.LayerManifest;
            return e;
        }
        // Owned fixture world for headless tests: nothing outside 'fresh' is touched.
        public static Env ForFixture(string fresh) {
            fresh=SafeFs.Root(fresh);
            var e=new Env{DataRoot=Path.Combine(fresh,"suite-data"),Fixture=true};
            e.GameDir=Path.Combine(fresh,"game");e.UserDataDir=Path.Combine(fresh,"documents","Rockstar Games","GTA V");
            e.OpenXrManifest=Path.Combine(fresh,"openxr","runtime.json");e.DevRepo=Path.Combine(fresh,"repo");
            e.LegacySessionsRoot=Path.Combine(fresh,"repo","local-cache","play","sessions");
            e.LegacySettingsPath=Path.Combine(fresh,"repo","local-cache","play","vr-settings.ini");
            e.GameProcessNames=new[]{"GTAVXR_FixtureGame_NeverRunning"};
            e.ExitMarkerDir=Path.Combine(fresh,"localappdata","Rockstar Games","GTA V");e.ExitGraceMs=1500;Directory.CreateDirectory(e.ExitMarkerDir);
            foreach(string d in new[]{e.DataRoot,e.GameDir,e.UserDataDir,Path.GetDirectoryName(e.OpenXrManifest),e.LegacySessionsRoot})Directory.CreateDirectory(d);
            // the fixture runtime is the active one; no other runtime, DLSS folder or OFXR layer unless a test adds them
            e.XrActive=()=>e.OpenXrManifest;e.XrAvailable=()=>new List<string>();e.DlssSearchRoots=()=>new List<string>();
            e.OfxrRoot=Path.Combine(fresh,"localappdata","ofxr-bridge");e.OfxrLayer=()=>null;
            return e;
        }

        public string SettingsFile{get{return SafeFs.Under(DataRoot,"settings.json");}}
        public SuiteSettings LoadSettings() {
            // English by default (user 2026-10-22); a language the player already chose is kept.
            if(!File.Exists(SettingsFile))return new SuiteSettings{schema="gtavxr-suite2-settings-v1",language="en",openxr_runtime=XrRuntimes.Auto};
            var s=Json.Read<SuiteSettings>(SettingsFile);
            if(s.schema!="gtavxr-suite2-settings-v1")throw new InvalidDataException("Unknown settings file");
            if(s.language!="en"&&s.language!="es")s.language="en";
            // 2.4.0: the manifest saved at first run becomes "Automatico" (on that PC the active runtime is the same one)
            if(String.IsNullOrEmpty(s.openxr_runtime))s.openxr_runtime=XrRuntimes.Auto;
            return s;
        }
        public void SaveSettings(SuiteSettings s){s.schema="gtavxr-suite2-settings-v1";Json.Atomic(SettingsFile,s);}
        public string Dir(string relative){string p=SafeFs.Under(DataRoot,relative);Directory.CreateDirectory(p);return p;}
        public bool DevHost{get{return DevRepo!=null&&File.Exists(Path.Combine(DevRepo,"work","AGENT_LEASE.json"));}}
    }

    // Finds the game, the saves folder, the headset runtime and the development repository without asking.
    public static class Detect {
        public static string UserData(){
            return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments),"Rockstar Games","GTA V");
        }
        public static List<string> SteamLibraries(){
            var libs=new List<string>();
            string steam=null;
            foreach(var hive in new[]{Registry.CurrentUser,Registry.LocalMachine}){
                foreach(string key in new[]{@"Software\Valve\Steam",@"SOFTWARE\WOW6432Node\Valve\Steam",@"SOFTWARE\Valve\Steam"}){
                    try{using(var k=hive.OpenSubKey(key)){if(k==null)continue;var v=(k.GetValue("SteamPath")??k.GetValue("InstallPath")) as string;if(!String.IsNullOrEmpty(v)&&steam==null)steam=v.Replace('/','\\');}}catch(Exception){}
                }
            }
            if(steam==null){string pf=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86),"Steam");if(Directory.Exists(pf))steam=pf;}
            if(steam==null)return libs;
            libs.Add(steam);
            string vdf=Path.Combine(steam,"steamapps","libraryfolders.vdf");
            try{
                if(File.Exists(vdf)&&new FileInfo(vdf).Length<1024*1024)
                    foreach(Match m in Regex.Matches(File.ReadAllText(vdf),"\"path\"\\s*\"([^\"]+)\""))libs.Add(m.Groups[1].Value.Replace("\\\\","\\"));
            }catch(Exception){}
            return libs.Distinct(StringComparer.OrdinalIgnoreCase).ToList();
        }
        public static List<string> GameCandidates(){
            var found=new List<string>();
            foreach(string lib in SteamLibraries())found.Add(Path.Combine(lib,"steamapps","common","Grand Theft Auto V"));
            foreach(string key in new[]{@"SOFTWARE\WOW6432Node\Rockstar Games\Grand Theft Auto V",@"SOFTWARE\Rockstar Games\Grand Theft Auto V"}){
                try{using(var k=Registry.LocalMachine.OpenSubKey(key)){if(k!=null){var v=k.GetValue("InstallFolder") as string;if(!String.IsNullOrEmpty(v))found.Add(v);}}}catch(Exception){}
            }
            found.Add(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles),"Rockstar Games","Grand Theft Auto V"));
            return found.Where(p=>File.Exists(Path.Combine(p,"GTA5.exe"))).Select(p=>Path.GetFullPath(p).TrimEnd('\\')).Distinct(StringComparer.OrdinalIgnoreCase).ToList();
        }
        public static string OpenXrRuntime(){
            try{using(var k=Registry.LocalMachine.OpenSubKey(@"SOFTWARE\Khronos\OpenXR\1")){if(k!=null){var v=k.GetValue("ActiveRuntime") as string;if(!String.IsNullOrEmpty(v)&&File.Exists(v))return v;}}}catch(Exception){}
            string vd=@"C:\Program Files\Virtual Desktop Streamer\OpenXR\virtualdesktop-openxr.json";
            return File.Exists(vd)?vd:null;
        }
        public static string RuntimeName(string manifest){
            if(manifest==null)return null;
            string n=manifest.ToLowerInvariant();
            if(n.Contains("virtual desktop")||n.Contains("virtualdesktop"))return "Virtual Desktop";
            if(n.Contains("oculus"))return "Meta Quest Link";
            if(n.Contains("steamvr"))return "SteamVR";
            return Path.GetFileNameWithoutExtension(manifest);
        }
        public static bool HeadsetStreamerRunning(){
            foreach(string name in new[]{"VirtualDesktop.Streamer","VirtualDesktop.Service"}){
                var p=System.Diagnostics.Process.GetProcessesByName(name);bool any=p.Length>0;foreach(var x in p)x.Dispose();if(any)return true;
            }
            return false;
        }
        public static string DevRepo(){
            string p=Env.DefaultRepo;
            return File.Exists(Path.Combine(p,"work","AGENT_LEASE.json"))?p:null;
        }
    }
}
