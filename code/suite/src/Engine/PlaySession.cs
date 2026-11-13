using System;
using System.IO;
using System.Linq;
using System.Text;
using System.Diagnostics;
using System.Collections.Generic;
using System.Security.Cryptography;

namespace GtavxrSuite {
    public class SessionRecord {
        public string schema {get;set;}
        public string id {get;set;}
        public string profile {get;set;}
        public string profile_name {get;set;}
        public string version {get;set;}
        public List<string> mods {get;set;}
        public List<string> omitted_mods {get;set;}
        public bool isolated_mod_state {get;set;}
        public string phase {get;set;}          // see PlaySession.Phases
        public string native_session {get;set;}
        public List<SaveFileRef> retail_user {get;set;}
        public bool vr_settings_was_present {get;set;}
        public ChildIdentity game {get;set;}
        public bool launch_attempted {get;set;}
        public string launch_staging_sha256 {get;set;}
        public bool lease {get;set;}
        public string lease_held_by {get;set;}  // an AI agent held play-runtime: the user played anyway (leases limit agents, never the user)
        public bool collector {get;set;}
        public string started_utc {get;set;}
        public string launched_utc {get;set;}
        public string ended_utc {get;set;}
        public int? exit_code {get;set;}
        public bool residual_closed {get;set;}
        public bool exit_marker {get;set;}        // in-<pid> was in place, so GTA announces its final exit with out-<pid>
        public bool exit_file_seen {get;set;}     // GTA wrote out-<pid>: saves finished, ExitProcess was next
        public string exit_hang {get;set;}        // closed-after-exit-file | forced-by-user | null
        public string exit_dump {get;set;}        // thread-stack dump of the stuck game (session folder), null if none
        public bool close_requested {get;set;}    // "Cerrar GTA": the game window was asked to close (clean exit route)
        public List<ReplaceRef> swaps {get;set;} // game files swapped in for this session (restored at the end, also by Recover)
        public bool swapped {get;set;}
        public List<FileRef> aside {get;set;}    // store-game mods GTAVXR's EXE cannot run, kept in the session folder while it played
        public bool aside_moved {get;set;}
        public List<string> aside_kept {get;set;} // a new file took the name meanwhile: the player's file stays in the session folder
        // Script host session (a script mod active on a version with the script host): the game's scripts/ folder is this profile's
        // while it plays; the player's own scripts/ waits beside it as scripts_aside. Written before each move (Recover finishes it).
        public bool script_host {get;set;}
        public string scripts_aside {get;set;}            // folder name in the game folder ("scripts.gtavxr-<session>")
        public bool scripts_retail_present {get;set;}     // the player had a scripts/ folder before
        public bool scripts_staged {get;set;}             // the profile's scripts/ is (being) placed
        public List<FileRef> scripts_placed {get;set;}    // the mods' files placed (paths inside scripts/): not captured back
        public string scripts_host {get;set;}             // GTAVXR_SCRIPTS_PATH: this session's copy of the version's scripthost/
        public bool scripts_done {get;set;}               // the player's scripts/ is back
        // 2.4.4: ScriptHookV .asi mods: their files in the game folder for this session, beside the .asi as in the store game.
        // Written before each move (Recover finishes it); only what the session placed or what the mods wrote is taken out again.
        public List<FileRef> asi_original {get;set;}      // the mods' own files (store copies, .asi renamed): changes to them are kept
        public List<FileRef> asi_placed {get;set;}        // what the session put in the game folder (the profile's saved copy wins)
        public List<string> asi_before {get;set;}         // game-folder files before: top-level names + files in folders it placed into
        public List<string> asi_dirs {get;set;}           // folders the session created (removed at the end when empty)
        public List<FileRef> asi_aside {get;set;}         // the player's own files with the same names, kept in the session folder
        public bool asi_staged {get;set;}
        public List<string> asi_claimed {get;set;}        // what the capture took for the profile (taken out of the game folder)
        public List<string> asi_kept {get;set;}           // a player's file that could not go back (a new file took the name): in the session
        public bool asi_done {get;set;}
        public string result {get;set;}         // played | cancelled | failed
        public string error {get;set;}
        // Test flight (test profiles): what GTAVXR looked like in the game before, and what the way back found.
        public bool test {get;set;}
        public string prev_version {get;set;}
        public List<string> prev_mods {get;set;}
        public bool aborted {get;set;}
        public List<string> quarantined {get;set;}
        public List<string> changed_game_files {get;set;}
        public List<string> missing_game_files {get;set;}
        public string retail_check {get;set;}   // ok | changed | null (not checked)
        // 2.4.0 (167-neural): where this session's EXE keeps its sessions / VR settings, the OpenXR runtime it used, the
        // graphics choice and the DLSS folder it got, and every external component as found (recorded, never refused by hash)
        public string session_mode {get;set;}
        public string settings_mode {get;set;}
        public string vr_settings {get;set;}            // settings_mode "suite": this session's copy of the profile's vr-settings.ini
        public string runtime_manifest {get;set;}
        public string runtime_name {get;set;}
        public GraphicsChoice graphics {get;set;}
        public string dlss_dir {get;set;}               // GTAVXR_DLSS_DIR given to the game (null = the chain stays off)
        public string dlss_note {get;set;}              // why not: no_dlss_folder | dlss_invalid:<reason> | null
        public List<ComponentRef> components {get;set;}
        // 2.4.2: the script mods that made this session start with "Online maps in Story" on (null = none needed it)
        public List<string> online_maps_for {get;set;}
    }
    public class ComponentRef { public string kind {get;set;} public string path {get;set;} public string version {get;set;} public string sha256 {get;set;} public long bytes {get;set;} public string note {get;set;} }
    public class PlayProgress { public string step {get;set;} public int done {get;set;} public int total {get;set;} }

    // One play: put the profile in place, run the game, bring the progress back, leave GTA as it was.
    //   STARTING -> GAME_READY -> USER_SAVED -> USER_STAGED -> LAUNCHED -> EXITED -> CAPTURED -> RESTORED -> REVERTED -> CLOSED
    // REVERTED only for test flights: GTAVXR in the game goes back to the version + mods installed before the test.
    // Every phase is written before its effects, so a crash at any point is finished by Recover().
    public sealed partial class PlaySession {
        public static readonly string[] Phases={"STARTING","GAME_READY","USER_SAVED","USER_STAGED","LAUNCHED","EXITED","CAPTURED","RESTORED","REVERTED","CLOSED"};
        private readonly Env env;private readonly Library lib;private readonly GameLayer layer;private readonly Blobs blobs;
        public Func<ProcessStartInfo,Process> Starter {get;set;}   // tests replace the game with an owned child
        public Action<Process> AfterLaunched {get;set;}            // tests: simulate the Suite dying while the game runs
        private readonly object gate=new object();private Process running;private SessionRecord current;
        public PlaySession(Env e){env=e;lib=new Library(e);layer=new GameLayer(e);blobs=new Blobs(e);Starter=Process.Start;}

        // "Abortar y restaurar": ends the game this session started (that exact process object, nothing else).
        // The session then lands normally: progress captured, normal GTA back, and for a test flight the game put back.
        public bool Abort(){
            lock(gate){
                if(running==null||current==null)return false;
                // The flag is written by the session thread's next phase save (no second writer on session.json).
                try{
                    if(running.HasExited)return false;current.aborted=true;
                    // Keep where the game was stuck before ending it (thread stacks only, a few MB).
                    current.exit_hang="forced-by-user";current.exit_dump=DumpThreads(running,SafeFs.Under(SessionDir(current.id),"exit-hang.dmp"));
                    running.Kill();return true;
                }
                catch(InvalidOperationException){return false;}
            }
        }

