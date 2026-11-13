using System;
using System.IO;
using System.Linq;
using System.Collections.Generic;

namespace GtavxrSuite {
    // ---------- Versions ----------
    public class LaunchContract {
        public string exe {get;set;}                          // relative to the game folder
        public List<string> args {get;set;}                   // "{LOGFILE}" is replaced by the session log path
        public List<string> disable_layers {get;set;}         // implicit OpenXR layers to disable (env var = 1)
        public string session_mode {get;set;}                 // "legacy-repo": native admits only the repo sessions folder; "suite" (2.4.0):
                                                              // the Suite's sessions folder, named by GTAVXR_SESSIONS_ROOT
        public string settings_mode {get;set;}                // "legacy-repo": native reads the repo vr-settings.ini; "suite" (2.4.0): this
                                                              // session's copy of the profile's file, named by GTAVXR_VR_SETTINGS
        public string display {get;set;}                      // "windowed1920x1080_stereo0"
        public string residual {get;set;}                     // "exact-one-thread": known source-build exit residue
    }
    // 167-neural: the EXE/bridge read the player's DLSS folder from dlss_dir_env and the profile's choice from vr-settings [Graphics].
    public class NeuralContract {
        public string dlss_dir_env {get;set;}                 // "GTAVXR_DLSS_DIR"
        public List<string> models {get;set;}                 // nvngx_dlss.dll, nvngx_dlssnr.dll (the player's; never in the package)
        public string settings_section {get;set;}             // "Graphics"
        public List<string> keys {get;set;}                   // upscale, neural, neural_intensity
        public List<string> paths_env {get;set;}              // GTAVXR_LOADER_PATH, GTAVXR_VR_SETTINGS, GTAVXR_SESSIONS_ROOT
    }
    public class ReplaceRef : FileRef { public string original_sha256 {get;set;} public long original_bytes {get;set;} }
    public class VersionManifest {
        public string schema {get;set;}
        public string id {get;set;}             // "155v2"
        public string build {get;set;}          // "155"
        public string title {get;set;}          // "Vehículos"
        public string title_en {get;set;}       // "Vehicles" (null = only one language)
        public string channel {get;set;}        // "stable" | "test"
        public string added_utc {get;set;}
        public List<string> notes {get;set;}
        public List<string> notes_en {get;set;}     // the same notes in English (null = only one language)
        public string source_name {get;set;}    // repository release name
        public string source_sha256 {get;set;}
        public bool embedded_release {get;set;}
        public Dictionary<string,string> retail {get;set;}  // GTA5.exe / update rpf hashes it was built for
        public List<FileRef> files {get;set;}       // into the game folder
        public List<FileRef> runtime {get;set;}     // bridge folder: bridge, trainer, fault collector
        public List<ReplaceRef> replaces {get;set;} // game files swapped in ONLY while playing (older builds); originals return at exit
        public LaunchContract launch {get;set;}
        public string mvga {get;set;}               // "none" | "required" | "switch"
        public string mvga_arg {get;set;}
        public string content {get;set;}            // "profile": the EXE serves gtavxr_content/<id> with -gtavxrContent=<id> (build 166+)
        public string script_host {get;set;}        // "switch": the EXE runs ScriptHookV-SDK mods with -gtavxrScriptHost (runtime scripthost/)
        public string script_host_arg {get;set;}
        public List<string> pso_xml {get;set;}      // PSO roots this EXE also reads as parser XML where GTA itself takes only binary (ContentBuilder.PsoOnlyRoots)
        public FileRef vr_settings_seed {get;set;}
        public NeuralContract neural {get;set;}     // 2.4.0: per-eye DLSS chain (167-neural); null = the version has none
        // 2.4.3: its script host brings the ScriptHookVDotNet 3 API (package 167-scripts v13 on)
        // 2.4.4: its script host loads ScriptHookV .asi mods from the game folder (GTAVXR_ASI_FILES; package 167-scripts v14 on)
        public bool RunsAsiMods(){return script_host=="switch"&&runtime!=null&&runtime.Any(f=>String.Equals(f.path,"scripthost/gtavxr-host-features.txt",StringComparison.OrdinalIgnoreCase));}
        public bool RunsShvdnV3(){return script_host=="switch"&&runtime!=null&&runtime.Any(f=>String.Equals(f.path,"scripthost/ScriptHookVDotNet3.dll",StringComparison.OrdinalIgnoreCase));}
        public string contract_sha256 {get;set;}    // 2.4.1: hash of the suite-launch.json it was imported from; another hash (or none while the stored manifest misses contract fields) re-imports it
        public long Bytes(){return files.Sum(f=>f.bytes)+runtime.Sum(f=>f.bytes)+(replaces==null?0:replaces.Sum(f=>f.bytes));}
        public string Label(){return build+" "+title;}
        public List<string> Notes(string lang){return lang=="en"&&notes_en!=null&&notes_en.Count>0?notes_en:notes;}
        public string Title(string lang){return lang=="en"&&!String.IsNullOrEmpty(title_en)?title_en:title;}
    }

