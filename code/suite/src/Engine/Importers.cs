using System;
using System.IO;
using System.Linq;
using System.Collections.Generic;

namespace GtavxrSuite {
    // Contract a build pipeline writes next to release.json (suite-launch.json) so the Suite can start it without a CMD.
    public class SuiteLaunchFile {
        public string schema {get;set;}
        public string id {get;set;}
        public string build {get;set;}
        public string title {get;set;}
        public List<string> notes {get;set;}
        public List<string> notes_en {get;set;}           // the same notes in English
        public string title_en {get;set;}
        public List<string> args {get;set;}
        public string mvga {get;set;}
        public string mvga_arg {get;set;}
        public string content {get;set;}
        public string script_host {get;set;}              // "switch": GTAVXRHost.dll + scripthost/ in the release (ScriptHookV-SDK mods)
        public string script_host_arg {get;set;}
        public List<string> pso_xml {get;set;}            // PSO roots the EXE also reads as XML (VersionManifest.pso_xml)
        // 2.4.0 (167-neural A3): where the EXE's sessions / VR settings live ("suite" = the Suite names them; absent = "legacy-repo"),
        // runtime files beside the bridge (its own OpenXR loader, the NGX forwarder copies) and the per-eye DLSS contract
        public string session_mode {get;set;}
        public string settings_mode {get;set;}
        public List<string> runtime_files {get;set;}
        public NeuralContract neural {get;set;}
    }

    // Brings existing development builds, MVGA and the current VR progress into the Suite library (copies only).
    public sealed class Importers {
        private readonly Env env;private readonly Library lib;private readonly Blobs blobs;
        public Importers(Env e){env=e;lib=new Library(e);blobs=new Blobs(e);}

        private static Dictionary<string,object> Obj(object o){return o as Dictionary<string,object>;}
        private static string Str(Dictionary<string,object> d,string k){object v;return d!=null&&d.TryGetValue(k,out v)?v as string:null;}
        private static long Num(Dictionary<string,object> d,string k){object v;return d!=null&&d.TryGetValue(k,out v)&&v!=null?Convert.ToInt64(v):0;}
        private static object[] Arr(Dictionary<string,object> d,string k){object v;if(d==null||!d.TryGetValue(k,out v)||v==null)return new object[0];var e=v as System.Collections.IEnumerable;return e==null||v is string?new object[0]:e.Cast<object>().ToArray();}

        public string ReleasesRoot{get{return env.DevRepo==null?null:Path.Combine(env.DevRepo,"local-cache","play","releases");}}
        public string CurrentStableName(){
            if(env.DevRepo==null)return null;
            string p=Path.Combine(env.DevRepo,"local-cache","play","current.json");
            return File.Exists(p)?Str(Json.Read<Dictionary<string,object>>(p),"name"):null;
        }

        // Releases the Suite can start: the stable one, plus any with a suite-launch.json contract.
        public List<string> ImportableReleases(){
            var list=new List<string>();string root=ReleasesRoot;if(root==null||!Directory.Exists(root))return list;
            string stable=CurrentStableName();
            foreach(string d in Directory.GetDirectories(root)){
                string name=Path.GetFileName(d);
                if(!File.Exists(Path.Combine(d,"release.json")))continue;
                if(name==stable||File.Exists(Path.Combine(d,"suite-launch.json")))list.Add(name);
            }
            return list;
        }

