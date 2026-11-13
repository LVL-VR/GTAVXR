using System;
using System.IO;
using System.Linq;
using System.Text;
using System.Diagnostics;
using System.Collections.Generic;
using Microsoft.Win32;

namespace GtavxrSuite {
    public sealed class OfxrState {
        public bool layer {get;set;}             // the OFXR implicit OpenXR layer is registered and enabled for this user
        public string manifest {get;set;}        // its layer manifest
        public string profile {get;set;}         // %LOCALAPPDATA%\ofxr-bridge\Profiles\<GTAVXR exe>.ini, null = not created yet
        public string status {get;set;}          // generating | armed | paused | loaded | no-data (status.json of a GTAVXR game) | null
        public double app_fps {get;set;}
        public double output_fps {get;set;}
        public string tray {get;set;}            // OFXRBridgeTray.exe when known (running process or the player's choice)
    }

    // 2.4.0 (167-neural B4): OFXR is the player's own tool, switched on from its tray before putting the headset on. The Suite
    // never installs, enables or drives it: it detects its layer, shows its status (status.json only when it names a GTAVXR
    // game process), opens its tray and, on request, recommends one setting in a profile that already exists.
    public static class Ofxr {
        public const string GameExe="GTAVXR_SourceBaseline";
        public static string Root(){return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"ofxr-bridge");}

        internal static bool NeuralEnabled(string root,Func<string> layer) {
            if(root==null||layer==null||layer()==null) return false;
            string dir=Path.Combine(root,"Profiles");
            if(!Directory.Exists(dir)) return false;
            string profile=Directory.GetFiles(dir,GameExe+"*.ini").OrderBy(path=>path.Length).ThenBy(path=>path,StringComparer.OrdinalIgnoreCase).FirstOrDefault();
            if(profile==null||new FileInfo(profile).Length>1024*1024) return false;
            return File.ReadLines(profile).Any(line=>{
                int eq=line.IndexOf('=');
                return eq>0&&line.Substring(0,eq).Trim().Equals("nr_enabled",StringComparison.OrdinalIgnoreCase)&&
                    new[]{"1","true","on","yes"}.Contains(line.Substring(eq+1).Trim(),StringComparer.OrdinalIgnoreCase);
            });
        }

        // HKCU\SOFTWARE\Khronos\OpenXR\1\ApiLayers\Implicit: value name = layer manifest, DWORD 0 = enabled
        public static string LayerManifest(){
            try{using(var k=Registry.CurrentUser.OpenSubKey(@"SOFTWARE\Khronos\OpenXR\1\ApiLayers\Implicit")){
                if(k==null)return null;
                foreach(string n in k.GetValueNames()){
                    string l=n.ToLowerInvariant();if(!l.Contains("ofxr")&&!l.Contains("xrframebridge"))continue;
                    object v=k.GetValue(n);if(v is int&&(int)v==0&&File.Exists(n))return n;
                }
            }}catch(Exception){}
            return null;
        }
        public static OfxrState Read(string root,string layerManifest,string chosenTray){
            var s=new OfxrState{manifest=layerManifest,layer=layerManifest!=null};
            string prof=Path.Combine(root,"Profiles");
            if(Directory.Exists(prof))s.profile=Directory.GetFiles(prof,GameExe+"*.ini").OrderBy(x=>x.Length).FirstOrDefault();
            s.tray=TrayPath(chosenTray);
            string js=Path.Combine(root,"status.json");
            if(!s.layer){s.status=null;return s;}
            s.status="no-data";
            try{
                if(File.Exists(js)&&new FileInfo(js).Length<1024*1024&&(DateTime.UtcNow-File.GetLastWriteTimeUtc(js)).TotalSeconds<10){
                    var j=new System.Web.Script.Serialization.JavaScriptSerializer().Deserialize<Dictionary<string,object>>(File.ReadAllText(js));
                    object ses;string proc=null;
                    if(j.TryGetValue("session",out ses)&&ses is Dictionary<string,object>){object p;if(((Dictionary<string,object>)ses).TryGetValue("process",out p))proc=p as string;}
                    if(proc!=null&&proc.StartsWith(GameExe,StringComparison.OrdinalIgnoreCase)){
                        s.status=Bool(j,"paused")?"paused":Bool(j,"generating")?"generating":Bool(j,"armed")?"armed":"loaded";
                        s.app_fps=Num(j,"app_fps");s.output_fps=Num(j,"output_fps");
                    }
                }
            }catch(Exception){s.status="no-data";}
            return s;
        }
        static bool Bool(Dictionary<string,object> j,string k){object v;return j.TryGetValue(k,out v)&&v is bool&&(bool)v;}
        static double Num(Dictionary<string,object> j,string k){object v;if(!j.TryGetValue(k,out v)||v==null)return 0;try{return Convert.ToDouble(v,System.Globalization.CultureInfo.InvariantCulture);}catch(Exception){return 0;}}

        // The tray the player runs: the running OFXRBridgeTray process, else the file the player chose once in the Suite.
        public static string TrayPath(string chosen){
            foreach(var p in Process.GetProcessesByName("OFXRBridgeTray")){
                using(p){try{var f=p.MainModule==null?null:p.MainModule.FileName;if(f!=null&&File.Exists(f))return f;}catch(Exception){}}
            }
            return !String.IsNullOrEmpty(chosen)&&File.Exists(chosen)&&Path.GetFileName(chosen).Equals("OFXRBridgeTray.exe",StringComparison.OrdinalIgnoreCase)?chosen:null;
        }

        // "Crear perfil recomendado": in the GTAVXR profile OFXR created itself, NR of OFXR off (GTAVXR's chain already runs NR per
        // eye; two NR passes are never wanted). Only an existing nr_enabled key is changed, written in the same form it has
        // (0/1, true/false, on/off); the file is kept beside it first. Returns null on success, else the reason.
        public static string Recommend(string profile){
            if(profile==null||!File.Exists(profile))return "OFXR_NO_PROFILE";
            string text=File.ReadAllText(profile);
            var lines=text.Replace("\r\n","\n").Split('\n').ToList();
            int at=lines.FindIndex(l=>{var t=l.Trim();return t.StartsWith("nr_enabled",StringComparison.OrdinalIgnoreCase)&&t.Substring("nr_enabled".Length).TrimStart().StartsWith("=");});
            if(at<0)return "OFXR_NO_KEY";
            string line=lines[at];int eq=line.IndexOf('=');string val=line.Substring(eq+1).Trim();
            string off=Off(val);if(off==null)return "OFXR_UNKNOWN_FORMAT";
            if(val==off)return null;
            string keep=profile+".gtavxr-before";if(!File.Exists(keep))File.Copy(profile,keep);
            lines[at]=line.Substring(0,eq+1)+(line.Substring(eq+1).StartsWith(" ")?" ":"")+off;
            string nl=text.Contains("\r\n")?"\r\n":"\n";
            File.WriteAllText(profile,String.Join(nl,lines),new UTF8Encoding(false));
            return null;
        }
        static string Off(string v){
            switch(v.ToLowerInvariant()){
                case "1":case "0":return "0";
                case "true":case "false":return v.Length>0&&Char.IsUpper(v[0])?"False":"false";
                case "on":case "off":return v.Length>0&&Char.IsUpper(v[0])?"Off":"off";
                case "yes":case "no":return v.Length>0&&Char.IsUpper(v[0])?"No":"no";
            }
            return null;
        }
    }
}
