using System;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Diagnostics;
using System.Globalization;
using System.Collections.Generic;
using Microsoft.Win32;

namespace GtavxrSuite {
    public sealed class OfxrState {
        public bool layer {get;set;}             // the OFXR implicit OpenXR layer is registered and enabled for this user
        public string manifest {get;set;}        // its layer manifest
        public string profile {get;set;}         // the one GTAVXR profile in %LOCALAPPDATA%\ofxr-bridge\Profiles, null = none or ambiguous
        public bool profile_ambiguous {get;set;} // several files could be the GTAVXR profile: the Suite changes none of them
        // null (no layer) | detected (layer, no GTAVXR game running) | insufficient (a GTAVXR game runs, but OFXR's
        // status.json does not describe it: see reason) | active (status.json describes this game and says generating=false)
        // | unconfirmed (it describes this game but "generating" is null, absent or not a boolean: neither generation nor its
        // absence is confirmed) | paused | generating (it says generating=true for this game)
        public string status {get;set;}
        public string reason {get;set;}          // why the status is insufficient
        public bool? generating {get;set;}       // OFXR's own "generating" boolean for this game; null = not reported
        // OFXR's own measurements for this game. null = not measured / not reported: never shown as 0.
        public double? app_fps {get;set;}
        public double? output_fps {get;set;}
        public double? multiplier {get;set;}
        public double? latency_ms {get;set;}
        public double? display_hz {get;set;}
        public string runtime {get;set;}         // SteamVR | VDXR | Other, as OFXR reports it
        public string updated {get;set;}         // status.json write time (UTC) that was accepted
        public string tray {get;set;}            // OFXRBridgeTray.exe when known (running process or the player's choice)
    }

    // 2.4.0 (167-neural B4): OFXR is the player's own tool, switched on from its tray before putting the headset on. The Suite
    // never installs, enables, removes or drives it: it detects its layer, shows its status for the running GTAVXR game, opens
    // its tray and, on request, recommends one setting in the one profile that is unambiguously GTAVXR's.
    // status.json (OFXR schema 1): %LOCALAPPDATA%\ofxr-bridge\status.json, written about once a second while
    // "Export status.json" is on; "updated" is ISO 8601 UTC; "session" is null without a game; "session.pid" names the game.
    public static class Ofxr {
        public const string GameExe="GTAVXR_SourceBaseline";
        public const double FreshSeconds=5, FutureSeconds=2;
        internal static readonly string[] States={"detected","insufficient","active","unconfirmed","paused","generating"};
        internal static readonly string[] Reasons={"game_not_running","no_status","status_too_large","status_unreadable","schema",
            "updated_missing","updated_future","updated_before_game","updated_stale","no_session","other_process","other_pid"};
        internal static readonly string[] Errors={"OFXR_NO_PROFILE","OFXR_NO_KEY","OFXR_UNKNOWN_FORMAT","OFXR_AMBIGUOUS_PROFILE","OFXR_NO_BACKUP",
            "OFXR_IN_USE","OFXR_CHANGED","OFXR_CONFLICT"};
        public static string Root(){return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"ofxr-bridge");}

        // The profile OFXR keeps for the GTAVXR executable. OFXR names one file per executable; the Suite accepts only the
        // exact names GTAVXR_SourceBaseline.ini / GTAVXR_SourceBaseline.exe.ini. Both present, or only other files that a
        // wildcard would match (GTAVXR_SourceBaseline*.ini), is ambiguous: no profile is chosen.
        internal static string Profile(string root,out bool ambiguous){
            ambiguous=false;
            string dir=root==null?null:Path.Combine(root,"Profiles");
            if(dir==null||!Directory.Exists(dir))return null;
            var all=Directory.GetFiles(dir,GameExe+"*.ini");
            var exact=all.Where(path=>{string name=Path.GetFileName(path);
                return name.Equals(GameExe+".ini",StringComparison.OrdinalIgnoreCase)||name.Equals(GameExe+".exe.ini",StringComparison.OrdinalIgnoreCase);}).ToArray();
            if(exact.Length==1)return exact[0];
            ambiguous=all.Length!=0;
            return null;
        }