    // ---------- Mods ----------
    // A content mod as the player imported it (kind "content-source"): its files are kept in the store with their paths
    // inside the mod (assembly.xml, content/...). At Play the Suite merges the profile's sources onto the player's own
    // 3889 files and serves the result as one content profile.
    public class ContentSourceInfo {
        public string format {get;set;}         // "oiv" | "pack" (a loose DLC pack: dlcpacks/<pack>/dlc.rpf) | "script" (kind "script": catalog only)
        public string pack {get;set;}
        public string catalog {get;set;}        // known mod: "wov" | "wovplus" | "v4ever" | "wov-no-umbrellas"; null = unknown
        public string option_of {get;set;}      // catalog id of the mod this is an official option of (no card of its own)
        public string author {get;set;}
    }
    public class ModManifest {
        public string schema {get;set;}
        public string id {get;set;}
        public string name {get;set;}
        public string version {get;set;}
        public string category {get;set;}       // graphics | driving | map | other
        public string added_utc {get;set;}
        public bool loadable {get;set;}         // the game can load it today
        public string not_loadable_reason {get;set;}
        public string kind {get;set;}           // "mvga-native" | "content-native" | "content-source" | "script" | "archive"
        public List<FileRef> files {get;set;}   // into the game folder when active
        public List<string> args {get;set;}
        public FileRef archive {get;set;}       // original archive kept for later activation
        public string family {get;set;}         // same mod across versions (update detection)
        public string tested_utc {get;set;}     // the player kept it after a test flight ("Pasar a mi perfil")
        public ContentSourceInfo source {get;set;}  // kind "content-source"
        public string built_key {get;set;}      // content-native made by the Suite at Play from the profile's sources (not a card)
        public string companion {get;set;}      // kind "script": the content-source with its game files (OpenIV folder), active with it
        // kind "script": its files under the game's scripts/ folder (paths relative to it), placed only while a script host session
        // runs (PlaySession.StageScripts), never through the game layer. files stays empty.
        public List<FileRef> script_files {get;set;}
        public string script_api {get;set;}     // "v2" (ScriptHookVDotNet2) | "v3" | "v1" | null (no ScriptHookVDotNet assembly found)
        // 2.4.4: kind "script": a ScriptHookV .asi mod's files for the game folder (paths relative to it): each .asi / .dll copy
        // imports GTAVXRHost.dll instead of ScriptHookV.dll (ShvImports), its own settings / data files beside it as in the store
        // game. Placed only while a script host session runs (PlaySession.StageAsi), never through the game layer.
        public List<FileRef> asi_files {get;set;}
        // 2.4.4: files of this mod that still name ScriptHookV after the rename; 2.4.5: only inside a longer text (a message or a
        // path): whole "ScriptHookV.dll" / "ScriptHookV" texts are renamed too (ShvImports name_strings)
        public List<string> shv_by_name {get;set;}
        public bool HasAsi(){return asi_files!=null&&asi_files.Any(f=>f.path.EndsWith(".asi",StringComparison.OrdinalIgnoreCase));}
        public long Bytes(){return (files==null?0:files.Sum(f=>f.bytes))+(script_files==null?0:script_files.Sum(f=>f.bytes))+(asi_files==null?0:asi_files.Sum(f=>f.bytes))+(archive==null?0:archive.bytes);}
        public bool IsCard(){return built_key==null&&(source==null||source.option_of==null);}
        public bool WorksWith(VersionManifest v){
            if(!loadable||v==null)return false;
            if(kind=="mvga-native")return v.mvga=="switch"||v.mvga=="required";
            if(kind=="content-native"||kind=="content-source")return v.content=="profile";
            // 2.4.3: a ScriptHookVDotNet 3 mod needs a version whose script host brings the v3 API (scripthost/ScriptHookVDotNet3.dll)
            // 2.4.4: an .asi mod needs a version whose script host loads .asi mods from the game folder (package 167-scripts v14 on)
            if(kind=="script")return v.script_host=="switch"&&(script_api!="v3"||v.RunsShvdnV3())&&(!HasAsi()||v.RunsAsiMods());
            return false;
        }
        // 2.4.3: mods imported while the script host loaded only ScriptHookVDotNet 2 were stored "v3 pending"; read them as loadable
        // (WorksWith still asks the version for the v3 API).
        public ModManifest Upgraded(){
            if(kind=="script"&&not_loadable_reason=="SHVDN_V3_PENDING"&&script_api=="v3"){loadable=true;not_loadable_reason=null;}
            return this;
        }
    }