        // "Cerrar GTA": ask the game window to close, the same route as Task Manager "End task", which in the worn 157 session
        // ran GTA's own exit (saves finished, out-<pid> written, exit code 0). Nothing is killed here; a game that then sticks in
        // its own exit is handled by WaitForGame, and "Forzar cierre" stays available.
        public bool CloseGame(){
            lock(gate){
                if(running==null||current==null)return false;
                try{
                    if(running.HasExited)return false;
                    running.Refresh();
                    if(!running.CloseMainWindow())return false;
                    current.close_requested=true;return true;
                }catch(InvalidOperationException){return false;}
            }
        }

        private string SessionDir(string id){Ids.Check(id);return SafeFs.Under(env.Dir("sessions"),id);}
        private void Save(SessionRecord r){Json.Atomic(SafeFs.Under(SessionDir(r.id),"session.json"),r);}
        private static int Rank(string phase){return Array.IndexOf(Phases,phase);}

        public List<SessionRecord> Unfinished(){
            var list=new List<SessionRecord>();
            foreach(string d in Directory.GetDirectories(env.Dir("sessions"))){
                string f=Path.Combine(d,"session.json");if(!File.Exists(f))continue;
                var r=Json.Read<SessionRecord>(f);if(r.phase!="CLOSED")list.Add(r);
            }
            return list;
        }
        public SessionRecord Last(){
            SessionRecord best=null;
            foreach(string d in Directory.GetDirectories(env.Dir("sessions"))){
                string f=Path.Combine(d,"session.json");if(!File.Exists(f))continue;
                var r=Json.Read<SessionRecord>(f);if(best==null||String.CompareOrdinal(r.started_utc,best.started_utc)>0)best=r;
            }
            return best;
        }
        public bool GameRunning(){
            foreach(string n in env.GameProcessNames){var ps=Process.GetProcessesByName(n);bool any=ps.Any(p=>!p.HasExited);foreach(var p in ps)p.Dispose();if(any)return true;}
            return false;
        }

        // Full play. Blocks until the game has closed and everything is back in place.
        public SessionRecord Play(string profileId,Action<PlayProgress> progress){
            return Play(profileId,progress,new PlayOptions());
        }

        public SessionRecord Play(string profileId,Action<PlayProgress> progress,PlayOptions options){
            if(Unfinished().Count>0)throw new InvalidOperationException("RECOVERY_REQUIRED");
            if(layer.Pending)throw new InvalidOperationException("RECOVERY_REQUIRED");
            if(GameRunning())throw new InvalidOperationException("GAME_RUNNING");
            var p=lib.Profile(profileId);if(p==null)throw new InvalidDataException("Profile not found");
            var v=lib.Version(p.version);if(v==null)throw new InvalidDataException("VERSION_MISSING");
            // 2.4.0: a version that reads the repository folders (125-167) runs only where that repository is
            bool suiteMode=v.launch.session_mode=="suite";
            if(!suiteMode&&!Directory.Exists(Path.GetDirectoryName(env.LegacySettingsPath)))throw new InvalidDataException("LEGACY_REPO_ONLY");
            // the EXE reads the paths it is given with the ANSI API: a Suite data folder that code page cannot spell is refused here
            if(suiteMode&&!AnsiPath(env.DataRoot))throw new InvalidDataException("DATA_PATH_NOT_ANSI");
            if(v.id==EmbeddedRelease.VersionId&&v.embedded_release){
                var pathProblem=PlatformCompatibility.SessionPathProblem(env.DataRoot);
                if(pathProblem!=null) throw new InvalidDataException(pathProblem);
            }
            var xr=ResolveXr();if(xr.error!=null)throw new InvalidDataException("XR_RUNTIME:"+xr.error);
            if(v.embedded_release){var problem=XrRuntimes.NegotiationProblem(xr);if(problem!=null)throw new InvalidDataException("XR_RUNTIME:"+problem);}
            if(v.embedded_release)RequireInstalledEmbedded(v);
            bool compatibility=v.id==EmbeddedRelease.VersionId&&v.embedded_release;
            var diagnostics=new List<string>();
            var omitted=new List<string>(options.OmittedMods);
            var inputs=compatibility?CompatibleMods(p,v,options,diagnostics,omitted):ActiveMods(p,v);
            var graphics=compatibility?CheckGraphics(p,v,options):null;
            if(graphics!=null&&graphics.Problem!=null) diagnostics.Add(graphics.Problem);
            if(diagnostics.Count!=0)
                throw new OptionalComponentException(diagnostics,new PlayOptions { OmittedMods=omitted,
                    GraphicsFallback=graphics==null?null:graphics.Fallback });
            var prepared=BuildSelection(p,v,inputs,progress);
            var mods=prepared.mods;
            var settingsXml=prepared.settings_xml;
            var r=new SessionRecord{schema="gtavxr-suite2-session-v1",id=DateTime.UtcNow.ToString("yyyyMMddTHHmmss")+"-"+Ids.New().Substring(0,12),profile=p.id,profile_name=p.name,version=v.id,
                mods=mods.Select(m=>m.id).ToList(),phase="STARTING",started_utc=DateTime.UtcNow.ToString("o"),
                session_mode=v.launch.session_mode,settings_mode=v.launch.settings_mode,runtime_manifest=xr.manifest,runtime_name=xr.name,
                omitted_mods=omitted.Count==0?null:omitted,isolated_mod_state=omitted.Any(id=>{var mod=lib.Mod(id);return mod==null||mod.kind=="script";})};
            Directory.CreateDirectory(SessionDir(r.id));Save(r);
            if(p.test){
                // Test flight: write down the game folder first; the session only says "test" once that list exists.
                var st=layer.State();
                Json.Atomic(SafeFs.Under(SessionDir(r.id),"game-before.json"),TestFlight.Take(env.GameDir,st));
                r.test=true;r.prev_version=st==null?null:st.version;r.prev_mods=st==null||st.mods==null?new List<string>():new List<string>(st.mods);Save(r);
            }
            PlayLease lease=null;
            try{
                // Development host: hold play-runtime so agents know the user is playing. If an agent holds it, the user
                // plays anyway: its lease is left untouched and only noted in the session.
                if(env.DevHost){
                    try{lease=PlayLease.Acquire(env.DevRepo,r.id,"GTAVXR Suite: "+p.name+" ("+v.Label()+")");r.lease=true;}
                    catch(LeaseBusyException e){r.lease_held_by=e.Holder.agent+": "+e.Holder.purpose;}
                    Save(r);
                }
                Step(progress,"game",0,0);
                // Script mods never go through the game layer: their files are placed in scripts/ for this session only.
                layer.Apply(v,mods.Where(m=>m.kind!="script").ToList(),(what,d,t)=>Step(progress,"game",d,t),!v.embedded_release);
                if(!v.embedded_release)MaterializeRuntime(v);
                if(v.replaces!=null&&v.replaces.Count>0){r.swaps=v.replaces;r.swapped=true;Save(r);SwapIn(r,progress);}
                r.script_host=v.script_host=="switch"&&mods.Any(m=>m.kind=="script");
                SetAside(r);
                if(r.script_host)StageAsi(r,p,mods.Where(m=>m.kind=="script").ToList());
                if(r.script_host)StageScripts(r,p,v,mods.Where(m=>m.kind=="script").ToList());
                Advance(r,"GAME_READY");
                Step(progress,"saves",0,0);
                // 1. Keep the player's normal GTA user folder aside (verified copy).
                string backup=SafeFs.Under(SessionDir(r.id),"retail-user");
                r.retail_user=UserTree.Snapshot(env.UserDataDir);UserTree.CopyInto(env.UserDataDir,r.retail_user,backup);UserTree.Verify(backup,r.retail_user);
                // the repository's VR settings file is kept aside only for versions that read it (suite mode never touches it)
                if(r.settings_mode!="suite"){
                    string vrBackup=SafeFs.Under(SessionDir(r.id),"vr-settings-before.ini");
                    r.vr_settings_was_present=File.Exists(env.LegacySettingsPath);
                    if(r.vr_settings_was_present)File.Copy(env.LegacySettingsPath,vrBackup);
                }else{r.vr_settings=SafeFs.Under(SessionDir(r.id),"vr-settings.ini");}
                Advance(r,"USER_SAVED");
                // 2. Put this profile's user folder and VR settings in place.
                StageProfile(p,v,backup,r.retail_user,settingsXml,r);
                if(r.script_host)StageOnlineMaps(r,mods.Where(m=>m.kind=="script"));
                if(compatibility) StageGraphics(r,graphics); else ChooseGraphics(r,v);
                r.components=Components(r);
                Advance(r,"USER_STAGED");
                // 3. Run the game.
                Step(progress,"launch",0,0);
                Launch(r,p,v,mods,progress);
                Advance(r,"EXITED");
                Step(progress,"capture",0,0);
                Capture(r,p);
                Advance(r,"CAPTURED");
                RestoreRetail(r);
                if(r.swapped){Step(progress,"capture",0,0);SwapOut(r);}
                PutBack(r);
                Advance(r,"RESTORED");
                if(r.test){Step(progress,"revert",0,0);Revert(r);Advance(r,"REVERTED");}
                r.result="played";r.ended_utc=DateTime.UtcNow.ToString("o");
                if(lease!=null){lease.Dispose();lease=null;}
                Advance(r,"CLOSED");
                return r;
            }catch(Exception e){
                r.error=e.GetType().Name+": "+e.Message;Save(r);
                // Before the game started nothing of the profile can have changed: put GTA back now.
                if(Rank(r.phase)<Rank("LAUNCHED")&&!r.launch_attempted&&!GameRunning()){
                    try{if(Rank(r.phase)>=Rank("USER_SAVED"))RestoreRetail(r);if(r.swapped)SwapOut(r);PutBack(r);if(r.test)Revert(r);r.result="cancelled";r.ended_utc=DateTime.UtcNow.ToString("o");
                        if(lease!=null){lease.Dispose();lease=null;}Advance(r,"CLOSED");}
                    catch(Exception inner){r.error+=" | restore: "+inner.Message;Save(r);}
                }
                throw;
            }finally{
                // A lease left by an unfinished session is released by Recover(), never silently here.
                if(lease!=null&&r.phase=="CLOSED")lease.Dispose();
            }
        }

