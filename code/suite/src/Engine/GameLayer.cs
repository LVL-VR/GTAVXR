using System;
using System.IO;
using System.Linq;
using System.Collections.Generic;

namespace GtavxrSuite {
    public class GameState {
        public string schema {get;set;}
        public string game_dir {get;set;}
        public string version {get;set;}
        public List<string> mods {get;set;}
        public List<FileRef> files {get;set;}       // files GTAVXR put in the game folder
        public List<string> dirs {get;set;}         // folders GTAVXR created (removed again when empty)
        public string updated_utc {get;set;}
    }
    public class ApplyJournal {
        public string schema {get;set;}
        public string game_dir {get;set;}
        public string version {get;set;}
        public List<string> mods {get;set;}
        public List<FileRef> adds {get;set;}
        public List<FileRef> removes {get;set;}
        public List<FileRef> keeps {get;set;}
        public List<string> dirs {get;set;}
        public string started_utc {get;set;}
        public string quarantine {get;set;}    // test flight undo: a changed GTAVXR file is moved here instead of stopping
    }
    public class ApplyPlan { public int add {get;set;} public int remove {get;set;} public int keep {get;set;} public long bytes {get;set;} public bool unchanged {get;set;} }
    public class GameConflictException : IOException { public GameConflictException(string m):base(m){} }

    // What GTAVXR has put inside the game folder. The game's own files are never replaced:
    // a path that exists and is not ours stops the operation instead of being overwritten.
    // An interrupted change is finished on the next start from the journal (all inputs are stored blobs).
    public sealed class GameLayer {
        private readonly Env env;private readonly Blobs blobs;
        public GameLayer(Env e){env=e;blobs=new Blobs(e);}
        private string StatePath{get{return SafeFs.Under(env.Dir("game"),"state.json");}}
        private string JournalPath{get{return SafeFs.Under(env.Dir("game"),"apply.json");}}

        public GameState State(){
            if(!File.Exists(StatePath))return null;
            var s=Json.Read<GameState>(StatePath);
            if(s.schema!="gtavxr-suite2-game-v1")throw new InvalidDataException("Unknown game state");
            return s;
        }
        public bool Pending{get{return File.Exists(JournalPath);}}