        // 2.4.1: a version whose stored manifest does not reflect its release's suite-launch.json (an older Suite imported it and
        // skipped fields it did not know, e.g. 2.3.9 dropped 2.4.0's modes / runtime files / graphics) must be imported again.
        // With a recorded contract hash any change counts; without one, the fields older Suites ignored are compared.
        public bool ContractStale(VersionManifest v){
            if(v==null||String.IsNullOrEmpty(v.source_name)||ReleasesRoot==null)return false;
            try{
                string file=Path.Combine(ReleasesRoot,v.source_name,"suite-launch.json");
                if(!File.Exists(file))return false;
                if(v.contract_sha256!=null)return !String.Equals(v.contract_sha256,SafeFs.Hash(file),StringComparison.OrdinalIgnoreCase);
                var c=Json.Read<SuiteLaunchFile>(file);
                if(c==null||c.schema!="gtavxr-suite2-launch-v1")return false;
                string sm=c.session_mode??"legacy-repo",tm=c.settings_mode??"legacy-repo";
                if(v.launch==null||(v.launch.session_mode??"legacy-repo")!=sm||(v.launch.settings_mode??"legacy-repo")!=tm)return true;
                if((c.neural!=null)!=(v.neural!=null))return true;
                var have=new HashSet<string>((v.runtime??new List<FileRef>()).Select(f=>f.path),StringComparer.OrdinalIgnoreCase);
                foreach(string f in c.runtime_files??new List<string>())if(!have.Contains(f.Replace('\\','/')))return true;
                if((c.script_host??"")!=(v.script_host??"")||(c.content??"")!=(v.content??""))return true;
                return false;
            }catch(Exception){return false;}
        }

        // Arguments a release was actually started with by its launcher, read from that release's own session receipts.
        public List<string> RecordedArgs(string releaseSha){
            List<string> found=null;int sessions=0;
            if(!Directory.Exists(env.LegacySessionsRoot))return null;
            foreach(string d in Directory.GetDirectories(env.LegacySessionsRoot)){
                string l=Path.Combine(d,"launch.json");if(!File.Exists(l))continue;
                Dictionary<string,object> j;try{j=Json.Read<Dictionary<string,object>>(l);}catch(Exception){continue;}
                if(!String.Equals(Str(j,"release_sha256"),releaseSha,StringComparison.OrdinalIgnoreCase))continue;
                var a=Arr(j,"arguments").Select(x=>x as string).ToList();
                int log=a.IndexOf("-logfile");if(log<0||log+1>=a.Count)throw new InvalidDataException("Recorded launch without log file");
                a[log+1]="{LOGFILE}";
                if(found==null)found=a;else if(!found.SequenceEqual(a))throw new InvalidDataException("Recorded launches of this release disagree");
                sessions++;
            }
            return found;
        }