        public List<ModManifest> ActiveMods(Profile p,VersionManifest v){
            var mods=new List<ModManifest>();
            foreach(string id in p.mods??new List<string>()){var m=lib.Mod(id);if(m!=null&&m.WorksWith(v))mods.Add(m);}
            // A script mod brings its game files (its companion content source) when the version serves a content profile.
            foreach(var sm in mods.Where(m=>m.kind=="script"&&m.companion!=null).ToList()){
                var c=lib.Mod(sm.companion);if(c!=null&&c.WorksWith(v)&&!mods.Any(x=>x.id==c.id))mods.Add(c);
            }
            // A version that cannot run without MVGA gets it even when the profile has it off.
            if(v.mvga=="required"&&!mods.Any(m=>m.kind=="mvga-native")){
                var mvga=lib.Mods().FirstOrDefault(m=>m.kind=="mvga-native"&&m.loadable);
                if(mvga==null)throw new InvalidDataException("MVGA_REQUIRED");
                mods.Add(mvga);
            }
            // The EXE serves ONE content profile (-gtavxrContent=<id>): two would be two merges of the same game files.
            if(mods.Count(m=>m.kind=="content-native")>1)throw new InvalidDataException("CONTENT_PROFILE_TWICE");
            return mods;
        }

        private void Advance(SessionRecord r,string phase){r.phase=phase;Save(r);}
        private static void Step(Action<PlayProgress> p,string step,int done,int total){if(p!=null)p(new PlayProgress{step=step,done=done,total=total});}

        // Bridge, trainer and collector live in the Suite (one folder per version); GTAVXR_BRIDGE_PATH points there.
        public string RuntimeDir(VersionManifest v){
            return v.embedded_release?RuntimeRoot(v):MaterializeRuntime(v);
        }
        private string RuntimeRoot(VersionManifest v){return SafeFs.Under(env.DataRoot,"runtime/"+v.id);}
        private string MaterializeRuntime(VersionManifest v){
            string d=RuntimeRoot(v);Directory.CreateDirectory(d);
            foreach(var f in v.runtime){string dest=SafeFs.Under(d,f.path);if(!File.Exists(dest)||new FileInfo(dest).Length!=f.bytes||!String.Equals(SafeFs.Hash(dest),f.sha256,StringComparison.OrdinalIgnoreCase))blobs.Materialize(f,dest);}
            return d;
        }

        // 2.4.0: the OpenXR runtime of this Play ("auto" = Windows' active runtime now; a chosen manifest otherwise).
        private XrRuntime ResolveXr(){
            var s=env.LoadSettings();
            return XrRuntimes.Resolve(s.openxr_runtime,env.XrActive!=null?env.XrActive():env.OpenXrManifest);
        }
        public static bool AnsiPath(string path){
            var enc=Encoding.GetEncoding(Encoding.Default.CodePage,EncoderFallback.ExceptionFallback,DecoderFallback.ExceptionFallback);
            try{return enc.GetString(enc.GetBytes(path))==path;}catch(Exception){return false;}
        }
        // 167-neural: the profile's [Graphics] choice decides whether the game gets the DLSS folder (Super Resolution / neural).
        // No usable folder = no variable: the chain stays off and the game plays native; the reason is kept in the session.
        private void ChooseGraphics(SessionRecord r,VersionManifest v){
            if(v.neural==null)return;
            string vr=r.settings_mode=="suite"?r.vr_settings:env.LegacySettingsPath;
            r.graphics=GtavxrSuite.Graphics.Read(vr!=null&&File.Exists(vr)?File.ReadAllText(vr):"");
            if(!r.graphics.NeedsDlss()){Save(r);return;}
            var s=env.LoadSettings();
            DlssFolder f=null;try{f=Dlss.Find(s.dlss_dir,env.Dir("dlss"),env.DlssSearchRoots!=null?env.DlssSearchRoots():new List<string>());}catch(Exception){f=null;}
            if(f==null)r.dlss_note="no_dlss_folder";
            else if(!Dlss.Usable(f))r.dlss_note="dlss_invalid:"+f.sr.error;
            else{r.dlss_dir=f.dir;if(r.graphics.neural&&!Dlss.NeuralUsable(f))r.dlss_note="no_neural_model";}
            Save(r);
        }
        // Every external component this Play depends on, as found now: runtime, DLSS models, OFXR layer, display driver.
        private List<ComponentRef> Components(SessionRecord r){
            var list=new List<ComponentRef>();
            Action<string,string,string> add=(kind,path,note)=>{
                try{
                    if(String.IsNullOrEmpty(path)||!File.Exists(path)){list.Add(new ComponentRef{kind=kind,path=path,note=note??"missing"});return;}
                    var fi=new FileInfo(path);string ver=null;try{ver=FileVersionInfo.GetVersionInfo(path).FileVersion;}catch(Exception){}
                    list.Add(new ComponentRef{kind=kind,path=fi.FullName,version=ver,bytes=fi.Length,sha256=fi.Length<=512L*1024*1024?SafeFs.Hash(path):null,note=note});
                }catch(Exception e){list.Add(new ComponentRef{kind=kind,path=path,note="unreadable: "+e.GetType().Name});}
            };
            add("openxr_manifest",r.runtime_manifest,r.runtime_name);
            var xr=XrRuntimes.Inspect(r.runtime_manifest,null);if(xr.library!=null)add("openxr_runtime",xr.library,r.runtime_name);
            if(r.dlss_dir!=null){add("dlss_sr",Path.Combine(r.dlss_dir,Dlss.SrModel),null);string nr=Path.Combine(r.dlss_dir,Dlss.NrModel);if(File.Exists(nr))add("dlss_nr",nr,null);}
            try{string layer=env.OfxrLayer!=null?env.OfxrLayer():null;if(layer!=null)add("ofxr_layer",layer,null);}catch(Exception){}
            try{
                using(var k=Microsoft.Win32.Registry.LocalMachine.OpenSubKey(@"SYSTEM\CurrentControlSet\Control\Class\{4d36e968-e325-11ce-bfc1-08002be10318}\0000")){
                    if(k!=null&&!env.Fixture)list.Add(new ComponentRef{kind="display_driver",version=k.GetValue("DriverVersion") as string,note=k.GetValue("DriverDesc") as string});
                }
            }catch(Exception){}
            return list;
        }