    // ---------- Profiles ----------
    public class SaveFileRef { public string relative {get;set;} public string sha256 {get;set;} public long bytes {get;set;} }
    public class Profile {
        public string schema {get;set;}
        public string id {get;set;}
        public string name {get;set;}
        public int color {get;set;}             // index into the profile band palette
        public string image {get;set;}          // "Imagen de perfil": the gate picture (Assets.GateIds); null = original
        public string version {get;set;}
        public List<string> mods {get;set;}     // load order
        public string created_utc {get;set;}
        public string last_played_utc {get;set;}
        public long played_seconds {get;set;}
        public List<SaveFileRef> user {get;set;}    // the Rockstar user folder of this profile (saves, settings.xml)
        public bool has_vr_settings {get;set;}
        public string origin {get;set;}         // how it was created (import, new, copy)
        public bool test {get;set;}             // test profile: every Play is a test flight (game put back afterwards)
        public string start_utc {get;set;}      // test profile: when its starting point was taken
        public Dictionary<string,string> options {get;set;}  // mod options of this profile: "<catalog>.<option>" -> value (absent = default)
        public string Option(string key,string def){string v;return options!=null&&options.TryGetValue(key,out v)&&v!=null?v:def;}
    }
    // A copy of a profile's saves + VR settings: history entries and the test profile's starting point.
    public class ProfileCopy { public string schema {get;set;} public string tag {get;set;} public string utc {get;set;} public List<SaveFileRef> user {get;set;} public bool has_vr_settings {get;set;} }

    public sealed class Library {
        private readonly Env env;
        public Library(Env e){env=e;}
        private string VersionsDir{get{return env.Dir("versions");}}
        private string ModsDir{get{return env.Dir("mods");}}
        private string ProfilesDir{get{return env.Dir("profiles");}}