        public VersionManifest ImportRelease(string name,Action<int,int> progress){
            return ImportReleaseFrom(name,Path.Combine(ReleasesRoot,name),progress,false);
        }
        internal VersionManifest ImportEmbedded(string dir,Action<int,int> progress){
            return ImportReleaseFrom(EmbeddedRelease.Name,dir,progress,true);
        }
        VersionManifest ImportReleaseFrom(string name,string dir,Action<int,int> progress,bool embedded){
            string releaseFile=SafeFs.Under(dir,"release.json");
            var r=Json.Read<Dictionary<string,object>>(releaseFile);
            if(Str(r,"name")!=name)throw new InvalidDataException("Release name mismatch");
            string releaseSha=embedded?Str(r,"embedded_identity"):SafeFs.Hash(releaseFile);
            if(!SafeFs.IsHash(releaseSha))throw new InvalidDataException("Release identity missing");
            string stable=CurrentStableName();
            var contract=File.Exists(Path.Combine(dir,"suite-launch.json"))?Json.Read<SuiteLaunchFile>(Path.Combine(dir,"suite-launch.json")):null;
            if(contract!=null&&contract.schema!="gtavxr-suite2-launch-v1")throw new InvalidDataException("Unknown launch contract");
            List<string> args=contract!=null?contract.args:RecordedArgs(releaseSha);
            if(args==null||args.Count==0||!args.Contains("{LOGFILE}"))throw new InvalidDataException("No known way to start this release");
            string retailExe=Str(r,"retail_sha256");
            var retail=LegacyGamePolicy.Legacy3889();
            if(!String.Equals(retail["GTA5.exe"],retailExe,StringComparison.OrdinalIgnoreCase))throw new InvalidDataException("Release targets another GTA build");
            var v=new VersionManifest{id=contract!=null?contract.id:IdFor(name),build=contract!=null?contract.build:BuildOf(name),title=contract!=null?contract.title:TitleOf(name),title_en=contract!=null?contract.title_en:null,
                channel=embedded||name==stable?"stable":"test",embedded_release=embedded,added_utc=DateTime.UtcNow.ToString("o"),notes=contract!=null&&contract.notes!=null?contract.notes:new List<string>(),notes_en=contract!=null?contract.notes_en:null,
                source_name=name,source_sha256=releaseSha,retail=retail,files=new List<FileRef>(),runtime=new List<FileRef>(),
                mvga=contract!=null?contract.mvga:"none",mvga_arg=contract!=null?contract.mvga_arg:null,content=contract!=null?contract.content:null,
                script_host=contract!=null?contract.script_host:null,script_host_arg=contract!=null?contract.script_host_arg:null,
                pso_xml=contract!=null&&contract.pso_xml!=null?contract.pso_xml:new List<string>()};
            var existing=lib.Version(v.id);
            if(!embedded&&existing!=null&&existing.embedded_release)throw new InvalidDataException("Version already belongs to an embedded release");
            if(v.script_host=="switch"&&String.IsNullOrEmpty(v.script_host_arg))throw new InvalidDataException("Script host release without its argument");
            var payloads=Arr(r,"payloads").Select(Obj).ToList();
            var mvgaFiles=new List<FileRef>();
            int done=0;
            foreach(var p in payloads){
                string rel=SafeFs.Relative(Str(p,"relative").Replace('\\','/'));
                string sha=Str(p,"sha256");long bytes=Num(p,"bytes");
                blobs.Put(Str(p,"source"),sha,bytes);
                var orig=p.ContainsKey("original")?Obj(p["original"]):null;
                if(orig!=null){
                    // This build swaps a game file while playing (the launcher does the same and restores it at exit).
                    if(v.replaces==null)v.replaces=new List<ReplaceRef>();
                    v.replaces.Add(new ReplaceRef{path=rel,sha256=sha.ToUpperInvariant(),bytes=bytes,original_sha256=Str(orig,"sha256").ToUpperInvariant(),original_bytes=Num(orig,"bytes")});
                    if(progress!=null&&(++done%50==0||done==payloads.Count))progress(done,payloads.Count);
                    continue;
                }
                var f=new FileRef{path=rel,sha256=sha.ToUpperInvariant(),bytes=bytes};
                if(rel.StartsWith("gtavxr_mvga147/",StringComparison.OrdinalIgnoreCase))mvgaFiles.Add(f);else v.files.Add(f);
                if(progress!=null&&(++done%50==0||done==payloads.Count))progress(done,payloads.Count);
            }
            if(mvgaFiles.Count>0&&v.mvga=="none")v.mvga="required";
            // Runtime folder: bridge + trainer next to it + the crash collector.
            v.runtime.Add(Runtime(Str(r,"bridge"),"gtavxr_native_openxr.dll",r));
            v.runtime.Add(Runtime(Path.Combine(dir,"gtavxr_trainer.dll"),"gtavxr_trainer.dll",r));
            // Script host: GTAVXRHost.dll beside the bridge (the EXE loads it from there), our ScriptHookVDotNet build in scripthost/.
            if(v.script_host=="switch"){
                v.runtime.Add(Runtime(Path.Combine(dir,"GTAVXRHost.dll"),"GTAVXRHost.dll",r));
                foreach(string f in Directory.GetFiles(Path.Combine(dir,"scripthost")).OrderBy(x=>x,StringComparer.OrdinalIgnoreCase))
                    v.runtime.Add(Runtime(f,"scripthost/"+Path.GetFileName(f),r));
            }
            foreach(string f in contract!=null&&contract.runtime_files!=null?contract.runtime_files:new List<string>()){
                string leaf=SafeFs.Relative(f.Replace('\\','/'));
                if(leaf.Contains("/"))throw new InvalidDataException("Runtime file outside the bridge folder: "+f);
                v.runtime.Add(Runtime(Path.Combine(dir,leaf),leaf,r));
            }
            v.neural=contract!=null?contract.neural:null;
            v.contract_sha256=contract!=null?SafeFs.Hash(Path.Combine(dir,"suite-launch.json")):null;
            var fm=Obj(r.ContainsKey("fault_monitor")?r["fault_monitor"]:null);
            if(fm!=null){blobs.Put(Str(fm,"source"),Str(fm,"sha256"),Num(fm,"bytes"));v.runtime.Add(new FileRef{path="native_fault_monitor.exe",sha256=Str(fm,"sha256").ToUpperInvariant(),bytes=Num(fm,"bytes")});}
            string exe=v.files.Select(f=>f.path).FirstOrDefault(f=>f.EndsWith(".exe",StringComparison.OrdinalIgnoreCase)&&!f.Contains("/"));
            if(exe==null)throw new InvalidDataException("Release has no game executable");
            string sessionMode=contract!=null&&contract.session_mode!=null?contract.session_mode:"legacy-repo";
            string settingsMode=contract!=null&&contract.settings_mode!=null?contract.settings_mode:"legacy-repo";
            foreach(string m in new[]{sessionMode,settingsMode})if(m!="legacy-repo"&&m!="suite")throw new InvalidDataException("Unknown launch mode: "+m);
            if((sessionMode=="suite")!=(settingsMode=="suite"))throw new InvalidDataException("Session and settings modes differ");
            if(sessionMode=="suite"&&!v.runtime.Any(f=>f.path=="openxr_loader.dll"))throw new InvalidDataException("A suite-mode release brings its own openxr_loader.dll");
            v.launch=new LaunchContract{exe=exe,args=args,disable_layers=Arr(r,"implicit_layers").Select(Obj).Select(l=>Str(l,"disable")).Where(x=>!String.IsNullOrEmpty(x)).ToList(),
                session_mode=sessionMode,settings_mode=settingsMode,display="windowed1920x1080_stereo0",residual="exact-one-thread"};
            string seed=Path.Combine(dir,"trial-profile","vr-settings.ini");
            if(File.Exists(seed))v.vr_settings_seed=blobs.PutFile(seed,"vr-settings.ini");
            lib.SaveVersion(v);
            if(mvgaFiles.Count>0)ImportMvga(mvgaFiles,name);
            return v;
        }
        private FileRef Runtime(string source,string leaf,Dictionary<string,object> release){
            if(source==null||!File.Exists(source))throw new InvalidDataException("Release runtime file missing: "+leaf);
            // Prefer the release's own recorded hash for this file when it lists it.
            foreach(var f in Arr(release,"files").Select(Obj)){
                if(String.Equals(Str(f,"source"),source,StringComparison.OrdinalIgnoreCase)){blobs.Put(source,Str(f,"sha256"),Num(f,"bytes"));return new FileRef{path=leaf,sha256=Str(f,"sha256").ToUpperInvariant(),bytes=Num(f,"bytes")};}
            }
            return blobs.PutFile(source,leaf);
        }
        public static string IdFor(string release){
            // native-xr-build125-dual-v1 -> 125-dual ; native-xr-build155-mvga265-v1 -> 155
            string s=release.StartsWith("native-xr-build")?release.Substring("native-xr-build".Length):release;
            if(s.EndsWith("-v1"))s=s.Substring(0,s.Length-3);
            s=s.Replace("-mvga265","");
            return new string(s.Where(c=>Char.IsLetterOrDigit(c)||c=='-').ToArray());
        }
        public static string BuildOf(string release){string id=IdFor(release);return new string(id.TakeWhile(Char.IsDigit).ToArray());}
        public static string TitleOf(string release){
            string id=IdFor(release);string rest=id.Substring(BuildOf(release).Length).Trim('-');
            if(rest=="dual")return "Dual";
            return rest.Length==0?"":Char.ToUpperInvariant(rest[0])+rest.Substring(1);
        }