        private void StageProfile(Profile p,VersionManifest v,string retailBackup,List<SaveFileRef> retail,Dictionary<string,string> settingsXml,SessionRecord r){
            string user=lib.UserDir(p.id);
            var files=p.user??new List<SaveFileRef>();
            UserTree.Verify(user,files);
            // Make the GTA user folder hold exactly the profile's files.
            UserTree.Mirror(user,files,env.UserDataDir);
            // GTAVXR needs a windowed 1920x1080 monitor view; take the profile's settings.xml, or the normal one.
            string settings=SafeFs.Under(env.UserDataDir,"settings.xml");
            string source=File.Exists(settings)?settings:(retail.Any(f=>f.relative=="settings.xml")?SafeFs.Under(retailBackup,"settings.xml"):null);
            if(source!=null){
                string xml=Display.Convert(File.ReadAllText(source));
                if(v.content=="profile")xml=ContentSources.ApplySettings(xml,settingsXml);   // World of Variety's variety option
                File.WriteAllText(settings,xml,new UTF8Encoding(false));
            }
            // VR settings of this profile: versions 125-167 read one fixed file on this PC; a suite-mode version reads this
            // session's copy (GTAVXR_VR_SETTINGS), so nothing outside the Suite's own folder is written.
            string vr=r.settings_mode=="suite"?r.vr_settings:env.LegacySettingsPath;Directory.CreateDirectory(Path.GetDirectoryName(vr));
            string mine=lib.VrSettingsFile(p.id);
            if(p.has_vr_settings&&File.Exists(mine))File.Copy(mine,vr,true);
            else if(v.vr_settings_seed!=null)blobs.Materialize(v.vr_settings_seed,vr);
            else if(File.Exists(vr))File.Delete(vr);
        }

        // 2.4.2: a script mod that needs the Online map data (Open World Interiors lists "Enable MP Maps") gets "Online maps in
        // Story" on in this session's VR settings; the rest of the file stays as staged. The session end copies the file back,
        // so the profile keeps it on (the World switch shows it locked while such a mod is in the profile).
        private void StageOnlineMaps(SessionRecord r,IEnumerable<ModManifest> scripts){
            var who=Content.ContentCatalog.NeedsOnlineMaps(scripts);if(who.Count==0)return;
            string vr=r.settings_mode=="suite"?r.vr_settings:env.LegacySettingsPath;
            string text=File.Exists(vr)?File.ReadAllText(vr):"";
            if(VrIni.Get(text,"mods_mp_maps166")!="1")File.WriteAllText(vr,VrIni.Set(text,"mods_mp_maps166","1"),new UTF8Encoding(false));
            r.online_maps_for=who;Save(r);
        }

        private void Launch(SessionRecord r,Profile p,VersionManifest v,List<ModManifest> mods,Action<PlayProgress> progress){
            string game=SafeFs.Root(env.GameDir),exe=SafeFs.Under(game,v.launch.exe);
            string native;
            if(v.launch.session_mode=="legacy-repo"){native=Path.Combine(env.LegacySessionsRoot,r.id);Directory.CreateDirectory(native);}
            else native=SafeFs.Under(SessionDir(r.id),"native");
            Directory.CreateDirectory(native);r.native_session=native;Save(r);
            var args=new List<string>();
            foreach(string a in v.launch.args)args.Add(a=="{LOGFILE}"?Path.Combine(native,"native-runtime.log"):a);
            foreach(var m in mods)if(m.args!=null)foreach(string a in m.args)if(!args.Contains(a))args.Add(a);
            if(r.script_host&&!args.Contains(v.script_host_arg))args.Add(v.script_host_arg);
            if(mods.Any(m=>m.kind=="content-native")&&!args.Contains(ContentSources.HeapArg))args.Add(ContentSources.HeapArg);
            var psi=new ProcessStartInfo(exe,ProcessText.Join(args)){UseShellExecute=false,WorkingDirectory=game,CreateNoWindow=true,RedirectStandardOutput=true,RedirectStandardError=true};
            var envv=psi.EnvironmentVariables;
            foreach(string k in new[]{"GX_DEV_ROOT","SIMXR_DATA_DIR","GTAVXR_DIAGNOSTIC","GTAVXR_SCRIPTS_PATH","GTAVXR_LOADER_PATH","GTAVXR_VR_SETTINGS","GTAVXR_SESSIONS_ROOT","GTAVXR_DLSS_DIR"})
                if(envv.ContainsKey(k))envv.Remove(k);
            envv["XR_RUNTIME_JSON"]=r.runtime_manifest??env.OpenXrManifest;envv["GTAVXR_USER_PLAY"]="1";envv["GTAVXR_SESSION_DIR"]=native;
            envv["GTAVXR_BRIDGE_PATH"]=Path.Combine(RuntimeDir(v),"gtavxr_native_openxr.dll");
            // 2.4.0 (167-neural A1): a suite-mode EXE gets every user-play path from here (never a developer folder)
            if(r.session_mode=="suite"){
                envv["GTAVXR_SESSIONS_ROOT"]=env.Dir("sessions");envv["GTAVXR_VR_SETTINGS"]=r.vr_settings;
                envv["GTAVXR_LOADER_PATH"]=Path.Combine(RuntimeDir(v),"openxr_loader.dll");
            }
            if(v.neural!=null&&r.dlss_dir!=null)envv[String.IsNullOrEmpty(v.neural.dlss_dir_env)?"GTAVXR_DLSS_DIR":v.neural.dlss_dir_env]=r.dlss_dir;
            // the script host loads GTAVXRHost.dll beside the bridge and ScriptHookVDotNet.asi from this folder
            if(r.script_host)envv["GTAVXR_SCRIPTS_PATH"]=r.scripts_host;
            // 2.4.4: the .asi mods this session placed in the game folder; GTAVXRHost.dll loads them after its own folder's
            if(r.script_host&&r.asi_placed!=null){
                string gameRoot=SafeFs.Root(env.GameDir);
                var asi=r.asi_placed.Where(f=>f.path.EndsWith(".asi",StringComparison.OrdinalIgnoreCase)&&!f.path.Contains("/")).Select(f=>SafeFs.Under(gameRoot,f.path)).ToList();
                if(asi.Count>0)envv["GTAVXR_ASI_FILES"]=String.Join("|",asi);
            }
            foreach(string layerVar in v.launch.disable_layers??new List<string>())envv[layerVar]="1";
            File.WriteAllText(Path.Combine(SessionDir(r.id),"launch.txt"),"\""+exe+"\" "+psi.Arguments+Environment.NewLine,new UTF8Encoding(false));
            Process child=null;NativeFaultCompanion collector=null;string marker=null,exitFile=null;
            var outLog=new StreamWriter(Path.Combine(native,"native-stdout.log"),false,new UTF8Encoding(false));
            var errLog=new StreamWriter(Path.Combine(native,"native-stderr.log"),false,new UTF8Encoding(false));
            try{
                r.launch_staging_sha256=StagedDataIdentity(r);
                r.launch_attempted=true;Save(r);
                try{
                    child=Starter(psi);if(child==null)throw new IOException("The game did not start");
                }catch{
                    r.launch_attempted=false;Save(r);throw;
                }
                lock(gate){running=child;current=r;}
                child.OutputDataReceived+=(s,e)=>{if(e.Data!=null)lock(outLog)outLog.WriteLine(e.Data);};
                child.ErrorDataReceived+=(s,e)=>{if(e.Data!=null)lock(errLog)errLog.WriteLine(e.Data);};
                child.BeginOutputReadLine();child.BeginErrorReadLine();
                r.game=ProcessOwnership.Capture(child,exe);r.launched_utc=DateTime.UtcNow.ToString("o");Advance(r,"LAUNCHED");
                Step(progress,"playing",0,0);
                if(AfterLaunched!=null)AfterLaunched(child);
                // Crash collector only for development versions (normal play adds no debugger).
                string monitor=Path.Combine(RuntimeDir(v),"native_fault_monitor.exe");
                if(v.channel=="test"&&File.Exists(monitor)&&!env.Fixture){
                    try{string cdir=SafeFs.Under(SessionDir(r.id),"collector");Directory.CreateDirectory(cdir);collector=new NativeFaultCompanion(monitor,cdir);collector.AfterStart(child,exe);r.collector=true;Save(r);}catch(Exception){collector=null;}
                }
                // Rockstar launcher handshake: with in-<pid> present GTA writes out-<pid> right before its final ExitProcess(0),
                // after the network shutdown and every unfinished save (game/core/app.cpp:1709-1729, the only WriteExitFile call).
                try{
                    Directory.CreateDirectory(env.ExitMarkerDir);
                    marker=Path.Combine(env.ExitMarkerDir,"in-"+child.Id);exitFile=Path.Combine(env.ExitMarkerDir,"out-"+child.Id);
                    if(File.Exists(exitFile))File.Delete(exitFile);
                    File.WriteAllText(marker,"",new UTF8Encoding(false));r.exit_marker=true;Save(r);
                }catch(Exception){marker=null;exitFile=null;}
                WaitForGame(r,child,exitFile);
                r.exit_code=child.ExitCode;
                CloseResidual(r,exe);
                if(collector!=null)collector.AfterExit(child);
            }finally{
                lock(gate){running=null;current=null;}
                foreach(string f in new[]{marker,exitFile})if(f!=null)try{if(File.Exists(f))File.Delete(f);}catch(Exception){}
                if(collector!=null)collector.Dispose();
                if(child!=null)child.Dispose();
                lock(outLog)outLog.Dispose();lock(errLog)errLog.Dispose();
            }
            if(GameRunning())throw new IOException("GAME_STILL_RUNNING");
        }