        public static List<FileRef> Desired(VersionManifest v,IEnumerable<ModManifest> mods){
            var list=new List<FileRef>();var owner=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);
            foreach(var f in v.files){SafeFs.Relative(f.path);owner[f.path]=v.Label();list.Add(f);}
            foreach(var m in mods){
                if(m.files==null)continue;
                foreach(var f in m.files){
                    SafeFs.Relative(f.path);string who;
                    if(owner.TryGetValue(f.path,out who))throw new GameConflictException(m.name+" y "+who+" cambian el mismo archivo: "+f.path);
                    owner[f.path]=m.name;list.Add(f);
                }
            }
            return list;
        }
        // Installed = the same version and mods AND the same files: a version re-imported under its id (167-scripts v2, Suite
        // 2.3.7 session 20261103T053926 kept the v1 EXE) or a mod rebuilt under its id changes files without changing ids.
        private static bool Same(GameState s,VersionManifest v,List<ModManifest> mods){
            if(s==null||s.version!=v.id||!(s.mods??new List<string>()).SequenceEqual(mods.Select(m=>m.id)))return false;
            var have=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);
            foreach(var f in s.files??new List<FileRef>())have[f.path]=f.sha256;
            var want=Desired(v,mods);string sha;
            return want.Count==have.Count&&want.All(f=>have.TryGetValue(f.path,out sha)&&String.Equals(sha,f.sha256,StringComparison.OrdinalIgnoreCase));
        }

        public ApplyPlan Preview(VersionManifest v,List<ModManifest> mods){
            var s=State();
            if(Same(s,v,mods))return new ApplyPlan{unchanged=true};
            var j=Plan(s,v,mods);
            return new ApplyPlan{add=j.adds.Count,remove=j.removes.Count,keep=j.keeps.Count,bytes=j.adds.Sum(f=>f.bytes)};
        }

        private ApplyJournal Plan(GameState s,VersionManifest v,List<ModManifest> mods){
            string game=SafeFs.Root(env.GameDir);
            var desired=v==null?new List<FileRef>():Desired(v,mods);
            var owned=new Dictionary<string,FileRef>(StringComparer.OrdinalIgnoreCase);
            if(s!=null){
                if(!String.Equals(s.game_dir,game,StringComparison.OrdinalIgnoreCase))throw new InvalidDataException("GTAVXR is installed in another game folder: "+s.game_dir);
                foreach(var f in s.files)owned[f.path]=f;
            }
            var j=new ApplyJournal{schema="gtavxr-suite2-apply-v1",game_dir=game,version=v==null?null:v.id,mods=mods.Select(m=>m.id).ToList(),
                adds=new List<FileRef>(),removes=new List<FileRef>(),keeps=new List<FileRef>(),dirs=new List<string>(s==null||s.dirs==null?new string[0]:s.dirs.ToArray()),started_utc=DateTime.UtcNow.ToString("o")};
            var want=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach(var f in desired){
                want.Add(f.path);
                string dest=SafeFs.Under(game,f.path);FileRef mine;bool isMine=owned.TryGetValue(f.path,out mine);
                if(Directory.Exists(dest))throw new GameConflictException("Hay una carpeta donde GTAVXR necesita un archivo: "+f.path);
                if(isMine&&String.Equals(mine.sha256,f.sha256,StringComparison.OrdinalIgnoreCase)&&File.Exists(dest)){j.keeps.Add(f);continue;}
                if(File.Exists(dest)&&!isMine){
                    // Delta installations only manage paths already owned by Suite or added by this transaction.
                    if(!v.embedded_release&&new FileInfo(dest).Length==f.bytes&&String.Equals(SafeFs.Hash(dest),f.sha256,StringComparison.OrdinalIgnoreCase)){j.keeps.Add(f);continue;}
                    throw new GameConflictException("Este archivo del juego no es de GTAVXR y no se va a sobrescribir: "+f.path);
                }
                j.adds.Add(f);
            }
            foreach(var f in owned.Values)if(!want.Contains(f.path))j.removes.Add(f);
            foreach(var f in j.adds)if(!blobs.Has(f.sha256,f.bytes))throw new InvalidDataException("A stored file is missing: "+f.path+". Repair the library.");
            SafeFs.Budget(env.DataRoot,j.adds.Sum(f=>f.bytes)*2);
            return j;
        }

        // Make the game folder hold exactly this version + mods (null version = remove everything).
        // Undoing a test flight passes checkRetail=false (it returns to a state that was already installed) and a
        // quarantine folder, so a GTAVXR file the test changed is moved aside instead of stopping the way back.
        public ApplyPlan Apply(VersionManifest v,List<ModManifest> mods,Action<string,int,int> progress,bool checkRetail=true,string quarantine=null){
            if(Pending)throw new InvalidOperationException("A previous game change is unfinished; recover first");
            var s=State();
            if(v!=null&&Same(s,v,mods))return new ApplyPlan{unchanged=true};
            if(v!=null&&checkRetail)LegacyGamePolicy.VerifyFiles(env.GameDir,env.RetailOverride??v.retail);
            if(quarantine!=null&&v!=null){
                // A file that appeared during the test where the previous version needs one: aside, not a conflict.
                string game=SafeFs.Root(env.GameDir);
                var owned=new HashSet<string>((s==null?new List<FileRef>():s.files).Select(f=>f.path),StringComparer.OrdinalIgnoreCase);
                foreach(var f in Desired(v,mods)){
                    string dest=SafeFs.Under(game,f.path);
                    if(owned.Contains(f.path)||!File.Exists(dest))continue;
                    if(!String.Equals(SafeFs.Hash(dest),f.sha256,StringComparison.OrdinalIgnoreCase))Quarantine(dest,quarantine,f.path);
                }
            }
            var j=Plan(s,v,mods);
            j.quarantine=quarantine;
            Json.Atomic(JournalPath,j);
            Run(j,progress);
            return new ApplyPlan{add=j.adds.Count,remove=j.removes.Count,keep=j.keeps.Count,bytes=j.adds.Sum(f=>f.bytes)};
        }
        public void Uninstall(Action<string,int,int> progress){Apply(null,new List<ModManifest>(),progress);}
        public void Recover(Action<string,int,int> progress){
            if(!Pending)return;
            Run(Json.Read<ApplyJournal>(JournalPath),progress);
        }

        private void Run(ApplyJournal j,Action<string,int,int> progress){
            string game=SafeFs.Root(j.game_dir);var s=State();
            var oldSha=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);
            if(s!=null)foreach(var f in s.files)oldSha[f.path]=f.sha256;
            // Folders we created earlier stay ours even if this journal did not list them.
            var dirs=new HashSet<string>(j.dirs??new List<string>(),StringComparer.OrdinalIgnoreCase);
            if(s!=null&&s.dirs!=null)foreach(string d in s.dirs)dirs.Add(d);
            int total=j.adds.Count+j.removes.Count,done=0;
            foreach(var f in j.adds){
                string dest=SafeFs.Under(game,f.path);
                if(File.Exists(dest)){
                    string h=SafeFs.Hash(dest);string old;
                    if(String.Equals(h,f.sha256,StringComparison.OrdinalIgnoreCase)){Report(progress,"install",++done,total);continue;}
                    if(!oldSha.TryGetValue(f.path,out old)||!String.Equals(h,old,StringComparison.OrdinalIgnoreCase)){
                        if(j.quarantine==null||!oldSha.ContainsKey(f.path))throw new GameConflictException("Un archivo de GTAVXR fue cambiado por otro programa: "+f.path);
                        Quarantine(dest,j.quarantine,f.path);
                    }
                }
                var missingDirs=NewDirs(game,f.path).Where(d=>!dirs.Contains(d)).ToList();
                if(missingDirs.Count>0){
                    // Record ownership before Place creates the directories so recovery can remove them later.
                    dirs.UnionWith(missingDirs);
                    j.dirs=dirs.ToList();
                    Json.Atomic(JournalPath,j);
                }
                blobs.Place(f,dest);
                Report(progress,"install",++done,total);
            }
            foreach(var f in j.removes){
                string dest=SafeFs.Under(game,f.path);
                if(File.Exists(dest)){
                    if(!String.Equals(SafeFs.Hash(dest),f.sha256,StringComparison.OrdinalIgnoreCase)){
                        if(j.quarantine==null)throw new GameConflictException("Un archivo de GTAVXR fue cambiado por otro programa: "+f.path);
                        Quarantine(dest,j.quarantine,f.path);
                    }else SafeFs.DeleteName(dest);
                }
                Report(progress,"remove",++done,total);
            }
            var files=j.keeps.Concat(j.adds).ToList();
            // Remove now-empty folders we created, deepest first.
            foreach(string d in dirs.OrderByDescending(x=>x.Length).ToList()){
                string full=SafeFs.Under(game,d);
                if(Directory.Exists(full)&&!Directory.EnumerateFileSystemEntries(full).Any()){Directory.Delete(full);dirs.Remove(d);}
            }
            if(j.version==null&&files.Count==0){
                PlaySession.ForgetEmbeddedInstallation(env);
                if(File.Exists(StatePath))File.Delete(StatePath);
            }
            else Json.Atomic(StatePath,new GameState{schema="gtavxr-suite2-game-v1",game_dir=game,version=j.version,mods=j.mods,files=files,dirs=dirs.ToList(),updated_utc=DateTime.UtcNow.ToString("o")});
            File.Delete(JournalPath);
        }
        private static IEnumerable<string> NewDirs(string game,string rel){
            var parts=rel.Split('/');var acc="";
            for(int i=0;i<parts.Length-1;i++){
                acc=acc.Length==0?parts[i]:acc+"/"+parts[i];
                if(!Directory.Exists(SafeFs.Under(game,acc)))yield return acc;
            }
        }
        // Move a file aside into a quarantine folder (never deleted; a second file with the same path gets a suffix).
        public static void Quarantine(string src,string quarantine,string rel){
            string dest=SafeFs.Under(quarantine,rel);
            if(File.Exists(dest))dest=dest+"."+DateTime.UtcNow.Ticks;
            Directory.CreateDirectory(Path.GetDirectoryName(dest));
            File.SetAttributes(src,FileAttributes.Normal);File.Move(src,dest);
        }
        private static void Report(Action<string,int,int> p,string what,int done,int total){if(p!=null&&(done==total||done%25==0))p(what,done,total);}

        // Ajustes > Comprobar archivos: rewrite every GTAVXR file that is missing or damaged from the store.
        public List<string> Repair(){
            var bad=Verify();var s=State();if(s==null||bad.Count==0)return bad;
            foreach(string path in bad){var f=s.files.First(x=>x.path==path);blobs.Place(f,SafeFs.Under(s.game_dir,f.path));}
            return bad;
        }        // Explicit check of every installed file (Ajustes > Reparar). Returns damaged paths.
        public List<string> Verify(){
            var s=State();var bad=new List<string>();if(s==null)return bad;
            foreach(var f in s.files){string p=SafeFs.Under(s.game_dir,f.path);if(!File.Exists(p)||new FileInfo(p).Length!=f.bytes||!String.Equals(SafeFs.Hash(p),f.sha256,StringComparison.OrdinalIgnoreCase))bad.Add(f.path);}
            return bad;
        }
    }
}