        // MVGA files shipped inside a release become one library mod (the same mod for every version that accepts it).
        private ModManifest ImportMvga(List<FileRef> files,string release){
            string family="mvga";
            var existing=lib.Mods().FirstOrDefault(m=>m.family==family&&m.files!=null&&m.files.Count==files.Count&&!m.files.Where((f,i)=>f.sha256!=files[i].sha256).Any());
            if(existing!=null)return existing;
            var sameFamily=lib.Mods().Where(m=>m.family==family).ToList();
            string id="mvga-2-65-vr"+(sameFamily.Count==0?"":"-"+(sameFamily.Count+1));
            var mod=new ModManifest{id=id,name="Make Visuals Great Again",version="2.65 · adaptado a VR",category="graphics",added_utc=DateTime.UtcNow.ToString("o"),
                loadable=true,kind="mvga-native",files=files,args=new List<string>{"-gtavxrMvga"},family=family};
            lib.SaveMod(mod);
            return mod;
        }

        // (Build 166 project-built content packs are no longer imported: the Suite builds the profile from the player's own
        // mods, ContentSources; ReplaceProjectPacks retires the ones already in a library.)

        // Any ZIP / OIV / RAR: inspected without running anything and kept in the library. Content mods inside it (.oiv
        // packages, loose DLC packs) become content sources the Suite merges onto the game at Play; the card returned is the
        // main one (an official option such as WoV's No Umbrellas has no card of its own).
        public ModManifest ImportArchive(string path){
            var mvga=MvgaOptional.TryImport(env,lib.Version(EmbeddedRelease.VersionId),path);
            if(mvga!=null)return ImportMvga(mvga,EmbeddedRelease.Name);
            var cs=new ContentSources(env);var content=cs.Import(path,null);
            if(content.Count>0){cs.ReplaceProjectPacks();return content.FirstOrDefault(m=>m.IsCard())??content[0];}
            // A script mod (ScriptHookVDotNet: scripts/*.dll), with its game files (OpenIV folder) as its companion.
            var script=new ScriptMods(env).Import(path);
            if(script!=null)return script;
            if(Directory.Exists(path)||path.EndsWith(".rpf",StringComparison.OrdinalIgnoreCase))throw new InvalidDataException("NO_CONTENT_MODS");
            return ImportPlainArchive(path);
        }
        ModManifest ImportPlainArchive(string path){
            string scratch=env.Dir("scratch/"+Ids.New());
            Inspection ins;try{ins=Importer.Inspect(path,scratch);}finally{try{Directory.Delete(scratch,true);}catch(Exception){}}
            var archive=blobs.PutFile(path,Path.GetFileName(path));
            string name=Path.GetFileNameWithoutExtension(path).Replace('_',' ').Replace('.',' ').Trim();
            var m=new ModManifest{id="mod-"+Ids.New().Substring(0,12),name=name,version="",category=Guess(ins),added_utc=DateTime.UtcNow.ToString("o"),
                loadable=false,not_loadable_reason="ENGINE_PENDING",kind="archive",archive=archive,files=new List<FileRef>(),args=new List<string>(),family=Family(name)};
            var same=lib.Mods().FirstOrDefault(x=>x.family==m.family&&x.kind=="archive");
            if(same!=null)m.version="";
            lib.SaveMod(m);
            Json.Atomic(SafeFs.Under(env.Dir("mods"),m.id+".inspection.json"),ins);
            return m;
        }
        private static string Family(string name){return new string(name.ToLowerInvariant().Where(c=>Char.IsLetter(c)).ToArray());}
        private static string Guess(Inspection ins){
            string all=String.Join(" ",ins.members).ToLowerInvariant();
            if(all.Contains("handling.meta")||all.Contains("vehicles.meta"))return "driving";
            if(all.Contains(".ymap")||all.Contains(".ytyp")||all.Contains("dlc.rpf"))return "map";
            if(all.Contains("timecycle")||all.Contains("visualsettings")||all.Contains(".ytd"))return "graphics";
            return "other";
        }