        // The game is done when its process ends. A game that already wrote out-<pid> (saves done, ExitProcess next) but stays alive
        // for ExitGraceMs is stuck in its own exit (worn sessions 2026-10-20 needed Task Manager every time): its thread stacks are
        // kept and that exact process is ended. Without out-<pid> nothing is ended here (saves may still be running): the
        // "Forzar cierre" button stays the way out.
        private void WaitForGame(SessionRecord r,Process child,string exitFile){
            DateTime? seen=null;bool ended=false;
            while(!child.WaitForExit(500)){
                if(ended||exitFile==null||!File.Exists(exitFile))continue;
                if(seen==null){seen=DateTime.UtcNow;r.exit_file_seen=true;continue;}
                if((DateTime.UtcNow-seen.Value).TotalMilliseconds<env.ExitGraceMs)continue;
                lock(gate){
                    try{
                        if(child.HasExited)break;
                        r.exit_hang="closed-after-exit-file";r.exit_dump=DumpThreads(child,SafeFs.Under(SessionDir(r.id),"exit-hang.dmp"));
                        child.Kill();ended=true;
                    }catch(InvalidOperationException){}
                }
            }
            // Completes the redirected output readers.
            child.WaitForExit();
        }

        [System.Runtime.InteropServices.DllImport("dbghelp.dll",SetLastError=true)]
        private static extern bool MiniDumpWriteDump(IntPtr process,uint pid,Microsoft.Win32.SafeHandles.SafeFileHandle file,uint type,IntPtr exception,IntPtr user,IntPtr callback);
        // Thread stacks + module list only (MiniDumpNormal|WithThreadInfo|WithUnloadedModules): a few MB, never the game memory.
        // Skipped when the disk is nearly full (less than the Suite's 1 GiB reserve + 256 MiB). Returns the dump path, or null.
        private string DumpThreads(Process p,string path){
            try{
                var drive=new DriveInfo(Path.GetPathRoot(Path.GetFullPath(path)));
                if(drive.AvailableFreeSpace<SafeFs.Reserve+268435456L)return null;
                if(File.Exists(path))return null;
                using(var fs=new FileStream(path,FileMode.CreateNew,FileAccess.Write,FileShare.Read)){
                    if(!MiniDumpWriteDump(p.Handle,(uint)p.Id,fs.SafeFileHandle,0x1020u,IntPtr.Zero,IntPtr.Zero,IntPtr.Zero))return null;
                }
                return path;
            }catch(Exception){return null;}
        }

        // The source build can leave exactly one thread of the same process after its window closes.
        // Only that exact process (same pid, start time and image, one thread) is ended.
        private void CloseResidual(SessionRecord r,string exe){
            Process p;
            try{p=Process.GetProcessById(r.game.pid);}catch(ArgumentException){return;}
            using(p){
                try{
                    if(p.HasExited)return;
                    if(p.StartTime.ToUniversalTime().Ticks!=r.game.creation_utc_ticks)return;
                    var image=p.MainModule==null?null:p.MainModule.FileName;
                    if(!String.Equals(image,exe,StringComparison.OrdinalIgnoreCase)||p.Threads.Count!=1)throw new IOException("GAME_EXIT_INCOMPLETE");
                    p.Kill();if(!p.WaitForExit(10000))throw new IOException("GAME_EXIT_INCOMPLETE");
                    r.residual_closed=true;Save(r);
                }catch(InvalidOperationException){if(!p.HasExited)throw;}
            }
        }

        private void Capture(SessionRecord r,Profile p){
            // Snapshot what the game left, keep the previous profile copy as history, then store the new one.
            if(r.script_host)CaptureScripts(r,p);
            if(r.asi_staged)CaptureAsi(r,p);
            string user=lib.UserDir(p.id);
            var now=UserTree.Snapshot(env.UserDataDir);
            // The profile as it was before this session (saves + VR settings): "Deshacer la partida de esta prueba".
            // Written once and atomically, so a Capture repeated by Recover never copies half-updated files.
            if(lib.ReadCopy(p.id,"history/"+r.id)==null)lib.KeepCopy(p,"history/"+r.id,r.id);
            UserTree.Mirror(env.UserDataDir,now,user);UserTree.Verify(user,now);
            p=lib.Profile(p.id);p.user=now;
            string vrNow=r.settings_mode=="suite"?r.vr_settings:env.LegacySettingsPath;
            if(vrNow!=null&&File.Exists(vrNow)){
                string vrTarget=lib.VrSettingsFile(p.id);
                if(File.Exists(SafeFs.Under(SessionDir(r.id),"before-graphics-fallback.ini")))
                    File.WriteAllText(vrTarget,RestoreGraphicsChoice(r,File.ReadAllText(vrNow)),new UTF8Encoding(false));
                else File.Copy(vrNow,vrTarget,true);
                p.has_vr_settings=true;
            }
            p.last_played_utc=DateTime.UtcNow.ToString("o");
            DateTime start;if(r.launched_utc!=null&&DateTime.TryParse(r.launched_utc,null,System.Globalization.DateTimeStyles.RoundtripKind,out start))p.played_seconds+=(long)Math.Max(0,(DateTime.UtcNow-start).TotalSeconds);
            lib.SaveProfile(p);
            lib.PruneHistory(p.id,6);
        }