        public List<VersionManifest> Versions(){
            var list=new List<VersionManifest>();
            foreach(string f in Directory.GetFiles(VersionsDir,"*.json")){
                try{var v=Json.Read<VersionManifest>(f);if(v.schema=="gtavxr-suite2-version-v1"&&v.id+".json"==Path.GetFileName(f))list.Add(v);}catch(Exception){}
            }
            return list.OrderByDescending(v=>BuildNumber(v.build)).ThenBy(v=>v.id).ToList();
        }
        public VersionManifest Version(string id){
            if(id==null)return null;Ids.Check(id);string p=SafeFs.Under(VersionsDir,id+".json");
            return File.Exists(p)?Json.Read<VersionManifest>(p):null;
        }
        public void SaveVersion(VersionManifest v){v.schema="gtavxr-suite2-version-v1";Ids.Check(v.id);Json.Atomic(SafeFs.Under(VersionsDir,v.id+".json"),v);}
        public static int BuildNumber(string b){int n;return Int32.TryParse(new string((b??"").TakeWhile(Char.IsDigit).ToArray()),out n)?n:0;}

        public List<ModManifest> Mods(){
            var list=new List<ModManifest>();
            foreach(string f in Directory.GetFiles(ModsDir,"*.json")){
                try{var m=Json.Read<ModManifest>(f);if(m.schema=="gtavxr-suite2-mod-v1"&&m.id+".json"==Path.GetFileName(f))list.Add(m.Upgraded());}catch(Exception){}
            }
            return list.OrderBy(m=>m.name,StringComparer.CurrentCultureIgnoreCase).ToList();
        }
        public ModManifest Mod(string id){Ids.Check(id);string p=SafeFs.Under(ModsDir,id+".json");return File.Exists(p)?Json.Read<ModManifest>(p).Upgraded():null;}
        internal string ModIdentity(string id){Ids.Check(id);return SafeFs.Hash(SafeFs.Under(ModsDir,id+".json"));}
        public void SaveMod(ModManifest m){m.schema="gtavxr-suite2-mod-v1";Ids.Check(m.id);Json.Atomic(SafeFs.Under(ModsDir,m.id+".json"),m);}
        // Removing a mod takes it out of every profile first. Blobs stay until Clean.
        public List<string> DeleteMod(string id){
            var changed=new List<string>();
            foreach(var p in Profiles())if(p.mods!=null&&p.mods.Remove(id)){SaveProfile(p);changed.Add(p.name);}
            string f=SafeFs.Under(ModsDir,id+".json");if(File.Exists(f))File.Delete(f);
            return changed;
        }

        public string ProfileDir(string id){Ids.Check(id);return SafeFs.Under(ProfilesDir,id);}
        public List<Profile> Profiles(){
            var list=new List<Profile>();
            foreach(string d in Directory.GetDirectories(ProfilesDir)){
                string f=Path.Combine(d,"profile.json");if(!File.Exists(f))continue;
                try{var p=Json.Read<Profile>(f);if(p.schema=="gtavxr-suite2-profile-v1"&&p.id==Path.GetFileName(d))list.Add(p);}catch(Exception){}
            }
            return list.OrderByDescending(p=>p.last_played_utc??"").ThenBy(p=>p.created_utc).ToList();
        }
        public Profile Profile(string id){if(id==null)return null;string f=SafeFs.Under(ProfileDir(id),"profile.json");return File.Exists(f)?Json.Read<Profile>(f):null;}
        public void SaveProfile(Profile p){
            p.schema="gtavxr-suite2-profile-v1";Ids.Check(p.id);if(p.mods==null)p.mods=new List<string>();if(p.user==null)p.user=new List<SaveFileRef>();
            string d=ProfileDir(p.id);Directory.CreateDirectory(d);Json.Atomic(Path.Combine(d,"profile.json"),p);
        }
        public string UserDir(string id){string d=SafeFs.Under(ProfileDir(id),"user");Directory.CreateDirectory(d);return d;}
        public string VrSettingsFile(string id){return SafeFs.Under(ProfileDir(id),"vr-settings.ini");}
        // What this profile's script mods wrote in the game's scripts folder (settings, saved outfits); back in place next Play.
        // 2.4.4: what the profile's .asi mods wrote in the game folder (settings, saves), placed again at the next session
        public string AsiDir(string id){string d=SafeFs.Under(ProfileDir(id),"asi");Directory.CreateDirectory(d);return d;}
        public string ScriptsDir(string id){string d=SafeFs.Under(ProfileDir(id),"scripts");Directory.CreateDirectory(d);return d;}