        // ---------- profiles from the existing launcher state (copies) ----------
        public Profile ImportAcceptedProfile(string name,string versionId){
            string cache=Path.Combine(env.DevRepo,"local-cache","play");
            var ptr=Json.Read<Dictionary<string,object>>(Path.Combine(cache,"profile-current.json"));
            string session=Str(ptr,"session");string manifest=Path.Combine(session,"after-user.json");
            if(!String.Equals(SafeFs.Hash(manifest),Str(ptr,"manifest_sha256"),StringComparison.OrdinalIgnoreCase))throw new InvalidDataException("VR profile changed since it was saved");
            var files=FilesOf(Json.Read<Dictionary<string,object>>(manifest));
            return CreateFrom(name,versionId,Path.Combine(session,"vr-user"),files,File.Exists(env.LegacySettingsPath)?env.LegacySettingsPath:null,"import-accepted");
        }
        public Profile ImportCandidateProfile(string releaseName,string name,string versionId){
            string cache=Path.Combine(env.DevRepo,"local-cache","play"),dir=Path.Combine(ReleasesRoot,releaseName);
            string pointer=Path.Combine(cache,"candidate-profiles",releaseName+".json");
            if(File.Exists(pointer)){
                var ptr=Json.Read<Dictionary<string,object>>(pointer);string session=Str(ptr,"session");string manifest=Path.Combine(session,"after-user.json");
                if(!String.Equals(SafeFs.Hash(manifest),Str(ptr,"manifest_sha256"),StringComparison.OrdinalIgnoreCase))throw new InvalidDataException("Candidate progress changed");
                string ini=Path.Combine(session,"candidate-vr-settings.ini");
                return CreateFrom(name,versionId,Path.Combine(session,"vr-user"),FilesOf(Json.Read<Dictionary<string,object>>(manifest)),File.Exists(ini)?ini:null,"import-candidate");
            }
            var seed=Json.Read<Dictionary<string,object>>(Path.Combine(dir,"trial-profile","manifest.json"));
            string seedIni=Path.Combine(dir,"trial-profile","vr-settings.ini");
            return CreateFrom(name,versionId,Path.Combine(dir,"trial-profile","data"),FilesOf(seed),File.Exists(seedIni)?seedIni:null,"import-candidate-seed");
        }
        private static List<SaveFileRef> FilesOf(Dictionary<string,object> manifest){
            return Arr(manifest,"files").Select(Obj).Select(f=>new SaveFileRef{relative=SafeFs.Relative(Str(f,"relative").Replace('\\','/')),sha256=Str(f,"sha256").ToUpperInvariant(),bytes=Num(f,"bytes")}).ToList();
        }
        public Profile CreateFrom(string name,string versionId,string userSource,List<SaveFileRef> files,string vrIni,string origin){
            var p=new Profile{id=Ids.New().Substring(0,16),name=name,color=lib.Profiles().Count%Palette,version=versionId,mods=new List<string>(),created_utc=DateTime.UtcNow.ToString("o"),user=files,origin=origin};
            UserTree.Verify(userSource,files);UserTree.CopyInto(userSource,files,lib.UserDir(p.id));
            if(vrIni!=null){File.Copy(vrIni,lib.VrSettingsFile(p.id),true);p.has_vr_settings=true;}
            lib.SaveProfile(p);return p;
        }
        public const int Palette=6;
        internal Profile CopyForEmbeddedRelease(Profile previous,string versionId){
            var profile=Duplicate(previous.id,previous.name+" · GTAVXR 1.0");
            profile.version=versionId;
            profile.options=previous.options==null?null:new Dictionary<string,string>(previous.options);
            foreach(var pair in new[]{new[]{lib.ScriptsDir(previous.id),lib.ScriptsDir(profile.id)},
                                      new[]{lib.AsiDir(previous.id),lib.AsiDir(profile.id)}}){
                var files=UserTree.Snapshot(pair[0]);
                UserTree.CopyInto(pair[0],files,pair[1]);
            }
            lib.SaveProfile(profile);
            return profile;
        }
        // New profile from the player's normal GTA saves (copy) or an empty new story.
        public Profile CreateProfile(string name,string versionId,bool copyNormalSaves,string copyVrFromProfile){
            List<SaveFileRef> files;string src;
            if(copyNormalSaves){src=env.UserDataDir;files=UserTree.Snapshot(src);}
            else{
                // New story: keep only settings.xml so graphics options carry over.
                src=env.UserDataDir;files=UserTree.Snapshot(src).Where(f=>f.relative=="settings.xml").ToList();
            }
            string ini=null;if(copyVrFromProfile!=null){string f=lib.VrSettingsFile(copyVrFromProfile);if(File.Exists(f))ini=f;}
            var p=CreateFrom(name,versionId,src,files,ini,copyNormalSaves?"new-copy-normal":"new-story");
            if(ini==null){
                // No profile to copy from: start like "Historia VR" (its settings list, not its calibration), so every
                // version, old ones too, starts the same. The game fills in everything else with its own defaults.
                File.WriteAllText(lib.VrSettingsFile(p.id),StartingVrSettings,new System.Text.UTF8Encoding(false));
                p=lib.Profile(p.id);p.has_vr_settings=true;lib.SaveProfile(p);
            }
            return p;
        }
        // The "Historia VR" settings list (user choice 2026-10-28, build 166 S2; the same list is the 165 EXE's defaults).
        public const string StartingVrSettings="[VR]\r\nsmooth=1\r\nsmooth_degrees=120\r\nsniper_scope_view=0\r\nsupport_grip_mode=1\r\nhand_mode147=0\r\n"+
            "recoil_mode149=1\r\nreload_pistol149=1\r\nreload_revolver149=1\r\nreload_smg149=1\r\nreload_rifle149=1\r\nreload_shotgun149=1\r\n"+
            "reload_sniper149=1\r\nreload_mg149=1\r\nreload_launcher149=1\r\n";
        // Replace a profile's saves with a copy of normal GTA's (or a new story). The current ones go to the profile history first.
        public void ResetSaves(string id,bool copyNormal){
            var p=lib.Profile(id);string user=lib.UserDir(id);
            string tag=DateTime.UtcNow.ToString("yyyyMMddTHHmmss")+"-reset";
            lib.KeepCopy(p,"history/"+tag,tag);
            string history=SafeFs.Under(lib.CopyDir(id,"history/"+tag),"user");
            var files=UserTree.Snapshot(env.UserDataDir);
            if(!copyNormal){
                // Keep the profile's own settings.xml if it has one, else the normal one; no saves.
                bool own=p.user!=null&&p.user.Any(f=>f.relative=="settings.xml");
                var keep=own?p.user.Where(f=>f.relative=="settings.xml").ToList():files.Where(f=>f.relative=="settings.xml").ToList();
                string src=own?history:env.UserDataDir;
                UserTree.Mirror(src,keep,user);p.user=keep;
            }else{UserTree.Mirror(env.UserDataDir,files,user);p.user=files;}
            UserTree.Verify(user,p.user);lib.SaveProfile(p);lib.PruneHistory(id,6);
        }
        // New test profile: same as a normal new profile (or a copy of another), marked test, with its starting point.
        public Profile CreateTestProfile(string name,string versionId,string copyOf,bool copyNormal){
            Profile p=copyOf!=null?Duplicate(copyOf,name):CreateProfile(name,versionId,copyNormal,null);
            p=lib.Profile(p.id);p.version=versionId;lib.SaveProfile(p);
            lib.SetTest(p.id,true);return lib.Profile(p.id);
        }
        // "Deshacer la partida de esta prueba": the profile goes back to how it was before that session.
        public void UndoSession(string profileId,string sessionId){lib.RestoreCopy(profileId,"history/"+sessionId,DateTime.UtcNow.ToString("yyyyMMddTHHmmss")+"-undo");}
        // "Volver al punto de partida" of a test profile.
        public void BackToStart(string profileId){lib.RestoreCopy(profileId,"start",DateTime.UtcNow.ToString("yyyyMMddTHHmmss")+"-start");}
        // "Pasar a mi perfil": the other profile takes this test profile's version and mods (its saves do not change).
        // The mods count as tested from now on.
        public void Promote(string testProfileId,string targetProfileId){
            var a=lib.Profile(testProfileId);var b=lib.Profile(targetProfileId);
            if(a==null||b==null||a.id==b.id)throw new InvalidDataException("Profile not found");
            b.version=a.version;b.mods=new List<string>(a.mods);lib.SaveProfile(b);
            foreach(string id in a.mods){var m=lib.Mod(id);if(m!=null&&m.tested_utc==null){m.tested_utc=DateTime.UtcNow.ToString("o");lib.SaveMod(m);}}
        }
        public Profile Duplicate(string id,string name){
            var p=lib.Profile(id);string ini=lib.VrSettingsFile(id);
            var copy=CreateFrom(name,p.version,lib.UserDir(id),p.user,p.has_vr_settings&&File.Exists(ini)?ini:null,"copy");
            copy.mods=new List<string>(p.mods);copy.color=(p.color+1)%Palette;copy.image=p.image;lib.SaveProfile(copy);return copy;
        }
    }
}