        // A crash can leave no process receipt even though staging already completed.
        private string StagedDataIdentity(SessionRecord r){
            string game=SafeFs.Root(env.GameDir);
            string vr=r.settings_mode=="suite"?r.vr_settings:env.LegacySettingsPath;
            var asi=r.asi_staged?ClaimAsi(r,game).OrderBy(path=>path,StringComparer.OrdinalIgnoreCase)
                .Select(path=>new { path, sha256=HashIfPresent(SafeFs.Under(game,path)) }).ToArray():null;
            var state=new {
                user=UserTree.Snapshot(env.UserDataDir),
                vr=HashIfPresent(vr),
                scripts=r.scripts_staged?UserTree.Snapshot(SafeFs.Under(game,"scripts")):null,
                asi
            };
            using(var sha=SHA256.Create())
                return BitConverter.ToString(sha.ComputeHash(Encoding.UTF8.GetBytes(Json.Encode(state)))).Replace("-","");
        }

        static string HashIfPresent(string path){
            if(path==null||!File.Exists(path)) return null;
            SafeFs.NoLinks(path);
            return SafeFs.Hash(path);
        }

        // Test flight landing: GTAVXR back to the version + mods installed before the test, new files in the game
        // folder aside (quarantine), changed game files and the retail check reported. Safe to repeat.
        private void Revert(SessionRecord r){
            string q=SafeFs.Under(env.Dir("quarantine"),r.id);
            if(layer.Pending)layer.Recover(null);
            var prev=r.prev_version==null?null:lib.Version(r.prev_version);
            var mods=new List<ModManifest>();
            if(prev!=null)foreach(string id in r.prev_mods??new List<string>()){var m=lib.Mod(id);if(m!=null)mods.Add(m);}
            layer.Apply(prev,mods,null,false,q);
            // Files kept across the switch are not rewritten: check them all, put changed ones aside and restore them.
            foreach(string bad in layer.Verify()){string full=SafeFs.Under(env.GameDir,bad);if(File.Exists(full))GameLayer.Quarantine(full,q,bad);}
            layer.Repair();
            var before=Json.Read<GameInventory>(SafeFs.Under(SessionDir(r.id),"game-before.json"));
            var verified=r.swaps==null?new List<string>():r.swaps.Select(f=>f.path).ToList();
            var rep=TestFlight.Settle(before,layer.State(),verified,q);
            r.quarantined=Directory.Exists(q)?Directory.GetFiles(q,"*",SearchOption.AllDirectories).Select(f=>f.Substring(q.TrimEnd('\\').Length+1).Replace('\\','/')).OrderBy(x=>x).ToList():new List<string>();
            r.changed_game_files=rep.changed;
            r.missing_game_files=rep.missing;
            var retail=env.RetailOverride??(prev??lib.Version(r.version)??new VersionManifest()).retail;
            if(retail!=null){try{LegacyGamePolicy.VerifyFiles(env.GameDir,retail);r.retail_check="ok";}catch(Exception){r.retail_check="changed";}}
            Save(r);
        }

        private void RestoreRetail(SessionRecord r){
            string backup=SafeFs.Under(SessionDir(r.id),"retail-user");
            if(r.retail_user!=null){UserTree.Verify(backup,r.retail_user);UserTree.Mirror(backup,r.retail_user,env.UserDataDir);}
            if(r.settings_mode=="suite")return;   // the session's own copy: nothing outside the Suite to put back
            string vrBackup=SafeFs.Under(SessionDir(r.id),"vr-settings-before.ini");
            if(r.vr_settings_was_present){if(File.Exists(vrBackup))File.Copy(vrBackup,env.LegacySettingsPath,true);}
            else if(File.Exists(env.LegacySettingsPath))File.Delete(env.LegacySettingsPath);
        }

        // Older builds swap game files only while playing. Originals are kept in the blob store the first time.
        private void SwapIn(SessionRecord r,Action<PlayProgress> progress){
            string game=SafeFs.Root(env.GameDir);int i=0;
            foreach(var f in r.swaps){
                string dest=SafeFs.Under(game,f.path);
                if(!File.Exists(dest))throw new GameConflictException("Falta un archivo del juego: "+f.path);
                string h=SafeFs.Hash(dest);
                if(!String.Equals(h,f.sha256,StringComparison.OrdinalIgnoreCase)){
                    if(!String.Equals(h,f.original_sha256,StringComparison.OrdinalIgnoreCase))throw new GameConflictException("Este archivo del juego no es el original que espera esta versión: "+f.path);
                    if(!blobs.Has(f.original_sha256,f.original_bytes))blobs.Put(dest,f.original_sha256,f.original_bytes);
                    blobs.Materialize(f,dest);
                }
                Step(progress,"game",++i,r.swaps.Count);
            }
        }
        private void SwapOut(SessionRecord r){
            if(r.swaps==null||!r.swapped)return;
            string game=SafeFs.Root(env.GameDir);
            foreach(var f in r.swaps){
                string dest=SafeFs.Under(game,f.path);string h=File.Exists(dest)?SafeFs.Hash(dest):null;
                if(String.Equals(h,f.original_sha256,StringComparison.OrdinalIgnoreCase))continue;
                if(h!=null&&!String.Equals(h,f.sha256,StringComparison.OrdinalIgnoreCase))throw new GameConflictException("Un archivo del juego cambió durante la partida: "+f.path);
                blobs.Materialize(new FileRef{path=f.path,sha256=f.original_sha256,bytes=f.original_bytes},dest);
            }
            r.swapped=false;Save(r);
        }

        // Store-game mods GTAVXR's EXE cannot run (RetailPlugins) wait in the session folder while it plays. The list is written
        // before the first move, so Recover puts back whatever moved.
        private void SetAside(SessionRecord r){
            string game=SafeFs.Root(env.GameDir);var st=layer.State();
            var owned=st==null||st.files==null?new List<string>():st.files.Select(f=>f.path).ToList();
            var names=RetailPlugins.Incompatible(game,owned,r.script_host);if(names.Count==0)return;
            r.aside=names.Select(n=>{string src=SafeFs.Under(game,n);return new FileRef{path=n,sha256=SafeFs.Hash(src),bytes=new FileInfo(src).Length};}).ToList();
            r.aside_moved=true;Save(r);
            string dir=SafeFs.Under(SessionDir(r.id),"aside");Directory.CreateDirectory(dir);
            foreach(var f in r.aside)File.Move(SafeFs.Under(game,f.path),SafeFs.Under(dir,f.path));
        }
        private void PutBack(SessionRecord r){
            // every way out of a session puts things back here: the .asi mods' files, then the player's scripts/
            AsiOut(r);
            ScriptsOut(r);
            if(r.aside==null||!r.aside_moved)return;
            string game=SafeFs.Root(env.GameDir),dir=SafeFs.Under(SessionDir(r.id),"aside");var kept=new List<string>();
            foreach(var f in r.aside){
                string src=SafeFs.Under(dir,f.path),dest=SafeFs.Under(game,f.path);
                if(!File.Exists(src))continue;                    // not moved yet when the Suite stopped, or already back
                if(File.Exists(dest)){kept.Add(f.path);continue;}  // a new file took the name: never overwrite, the player's file stays here
                File.Move(src,dest);
            }
            r.aside_kept=kept;r.aside_moved=false;Save(r);
        }