        // Copies of the profile (saves + VR settings): history/<tag> (last 5) and start (test profile's starting point).
        public string CopyDir(string id,string where){return SafeFs.Under(ProfileDir(id),where);}
        public void KeepCopy(Profile p,string where,string tag){
            string dir=CopyDir(p.id,where);
            if(Directory.Exists(dir))Directory.Delete(dir,true);
            string tmp=dir+".tmp";if(Directory.Exists(tmp))Directory.Delete(tmp,true);
            Directory.CreateDirectory(tmp);
            var files=p.user??new List<SaveFileRef>();
            UserTree.CopyInto(UserDir(p.id),files,Path.Combine(tmp,"user"));
            bool vr=p.has_vr_settings&&File.Exists(VrSettingsFile(p.id));
            if(vr)File.Copy(VrSettingsFile(p.id),Path.Combine(tmp,"vr-settings.ini"));
            Json.Atomic(Path.Combine(tmp,"copy.json"),new ProfileCopy{schema="gtavxr-suite2-profile-copy-v1",tag=tag,utc=DateTime.UtcNow.ToString("o"),user=files,has_vr_settings=vr});
            Directory.Move(tmp,dir);
        }
        public ProfileCopy ReadCopy(string id,string where){
            string f=SafeFs.Under(CopyDir(id,where),"copy.json");
            if(!File.Exists(f))return null;var c=Json.Read<ProfileCopy>(f);
            return c.schema=="gtavxr-suite2-profile-copy-v1"?c:null;
        }
        // Put a kept copy back as the profile's saves + VR settings. The current state is kept in the history first.
        public void RestoreCopy(string id,string where,string historyTag){
            var c=ReadCopy(id,where);if(c==null)throw new InvalidDataException("COPY_MISSING");
            string src=SafeFs.Under(CopyDir(id,where),"user");UserTree.Verify(src,c.user);
            var p=Profile(id);
            KeepCopy(p,"history/"+historyTag,historyTag);
            UserTree.Mirror(src,c.user,UserDir(id));UserTree.Verify(UserDir(id),c.user);
            p.user=c.user;
            string vr=SafeFs.Under(CopyDir(id,where),"vr-settings.ini");
            if(c.has_vr_settings&&File.Exists(vr)){File.Copy(vr,VrSettingsFile(id),true);p.has_vr_settings=true;}
            else{if(File.Exists(VrSettingsFile(id)))File.Delete(VrSettingsFile(id));p.has_vr_settings=false;}
            SaveProfile(p);PruneHistory(id,6);
        }
        public void PruneHistory(string id,int keep){
            string h=SafeFs.Under(ProfileDir(id),"history");if(!Directory.Exists(h))return;
            foreach(string d in Directory.GetDirectories(h).OrderByDescending(x=>x,StringComparer.Ordinal).Skip(keep))Directory.Delete(d,true);
        }
        // Test profile: take (or retake) the starting point from the profile's current saves + VR settings.
        public void SetStartPoint(string id){var p=Profile(id);KeepCopy(p,"start","start");p.start_utc=DateTime.UtcNow.ToString("o");SaveProfile(p);}
        public void SetTest(string id,bool on){
            var p=Profile(id);p.test=on;SaveProfile(p);
            if(on&&ReadCopy(id,"start")==null)SetStartPoint(id);
        }
        // Profiles are never destroyed outright: they move to a recycle folder that can be restored by hand.
        public void DeleteProfile(string id){
            string d=ProfileDir(id),bin=env.Dir("recycle");
            Directory.Move(d,Path.Combine(bin,id+"-"+DateTime.UtcNow.ToString("yyyyMMddTHHmmss")));
        }
    }
}