        // true = OFXR's own neural rendering is on for GTAVXR, false = it is off or there is no GTAVXR profile and the
        // shared starting configuration does not turn it on, null = indeterminate (ambiguous or unreadable profile).
        internal static bool? NeuralEnabled(string root,Func<string> layer) {
            if(root==null||layer==null||layer()==null) return false;
            bool ambiguous;string profile=Profile(root,out ambiguous);
            if(ambiguous) return null;
            if(profile==null){
                // A game without a profile gets one copied from ofxr-bridge.ini the first time it runs.
                string shared=Path.Combine(root,"ofxr-bridge.ini");
                return File.Exists(shared)&&Key(shared)==true?(bool?)null:false;
            }
            return Key(profile);
        }
        // The nr_enabled value of an INI: true/false, null when absent twice, unreadable or not a known boolean.
        static bool? Key(string path){
            try{
                if(new FileInfo(path).Length>1024*1024) return null;
                bool? value=null;int seen=0;
                foreach(string line in File.ReadLines(path)){
                    int eq=line.IndexOf('=');
                    if(eq<=0||!line.Substring(0,eq).Trim().Equals("nr_enabled",StringComparison.OrdinalIgnoreCase)) continue;
                    string v=line.Substring(eq+1).Trim().ToLowerInvariant();
                    ++seen;
                    value=v=="1"||v=="true"||v=="on"||v=="yes"?true:v=="0"||v=="false"||v=="off"||v=="no"?(bool?)false:null;
                    if(value==null) return null;
                }
                return seen==0?false:seen==1?value:null;
            }catch(IOException){return null;}catch(UnauthorizedAccessException){return null;}
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

        static bool SameAlive(ChildIdentity game){try{return ProcessOwnership.IsSameProcessAlive(game);}catch(Exception){return false;}}

        // game: the GTAVXR process the Suite started (pid, creation time, executable), null when no game runs. status.json is
        // credited to it only when it is schema 1, written after that process started, recent and not in the future, and its
        // session names that exact executable file name and pid while that same process (same creation time) still runs.
        public static OfxrState Read(string root,string layerManifest,string chosenTray,ChildIdentity game=null,
                                     Func<ChildIdentity,bool> alive=null,DateTime? nowUtc=null){
            var s=new OfxrState{manifest=layerManifest,layer=layerManifest!=null};
            bool ambiguous;s.profile=Profile(root,out ambiguous);s.profile_ambiguous=ambiguous;
            s.tray=TrayPath(chosenTray);
            if(!s.layer){s.status=null;return s;}
            if(game==null){s.status="detected";return s;}
            s.status="insufficient";
            s.reason=Attribute(root,game,alive??SameAlive,nowUtc??DateTime.UtcNow,s);
            return s;
        }

        static string Attribute(string root,ChildIdentity game,Func<ChildIdentity,bool> alive,DateTime now,OfxrState s){
            if(!alive(game))return "game_not_running";
            string js=Path.Combine(root,"status.json");
            Dictionary<string,object> j;
            try{
                if(!File.Exists(js))return "no_status";      // the tray is closed or "Export status.json" is off
                if(new FileInfo(js).Length>1024*1024)return "status_too_large";
                j=new System.Web.Script.Serialization.JavaScriptSerializer().Deserialize<Dictionary<string,object>>(File.ReadAllText(js));
            }catch(Exception){return "status_unreadable";}   // also a file being replaced: read again a second later
            if(j==null||!(j.ContainsKey("schema")&&Num(j,"schema")==1))return "schema";
            object raw;DateTime updated;
            if(!j.TryGetValue("updated",out raw)||!(raw is string)||!((string)raw).EndsWith("Z",StringComparison.Ordinal)||
               !DateTime.TryParse((string)raw,CultureInfo.InvariantCulture,DateTimeStyles.AdjustToUniversal|DateTimeStyles.AssumeUniversal,out updated))
                return "updated_missing";
            if(updated>now.AddSeconds(FutureSeconds))return "updated_future";
            if(updated.Ticks<game.creation_utc_ticks)return "updated_before_game";
            if((now-updated).TotalSeconds>FreshSeconds)return "updated_stale";
            object ses;
            if(!j.TryGetValue("session",out ses)||ses==null)return "no_session";
            var session=ses as Dictionary<string,object>;if(session==null)return "no_session";
            object proc;string exe=Path.GetFileName(game.executable??"");
            if(!session.TryGetValue("process",out proc)||!(proc is string)||!((string)proc).Equals(exe,StringComparison.OrdinalIgnoreCase))return "other_process";
            if(Num(session,"pid")!=game.pid)return "other_pid";
            s.updated=(string)raw;
            s.generating=Bool(j,"generating");
            s.status=Bool(j,"paused")==true?"paused":s.generating==true?"generating":s.generating==false?"active":"unconfirmed";
            s.app_fps=Num(j,"app_fps");s.output_fps=Num(j,"output_fps");s.multiplier=Num(j,"multiplier");
            s.latency_ms=Num(j,"latency_ms");s.display_hz=Num(j,"display_hz");
            object rt;if(session.TryGetValue("runtime",out rt))s.runtime=rt as string;
            return null;
        }
        static bool? Bool(Dictionary<string,object> j,string k){object v;return j.TryGetValue(k,out v)&&v is bool?(bool?)(bool)v:null;}
        // A number, or null when absent, null or not a number: an unmeasured value is never 0.
        static double? Num(Dictionary<string,object> j,string k){
            object v;if(!j.TryGetValue(k,out v)||v==null||v is string||v is bool)return null;
            try{double d=Convert.ToDouble(v,CultureInfo.InvariantCulture);return Double.IsNaN(d)||Double.IsInfinity(d)?(double?)null:d;}catch(Exception){return null;}
        }

        // The tray the player runs: the running OFXRBridgeTray process, else the file the player chose once in the Suite.
        public static string TrayPath(string chosen){
            foreach(var p in Process.GetProcessesByName("OFXRBridgeTray")){
                using(p){try{var f=p.MainModule==null?null:p.MainModule.FileName;if(f!=null&&File.Exists(f))return f;}catch(Exception){}}
            }
            return !String.IsNullOrEmpty(chosen)&&File.Exists(chosen)&&Path.GetFileName(chosen).Equals("OFXRBridgeTray.exe",StringComparison.OrdinalIgnoreCase)?chosen:null;
        }

        public const string Backup=".gtavxr-before";

        // "Recommended profile": in the one GTAVXR profile OFXR created itself, NR of OFXR off (GTAVXR's chain already runs NR
        // per eye; two NR passes are never wanted). Only one existing nr_enabled key is changed, written in the form it has
        // (0/1, true/false, on/off, yes/no). An exact copy of the profile is kept beside it first (recovery and the record of
        // the original value); the new profile replaces the old one in one step. No other file, key or profile is touched.
        // Returns null on success, else the reason; on any error the profile is as it was.
        public static string Recommend(string root){
            bool ambiguous;string profile=Profile(root,out ambiguous);
            if(ambiguous)return "OFXR_AMBIGUOUS_PROFILE";
            return RecommendFile(profile);
        }
        internal static string RecommendFile(string profile){
            if(profile==null||!File.Exists(profile))return "OFXR_NO_PROFILE";
            string keep=profile+Backup;
            bool made=false;
            try{
                using(var current=OpenProfile(profile)){
                    var doc=new IniProfile(current);
                    int offset;string value;
                    string error=doc.FindValue(out offset,out value);
                    if(error!=null)return error;
                    string off=Off(value);
                    if(off==null)return "OFXR_UNKNOWN_FORMAT";
                    if(File.Exists(keep)){
                        using(var backup=OpenProfile(keep)){
                            string prior=OriginalNr(backup);
                            if(prior==null)return "OFXR_BACKUP_EXISTS";
                            // Match the original backup's spelling so Undo recognizes the recommendation.
                            string expectedOff=Off(prior);
                            if(!String.Equals(value,expectedOff,StringComparison.Ordinal))
                                ReplaceAtomic(profile,doc.Bytes,doc.WithValue(offset,value,expectedOff));
                        }
                    }else if(!String.Equals(value,off,StringComparison.OrdinalIgnoreCase)){
                        using(var backup=new FileStream(keep,FileMode.CreateNew,FileAccess.Write,FileShare.None)){
                            made=true;
                            backup.Write(doc.Bytes,0,doc.Bytes.Length);
                            backup.Flush(true);
                        }
                        ReplaceAtomic(profile,doc.Bytes,doc.WithValue(offset,value,off));
                    }
                    return null;
                }
            }catch(DecoderFallbackException){return "OFXR_UNKNOWN_FORMAT";}
            catch(IOException){if(made)DeleteTemporary(keep);return "OFXR_IN_USE";}
            catch(UnauthorizedAccessException){if(made)DeleteTemporary(keep);return "OFXR_IN_USE";}
        }

        public static string Undo(string root){
            bool ambiguous;string profile=Profile(root,out ambiguous);
            if(ambiguous)return "OFXR_AMBIGUOUS_PROFILE";
            if(profile==null||!File.Exists(profile+Backup))return "OFXR_NO_BACKUP";
            return UndoFile(profile);
        }
        internal static string UndoFile(string profile){
            string keep=profile+Backup;
            if(!File.Exists(keep))return "OFXR_NO_BACKUP";
            try{
                using(var original=OpenProfile(keep))
                using(var current=OpenProfile(profile)){
                    string prior=OriginalNr(original);
                    if(prior==null)return "OFXR_CONFLICT";
                    var now=new IniProfile(current);
                    int offset;
                    string value;
                    if(now.FindValue(out offset,out value)!=null||value!=Off(prior))return "OFXR_CHANGED";
                    ReplaceAtomic(profile,now.Bytes,now.WithValue(offset,value,prior));
                    DeleteTemporary(keep);
                    return null;
                }
            }catch(DecoderFallbackException){return "OFXR_UNKNOWN_FORMAT";}
            catch(IOException){return "OFXR_IN_USE";}
            catch(UnauthorizedAccessException){return "OFXR_IN_USE";}
        }

        static string OriginalNr(FileStream backup){
            try{
                int offset;
                string value;
                if(new IniProfile(backup).FindValue(out offset,out value)!=null)return null;
                string off=Off(value);
                return off!=null&&!String.Equals(off,value,StringComparison.OrdinalIgnoreCase)?value:null;
            }catch(DecoderFallbackException){return null;}
        }

        static FileStream OpenProfile(string path){
            return new FileStream(path,FileMode.Open,FileAccess.Read,FileShare.Read|FileShare.Delete);
        }

        sealed class IniProfile{
            static readonly Encoding[] Encodings={new UTF32Encoding(false,true,true),new UTF32Encoding(true,true,true),
                new UnicodeEncoding(false,true,true),new UnicodeEncoding(true,true,true),new UTF8Encoding(true,true)};
            static readonly Regex Key=new Regex(@"(?:^|(?<=[\r\n]))[^\S\r\n]*nr_enabled[^\S\r\n]*=(?<raw>[^\r\n]*)",
                RegexOptions.IgnoreCase|RegexOptions.CultureInvariant);
            readonly Encoding encoding;
            readonly byte[] bom;
            readonly string text;
            internal readonly byte[] Bytes;

            internal IniProfile(FileStream stream){
                if(stream.Length>1024*1024)throw new DecoderFallbackException();
                using(var buffer=new MemoryStream()){
                    stream.CopyTo(buffer);
                    Bytes=buffer.ToArray();
                }
                encoding=Encodings.FirstOrDefault(kind=>Bytes.Take(kind.GetPreamble().Length).SequenceEqual(kind.GetPreamble()))
                    ??new UTF8Encoding(false,true);
                bom=encoding.GetPreamble();
                text=encoding.GetString(Bytes,bom.Length,Bytes.Length-bom.Length);
            }

            internal string FindValue(out int offset,out string value){
                offset=0;value=null;
                var lines=Key.Matches(text);
                if(lines.Count==0)return "OFXR_NO_KEY";
                if(lines.Count!=1)return "OFXR_UNKNOWN_FORMAT";
                var raw=lines[0].Groups["raw"];
                value=raw.Value.Trim();
                offset=raw.Index+raw.Value.Length-raw.Value.TrimStart().Length;
                return null;
            }
            internal byte[] WithValue(int offset,string value,string replacement){
                return bom.Concat(encoding.GetBytes(text.Substring(0,offset)+replacement+
                    text.Substring(offset+value.Length))).ToArray();
            }
        }

        // Hold the read handle until replacement to exclude in-place edits. Check for a path replacement by OFXR as well.
        static void ReplaceAtomic(string target,byte[] expected,byte[] bytes){
            string temp=target+".gtavxr-"+Ids.New()+".tmp";
            try{
                using(var output=new FileStream(temp,FileMode.CreateNew,FileAccess.Write,FileShare.None)){
                    output.Write(bytes,0,bytes.Length);
                    output.Flush(true);
                }
                if(!File.ReadAllBytes(target).SequenceEqual(expected))throw new IOException("OFXR profile changed during edit");
                File.Replace(temp,target,null);
            }finally{DeleteTemporary(temp);}
        }
        static void DeleteTemporary(string path){
            try{File.Delete(path);}catch(IOException){}catch(UnauthorizedAccessException){}
        }

        public static bool CanUndo(string root){bool ambiguous;string profile=Profile(root,out ambiguous);return profile!=null&&File.Exists(profile+Backup);}
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