        // Script host session: the game's scripts/ holds exactly this profile's script mods and what they wrote in earlier sessions
        // (Library.ScriptsDir); the player's own scripts/ (store-game mods ScriptHookVDotNet would also load) waits beside it under
        // another name. Each step is written before it is done.
        private void StageScripts(SessionRecord r,Profile p,VersionManifest v,List<ModManifest> scripts){
            string game=SafeFs.Root(env.GameDir),live=SafeFs.Under(game,"scripts");
            var sm=new ScriptMods(env);var placed=new List<FileRef>();
            foreach(var m in scripts)foreach(var f in sm.Placed(m,p)){
                if(placed.Any(x=>String.Equals(x.path,f.path,StringComparison.OrdinalIgnoreCase)))throw new InvalidDataException("SCRIPT_FILE_TWICE: "+f.path);
                placed.Add(f);
            }
            r.scripts_placed=placed;r.scripts_aside="scripts.gtavxr-"+r.id;r.scripts_retail_present=Directory.Exists(live);Save(r);
            if(r.scripts_retail_present)Directory.Move(live,SafeFs.Under(game,r.scripts_aside));
            r.scripts_staged=true;Save(r);
            Directory.CreateDirectory(live);
            string mine=lib.ScriptsDir(p.id);
            var paths=new HashSet<string>(placed.Select(f=>f.path),StringComparer.OrdinalIgnoreCase);
            foreach(string f in Directory.GetFiles(mine,"*",SearchOption.AllDirectories)){
                string relative=f.Substring(mine.TrimEnd('\\').Length+1).Replace('\\','/');
                if(r.isolated_mod_state&&!KnownModData(relative,paths)) continue;
                string d=SafeFs.Under(live,relative);
                Directory.CreateDirectory(Path.GetDirectoryName(d));File.Copy(f,d,true);File.SetAttributes(d,FileAttributes.Normal);
            }
            foreach(var f in placed)blobs.Materialize(f,SafeFs.Under(live,f.path));
            // The host folder: GTAVXR's ScriptHookVDotNet build (the version's runtime scripthost/), one copy per session so its log
            // stays with the session.
            string host=SafeFs.Under(SessionDir(r.id),"scripthost");Directory.CreateDirectory(host);string rt=RuntimeDir(v);
            foreach(var f in v.runtime.Where(x=>x.path.StartsWith("scripthost/",StringComparison.OrdinalIgnoreCase)))
                File.Copy(SafeFs.Under(rt,f.path),SafeFs.Under(host,f.path.Substring("scripthost/".Length)),true);
            if(!File.Exists(Path.Combine(host,"ScriptHookVDotNet.asi")))throw new InvalidDataException("SCRIPT_HOST_MISSING");
            r.scripts_host=host;Save(r);
        }
        // 2.4.4: ScriptHookV .asi mods in the game folder for this session, laid out as the store game has them: the .asi at the top
        // (a copy that imports GTAVXRHost.dll) with its own settings / data files and folders beside it. What the mods wrote there in
        // earlier sessions (the profile's asi/ folder) replaces their original file or comes beside it. A game file of GTAVXR's
        // with the same name refuses the session; a player's own file waits in the session folder and comes back at the end.
        // Every step is written before it is done.
        private void StageAsi(SessionRecord r,Profile p,List<ModManifest> scripts){
            var mods=scripts.Where(m=>m.HasAsi()).ToList();if(mods.Count==0)return;
            string game=SafeFs.Root(env.GameDir);
            var original=new List<FileRef>();
            foreach(var m in mods)foreach(var f in m.asi_files){
                if(original.Any(x=>String.Equals(x.path,f.path,StringComparison.OrdinalIgnoreCase)))throw new InvalidDataException("ASI_FILE_TWICE: "+f.path);
                original.Add(f);
            }
            string mine=lib.AsiDir(p.id);
            var saved=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);
            var paths=new HashSet<string>(original.Select(f=>f.path),StringComparer.OrdinalIgnoreCase);
            foreach(string f in Directory.GetFiles(mine,"*",SearchOption.AllDirectories)){
                string relative=f.Substring(mine.TrimEnd('\\').Length+1).Replace('\\','/');
                if(!r.isolated_mod_state||KnownModData(relative,paths)) saved[relative]=f;
            }
            var placed=new List<FileRef>();
            foreach(var f in original)placed.Add(saved.ContainsKey(f.path)?new FileRef{path=f.path,sha256=SafeFs.Hash(saved[f.path]),bytes=new FileInfo(saved[f.path]).Length}:f);
            foreach(var kv in saved)if(!original.Any(x=>String.Equals(x.path,kv.Key,StringComparison.OrdinalIgnoreCase)))
                placed.Add(new FileRef{path=kv.Key,sha256=SafeFs.Hash(kv.Value),bytes=new FileInfo(kv.Value).Length});
            var st=layer.State();
            var gtavxr=new HashSet<string>(st==null||st.files==null?new string[0]:st.files.Select(f=>f.path),StringComparer.OrdinalIgnoreCase);
            var aside=new List<FileRef>();var dirs=new List<string>();var before=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach(string f in Directory.GetFiles(game,"*",SearchOption.TopDirectoryOnly))before.Add(Path.GetFileName(f));
            foreach(var f in placed){
                string dest=SafeFs.Under(game,f.path);
                if(gtavxr.Contains(f.path))throw new GameConflictException("Un mod .asi trae un archivo que es de GTAVXR: "+f.path);
                if(Directory.Exists(dest))throw new GameConflictException("Hay una carpeta donde un mod .asi necesita un archivo: "+f.path);
                if(File.Exists(dest))aside.Add(new FileRef{path=f.path,sha256=SafeFs.Hash(dest),bytes=new FileInfo(dest).Length});
                // the folders it goes into: created now (removed at the end) or already there (their files written down)
                var parts=f.path.Split('/');string rel="";
                for(int i=0;i<parts.Length-1;i++){
                    rel=rel.Length==0?parts[i]:rel+"/"+parts[i];
                    string d=SafeFs.Under(game,rel);
                    if(File.Exists(d))throw new GameConflictException("Hay un archivo donde un mod .asi necesita una carpeta: "+rel);
                    if(!Directory.Exists(d)){if(!dirs.Contains(rel,StringComparer.OrdinalIgnoreCase))dirs.Add(rel);}
                    else if(i==0&&!dirs.Contains(rel,StringComparer.OrdinalIgnoreCase))
                        foreach(string x in Directory.GetFiles(d,"*",SearchOption.AllDirectories))before.Add(x.Substring(game.TrimEnd('\\').Length+1).Replace('\\','/'));
                }
            }
            r.asi_original=original;r.asi_placed=placed;r.asi_aside=aside;r.asi_dirs=dirs;r.asi_before=before.OrderBy(x=>x,StringComparer.OrdinalIgnoreCase).ToList();
            r.asi_staged=true;Save(r);
            string keep=SafeFs.Under(SessionDir(r.id),"asi-aside");
            foreach(var f in aside){string d=SafeFs.Under(keep,f.path);Directory.CreateDirectory(Path.GetDirectoryName(d));File.SetAttributes(SafeFs.Under(game,f.path),FileAttributes.Normal);File.Move(SafeFs.Under(game,f.path),d);}
            foreach(string d in dirs)Directory.CreateDirectory(SafeFs.Under(game,d));
            foreach(var f in placed){
                string dest=SafeFs.Under(game,f.path);
                if(saved.ContainsKey(f.path))File.Copy(saved[f.path],dest,true);else blobs.Materialize(f,dest);
                File.SetAttributes(dest,FileAttributes.Normal);
            }
        }
        // What the .asi mods wrote goes to the profile's asi/ folder: their own files when changed, new files in the folders they
        // placed into, and new top-level files named after one of their .asi (MyMod.log, MyMod.ini). Anything else new in the game
        // folder is not theirs for sure: it stays where it is (listed in the session). Repeatable until AsiOut.
        private void CaptureAsi(SessionRecord r,Profile p){
            if(!r.asi_staged||r.asi_done||r.asi_placed==null)return;
            string game=SafeFs.Root(env.GameDir);
            var claimed=ClaimAsi(r,game);
            var orig=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);
            foreach(var f in r.asi_original??new List<FileRef>())orig[f.path]=f.sha256;
            string dest=r.isolated_mod_state?SafeFs.Under(SessionDir(r.id),"optional-capture/asi"):lib.AsiDir(p.id),tmp=dest+".new";
            if(Directory.Exists(tmp))Directory.Delete(tmp,true);
            Directory.CreateDirectory(tmp);
            foreach(string rel in claimed){
                string src=SafeFs.Under(game,rel);if(!File.Exists(src))continue;string sha;
                if(orig.TryGetValue(rel,out sha)&&String.Equals(SafeFs.Hash(src),sha,StringComparison.OrdinalIgnoreCase))continue;
                string d=SafeFs.Under(tmp,rel);Directory.CreateDirectory(Path.GetDirectoryName(d));File.Copy(src,d);File.SetAttributes(d,FileAttributes.Normal);
            }
            if(Directory.Exists(dest))Directory.Delete(dest,true);
            Directory.Move(tmp,dest);
            r.asi_claimed=claimed;Save(r);
        }
        // The game-folder files that are the .asi mods' this session: what it placed, and what appeared since in the folders it placed
        // into or at the top under one of their .asi names.
        private List<string> ClaimAsi(SessionRecord r,string game){
            var before=new HashSet<string>(r.asi_before??new List<string>(),StringComparer.OrdinalIgnoreCase);
            var list=new List<string>();Action<string> add=x=>{if(!list.Contains(x,StringComparer.OrdinalIgnoreCase))list.Add(x);};
            foreach(var f in r.asi_placed)add(f.path);
            var tops=new HashSet<string>(r.asi_placed.Where(f=>f.path.Contains("/")).Select(f=>f.path.Substring(0,f.path.IndexOf('/'))),StringComparer.OrdinalIgnoreCase);
            foreach(string top in tops){
                string d=SafeFs.Under(game,top);if(!Directory.Exists(d))continue;
                foreach(string x in Directory.GetFiles(d,"*",SearchOption.AllDirectories)){string rel=x.Substring(game.TrimEnd('\\').Length+1).Replace('\\','/');if(!before.Contains(rel))add(rel);}
            }
            var names=r.asi_placed.Where(f=>!f.path.Contains("/")&&f.path.EndsWith(".asi",StringComparison.OrdinalIgnoreCase)).Select(f=>Path.GetFileNameWithoutExtension(f.path)).ToList();
            foreach(string x in Directory.GetFiles(game,"*",SearchOption.TopDirectoryOnly)){
                string n=Path.GetFileName(x);if(before.Contains(n))continue;
                if(names.Any(a=>n.StartsWith(a+".",StringComparison.OrdinalIgnoreCase)||n.StartsWith(a+"_",StringComparison.OrdinalIgnoreCase)))add(n);
            }
            return list;
        }
        // The .asi mods' files out of the game folder (after the capture), the folders the session created removed when empty, the
        // player's own files back.
        private void AsiOut(SessionRecord r){
            if(!r.asi_staged||r.asi_done)return;
            string game=SafeFs.Root(env.GameDir);
            var take=r.asi_claimed??r.asi_placed.Select(f=>f.path).ToList();
            foreach(string rel in take){string f=SafeFs.Under(game,rel);if(File.Exists(f)){File.SetAttributes(f,FileAttributes.Normal);File.Delete(f);}}
            foreach(string d in (r.asi_dirs??new List<string>()).OrderByDescending(x=>x.Count(c=>c=='/')).ThenByDescending(x=>x,StringComparer.OrdinalIgnoreCase)){
                string full=SafeFs.Under(game,d);
                if(Directory.Exists(full)&&!Directory.EnumerateFileSystemEntries(full).Any())Directory.Delete(full);
            }
            string keep=SafeFs.Under(SessionDir(r.id),"asi-aside");var kept=new List<string>();
            foreach(var f in r.asi_aside??new List<FileRef>()){
                string src=SafeFs.Under(keep,f.path),dest=SafeFs.Under(game,f.path);
                if(!File.Exists(src))continue;
                if(File.Exists(dest)){kept.Add(f.path);continue;}
                Directory.CreateDirectory(Path.GetDirectoryName(dest));File.Move(src,dest);
            }
            r.asi_kept=kept;r.asi_done=true;Save(r);
        }
        // What the script mods wrote in scripts/ (settings, saved outfits) goes back to the profile; the mods' own files do not.
        // Repeatable: the game's scripts/ stays until ScriptsOut, after the capture.
        private void CaptureScripts(SessionRecord r,Profile p){
            if(!r.scripts_staged||r.scripts_done)return;
            string live=SafeFs.Under(SafeFs.Root(env.GameDir),"scripts");if(!Directory.Exists(live))return;
            var ours=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);
            foreach(var f in r.scripts_placed??new List<FileRef>())ours[f.path]=f.sha256;
            string dest=r.isolated_mod_state?SafeFs.Under(SessionDir(r.id),"optional-capture/scripts"):lib.ScriptsDir(p.id),tmp=dest+".new";
            if(Directory.Exists(tmp))Directory.Delete(tmp,true);
            Directory.CreateDirectory(tmp);
            foreach(string f in Directory.GetFiles(live,"*",SearchOption.AllDirectories)){
                string rel=f.Substring(live.TrimEnd('\\').Length+1).Replace('\\','/');string sha;
                if(ours.TryGetValue(rel,out sha)&&String.Equals(SafeFs.Hash(f),sha,StringComparison.OrdinalIgnoreCase))continue;
                string d=SafeFs.Under(tmp,rel);Directory.CreateDirectory(Path.GetDirectoryName(d));File.Copy(f,d);File.SetAttributes(d,FileAttributes.Normal);
            }
            if(Directory.Exists(dest))Directory.Delete(dest,true);
            Directory.Move(tmp,dest);
        }
        static bool KnownModData(string relative,HashSet<string> paths){
            return paths.Contains(relative)&&!new[]{".dll",".asi",".exe",".cs",".vb"}.Contains(Path.GetExtension(relative),StringComparer.OrdinalIgnoreCase);
        }
        // The player's scripts/ back. The one in the game is ours when the player's waits aside, or when the player had none and ours
        // was placed; a scripts/ the player had and that never moved is left alone.
        private void ScriptsOut(SessionRecord r){
            if(r.scripts_aside==null||r.scripts_done)return;
            string game=SafeFs.Root(env.GameDir),live=SafeFs.Under(game,"scripts"),aside=SafeFs.Under(game,r.scripts_aside);
            bool ours=Directory.Exists(aside)||(!r.scripts_retail_present&&r.scripts_staged);
            if(ours&&Directory.Exists(live)){
                foreach(string f in Directory.GetFiles(live,"*",SearchOption.AllDirectories))File.SetAttributes(f,FileAttributes.Normal);
                Directory.Delete(live,true);
            }
            if(Directory.Exists(aside))Directory.Move(aside,live);
            r.scripts_done=true;Save(r);
        }

        // Finish whatever an interrupted Suite left behind. Never closes the game: if it is running, wait.
        public string Recover(){
            if(GameRunning())return "GAME_RUNNING";
            layer.Recover(null);
            foreach(var r in Unfinished()){
                if(Rank(r.phase)<Rank("USER_SAVED")){if(r.swapped)SwapOut(r);PutBack(r);if(r.test)Revert(r);r.result="cancelled";}
                else if(Rank(r.phase)<Rank("LAUNCHED")&&
                        (!r.launch_attempted || r.launch_staging_sha256==StagedDataIdentity(r))){
                    RestoreRetail(r);if(r.swapped)SwapOut(r);PutBack(r);if(r.test)Revert(r);r.result="cancelled";
                }
                else{
                    if(r.game!=null&&ProcessOwnership.IsSameProcessAlive(r.game))return "GAME_RUNNING";
                    if(Rank(r.phase)<Rank("CAPTURED")){Capture(r,lib.Profile(r.profile));Advance(r,"CAPTURED");}
                    if(Rank(r.phase)<Rank("RESTORED")){RestoreRetail(r);if(r.swapped)SwapOut(r);PutBack(r);Advance(r,"RESTORED");}
                    if(r.test&&Rank(r.phase)<Rank("REVERTED")){Revert(r);Advance(r,"REVERTED");}
                    r.result=r.result??"played";
                }
                if(r.lease&&env.DevHost)PlayLease.ReleaseOwn(env.DevRepo,r.id);
                r.ended_utc=r.ended_utc??DateTime.UtcNow.ToString("o");
                Advance(r,"CLOSED");
            }
            return "OK";
        }
    }
}
