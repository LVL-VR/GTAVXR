using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace GtavxrSuite {
    internal sealed class ReleaseMigration {
        sealed class Journal {
            public string target { get; set; }
            public string bundle { get; set; }
            public string game { get; set; }
            public string selected { get; set; }
            public Dictionary<string,string> profiles { get; set; }
            public List<VersionManifest> retired { get; set; }
            // 1.0.5: retirement still owed by a superseded journal whose version records were already deleted. The journal
            // was the last place that named their files; their unreferenced blobs are retired by this journal's Finish.
            public List<VersionManifest> carried { get; set; }
        }
        // Written before an older journal is set aside and absorbed by the next Prepare, so an interruption between the
        // two never loses the owed retirement.
        sealed class Handover {
            public string from { get; set; }
            public string game { get; set; }
            public List<VersionManifest> carried { get; set; }
        }

        readonly Env env;
        readonly Library library;
        internal const string JournalName="release-migration.json", HandoverName="release-migration-handover.json";
        readonly string path,handover;

        internal ReleaseMigration(Env env) {
            this.env=env;
            library=new Library(env);
            path=SafeFs.Under(env.DataRoot,JournalName);
            handover=SafeFs.Under(env.DataRoot,HandoverName);
        }

        internal bool Pending { get { return File.Exists(path)||File.Exists(handover); } }
        // Altered blobs of retired releases that the last Finish set aside instead of deleting (reports/damaged-store).
        internal List<string> Kept { get; private set; }

        internal static void CheckPublicGamePaths(Env env,GameState state) {
            if(state!=null&&!String.Equals(SafeFs.Root(state.game_dir),SafeFs.Root(env.GameDir),StringComparison.OrdinalIgnoreCase))
                throw new InvalidOperationException("This library still manages another GTA folder: "+state.game_dir+
                    ". Reconnect or locate that installation and uninstall GTAVXR from it first.");
        }

        internal void CheckLibraries(VersionManifest target,string selected,DataLocation location=null) {
            if((!env.ManagedLocation&&location==null)||Pending||Plan(target,selected)==null)return;
            location=location??new DataLocation(Env.DefaultDataRoot());
            foreach(string directory in location.KnownLibraries()){
                if(String.Equals(directory,env.DataRoot,StringComparison.OrdinalIgnoreCase))continue;
                if(!new DriveInfo(Path.GetPathRoot(directory)).IsReady)
                    throw new IOException("Reconnect the previous GTAVXR library drive: "+directory);
                if(!Directory.Exists(directory))continue;
                using(var previous=GameScope.Acquire(directory)){
                    string oldProfiles=SafeFs.Under(directory,"profiles");
                    if(!Directory.Exists(oldProfiles))continue;
                    foreach(string profile in Directory.EnumerateDirectories(oldProfiles)){
                        string id=Path.GetFileName(profile);
                        string oldRecord=SafeFs.Under(profile,"profile.json");
                        if(!File.Exists(oldRecord))continue;
                        var old=Json.Read<Profile>(oldRecord);
                        if(old==null||old.id!=id||old.schema!="gtavxr-suite2-profile-v1")
                            throw new InvalidDataException("Invalid previous profile record: "+oldRecord);
                        if(PublicVersion(old.version)==null)continue;
                        Ids.Check(id);
                        string active=SafeFs.Under(env.DataRoot,"profiles/"+id);
                        if((!Directory.Exists(active)||!DataLocation.SameProfileFiles(profile,active))&&
                            !ContainsImportedStory(profile,old,library.Profile(id)??library.Profile(selected)))
                            throw new IOException("Another Suite library has a different profile or save. Review the original library or import its saves from the Save tab: "+profile);
                    }
                }
            }
        }

        bool ContainsImportedStory(string previous,Profile source,Profile current) {
            if(current==null)return false;
            var candidates=StoryStart.Choices(current).ToList();
            var saves=StoryStart.Choices(source).ToList();
            if(saves.Count==0)return false;
            string destination=library.UserDir(current.id);
            string from=SafeFs.Under(previous,"user");
            foreach(var save in saves){
                string name=Path.GetFileName(save.relative);
                var copied=candidates.FirstOrDefault(file=>file.bytes==save.bytes&&
                    String.Equals(file.sha256,save.sha256,StringComparison.OrdinalIgnoreCase)&&
                    (String.Equals(file.relative,save.relative,StringComparison.OrdinalIgnoreCase)||
                     String.Equals(file.relative,StoryStart.Archive+"import-"+save.sha256+"/"+name,StringComparison.OrdinalIgnoreCase)));
                if(copied==null)return false;
                SafeFs.AssertFile(SafeFs.Under(from,save.relative),save.sha256,save.bytes);
                SafeFs.AssertFile(SafeFs.Under(destination,copied.relative),copied.sha256,copied.bytes);
            }
            return true;
        }

        static Version PublicVersion(string id) {
            if(id==null||!id.StartsWith("gtavxr-",StringComparison.Ordinal))return null;
            var pieces=id.Substring(7).Split('-');
            Version version;
            if(pieces.Length!=3||!Version.TryParse(String.Join(".",pieces),out version))return null;
            return "gtavxr-"+version.ToString(3).Replace('.','-')==id?version:null;
        }

        IEnumerable<VersionManifest> Versions() {
            foreach(string file in Directory.EnumerateFiles(env.Dir("versions"),"*.json")){
                var version=Json.Read<VersionManifest>(file);
                if(version==null||version.schema!="gtavxr-suite2-version-v1"||version.id!=Path.GetFileNameWithoutExtension(file))
                    throw new InvalidDataException("Invalid version record: "+file);
                yield return version;
            }
        }

        IEnumerable<Profile> Profiles() {
            foreach(string directory in Directory.EnumerateDirectories(env.Dir("profiles"))){
                SafeFs.NoLinks(directory);
                string id=Path.GetFileName(directory);
                var profile=library.Profile(id);
                if(profile==null)continue;
                if(profile.id!=id||profile.schema!="gtavxr-suite2-profile-v1")
                    throw new InvalidDataException("Invalid profile record: "+directory);
                yield return profile;
            }
        }

        internal void CheckDowngrade(string target) {
            var current=PublicVersion(target);
            var installed=new GameLayer(env).State();
            CheckPublicGamePaths(env,installed);
            if(Versions().Any(v=>v.embedded_release&&PublicVersion(v.id)>current)||
               (installed!=null&&PublicVersion(installed.version)>current))
                throw new InvalidOperationException("A newer GTAVXR release is already installed. Use the newest GTAVXR launcher.");
        }

        internal static bool HasPrevious(string target,IEnumerable<VersionManifest> versions,IEnumerable<Profile> profiles) {
            var current=PublicVersion(target);
            var available=versions.ToList();
            if(available.Any(v=>v.embedded_release&&PublicVersion(v.id)!=null&&PublicVersion(v.id)<current)||
                profiles.Any(p=>KnownMissingRelease(p.version,target)))return true;
            var installed=available.FirstOrDefault(v=>v.embedded_release&&v.id==target);
            return installed!=null&&target==EmbeddedRelease.VersionId&&EmbeddedRelease.Available&&
                !String.Equals(installed.source_sha256,EmbeddedRelease.DescriptorIdentity(System.Threading.CancellationToken.None),
                    StringComparison.OrdinalIgnoreCase);
        }

        Journal Plan(VersionManifest target,string selected) {
            if(File.Exists(path))return Read(target);
            var current=PublicVersion(target.id);
            var versions=Versions().ToList();
            var retired=versions.Where(v=>v.embedded_release&&PublicVersion(v.id)!=null&&PublicVersion(v.id)<current).ToList();
            var ids=new HashSet<string>(retired.Select(v=>v.id),StringComparer.Ordinal);
            var all=Profiles().ToList();
            var known=new HashSet<string>(versions.Select(v=>v.id),StringComparer.Ordinal);
            var profiles=all.Where(p=>ids.Contains(p.version)||(!known.Contains(p.version)&&KnownMissingRelease(p.version,target.id)))
                .ToDictionary(p=>p.id,p=>p.version,StringComparer.Ordinal);
            var carried=ReadHandover(target.id);
            if(retired.Count==0&&profiles.Count==0&&carried.Count==0)return null;
            var chosen=all.FirstOrDefault(p=>p.id==selected&&(profiles.ContainsKey(p.id)||p.version==target.id))??
                       all.FirstOrDefault(p=>profiles.ContainsKey(p.id)||p.version==target.id);
            return new Journal{target=target.id,bundle=target.source_sha256,game=SafeFs.Root(env.GameDir),
                selected=chosen==null?null:chosen.id,profiles=profiles,retired=retired,carried=carried};
        }

        // The owed retirement handed over by a superseded journal; every entry must be an embedded release older than the
        // target, for this game folder. Absent file = nothing owed.
        List<VersionManifest> ReadHandover(string targetId) {
            if(!File.Exists(handover))return new List<VersionManifest>();
            Handover data;
            try { data=Json.Read<Handover>(handover); }
            catch(Exception error) { throw new InvalidDataException("The update handover record cannot be read. Keep it and contact support: "+handover,error); }
            if(data==null||data.carried==null||!String.Equals(data.game,SafeFs.Root(env.GameDir),StringComparison.OrdinalIgnoreCase)||
               !ValidRetired(data.carried,PublicVersion(targetId)))
                throw new InvalidDataException("The update handover record is not valid. Keep it and contact support: "+handover);
            return data.carried;
        }

        static bool ValidRetired(IEnumerable<VersionManifest> versions,Version below) {
            return below!=null&&versions.All(v=>v!=null&&v.embedded_release&&PublicVersion(v.id)!=null&&PublicVersion(v.id)<below);
        }

        static List<VersionManifest> Union(IEnumerable<VersionManifest> first,IEnumerable<VersionManifest> second) {
            var result=new List<VersionManifest>();
            var seen=new HashSet<string>(StringComparer.Ordinal);
            foreach(var version in (first??Enumerable.Empty<VersionManifest>()).Concat(second??Enumerable.Empty<VersionManifest>()))
                if(seen.Add(version.id))result.Add(version);
            return result;
        }

        // An update that stopped half way, met by this installer BEFORE anything new is imported. This installer's own
        // journal (same release and bundle) is resumed by Prepare/Finish as before. A verified journal of an older release,
        // or of another bundle of this release, for this same game folder and with no profile changed outside it, is set
        // aside (moved to reports/, never deleted) so this installer plans again: its plan retires every release older
        // than its own, so it covers everything the interrupted one would have retired. A journal of a newer release or of
        // another game folder stops here with the exact step. Returns the path of the journal set aside, else null.
        internal string ResolvePending(string targetId,string bundle) {
            if(!File.Exists(path)){ReadHandover(targetId);return null;}
            Journal journal;
            try { journal=Json.Read<Journal>(path); }
            catch(Exception error) { throw new InvalidDataException("The unfinished update record cannot be read. Keep it and contact support: "+path,error); }
            if(journal==null)throw new InvalidDataException("The unfinished update record cannot be read. Keep it and contact support: "+path);
            if(journal.target==targetId&&journal.bundle==bundle)return null;
            var from=PublicVersion(journal.target);
            var to=PublicVersion(targetId);
            if(from==null||to==null||journal.retired==null||journal.profiles==null||!SafeFs.IsHash(journal.bundle)||
               !ValidRetired(journal.retired,from)||!ValidRetired(journal.carried??new List<VersionManifest>(),from))
                throw new InvalidDataException("The unfinished update record is not valid. Keep it and contact support: "+path);
            if(from>to)
                throw new InvalidOperationException("An update to GTAVXR "+from.ToString(3)+" is unfinished. Open the GTAVXR "+from.ToString(3)+
                    " launcher (or a newer one) and select Update GTAVXR to finish it.");
            if(!String.Equals(journal.game,SafeFs.Root(env.GameDir),StringComparison.OrdinalIgnoreCase))
                throw new InvalidOperationException("The unfinished update belongs to another GTA folder: "+journal.game+
                    ". Reconnect or select that folder, then select Update GTAVXR again.");
            if(new GameLayer(env).Pending||new PlaySession(env).Unfinished().Count!=0)
                throw new InvalidOperationException("RECOVERY_REQUIRED");
            foreach(var entry in journal.profiles){
                var profile=library.Profile(entry.Key);
                if(profile!=null&&profile.version!=entry.Value&&profile.version!=journal.target)
                    throw new InvalidDataException("A profile changed during the unfinished update. Keep the library as it is and contact support: "+entry.Key);
            }
            // 1.0.5 review: its Finish may already have deleted version records of releases it retires; the journal is then the
            // last record of their files. Those releases (and what it carried itself) are handed over durably BEFORE the
            // journal is set aside; releases whose records still exist are planned again from those records.
            var owed=Union(journal.retired,journal.carried).Where(v=>!File.Exists(SafeFs.Under(env.DataRoot,"versions/"+v.id+".json"))).ToList();
            var previous=ReadHandover(targetId);
            var carried=Union(previous,owed);
            if(carried.Count!=previous.Count)
                Json.Atomic(handover,new Handover{from=journal.target,game=SafeFs.Root(env.GameDir),carried=carried});
            string kept=SafeFs.Under(env.Dir("reports"),"release-migration-superseded-"+DateTime.UtcNow.ToString("yyyyMMdd'T'HHmmssfff'Z'")+".json");
            File.Move(path,kept);
            return kept;
        }

        internal string SelectProfile(VersionManifest target,string selected) {
            var plan=Plan(target,selected);
            return plan==null?null:plan.selected;
        }

        // Publishes this installer's journal (or resumes its own), absorbing any handover first; the handover file goes only
        // once the journal that holds its releases is on disk.
        internal void Prepare(VersionManifest target,string selected) {
            if(File.Exists(path)){
                var journal=Read(target);
                var owed=ReadHandover(target.id);
                var merged=Union(journal.carried,owed);
                if(merged.Count!=(journal.carried??new List<VersionManifest>()).Count){journal.carried=merged;Json.Atomic(path,journal);}
            }else{
                var plan=Plan(target,selected);
                if(plan!=null)Json.PublishNew(path,plan);
            }
            if(File.Exists(handover))SafeFs.DeleteName(handover);
        }

        static bool KnownMissingRelease(string id,string target) {
            return EmbeddedRelease.KnownEarlier(id)&&PublicVersion(id)<PublicVersion(target);
        }

        Journal Read(VersionManifest target) {
            var journal=Json.Read<Journal>(path);
            if(journal==null||journal.target!=target.id||journal.bundle!=target.source_sha256||
               !String.Equals(journal.game,SafeFs.Root(env.GameDir),StringComparison.OrdinalIgnoreCase)||
               journal.retired==null||journal.profiles==null||
               !ValidRetired(journal.retired,PublicVersion(target.id))||!ValidRetired(journal.carried??new List<VersionManifest>(),PublicVersion(target.id)))
                throw new InvalidDataException("The unfinished update belongs to a different game folder or installer.");
            if(journal.carried==null)journal.carried=new List<VersionManifest>();
            return journal;
        }

        // Retires what the journal names. A blob of a retired release that no active version, mod or the installed game uses
        // is deleted when it still has its hash; an altered one is kept aside in reports/damaged-store and named in a report
        // (Kept), never deleted and never a reason to leave the update unfinished. The journal is removed last.
        internal void Finish(VersionManifest target) {
            Kept=new List<string>();
            if(!File.Exists(path))return;
            var journal=Read(target);
            var layer=new GameLayer(env);
            var installed=layer.State();
            if(layer.Pending||installed==null||installed.version!=target.id||layer.Verify().Count!=0)
                throw new InvalidDataException("The updated game files have not been verified.");
            var unfinished=new PlaySession(env).Unfinished();
            if(unfinished.Count!=0)throw new InvalidOperationException("RECOVERY_REQUIRED");
            var retiring=Union(journal.retired,journal.carried);
            var obsolete=new HashSet<string>(retiring.Select(v=>v.id).Concat(journal.profiles.Values),StringComparer.Ordinal);
            if(obsolete.Any(id=>PublicVersion(id)==null||PublicVersion(id)>=PublicVersion(target.id)))
                throw new InvalidDataException("Unexpected release in the migration journal.");
            foreach(var entry in journal.profiles){
                var profile=library.Profile(entry.Key);
                if(profile==null||(profile.version!=entry.Value&&profile.version!=target.id))
                    throw new InvalidDataException("Profile changed during the update: "+entry.Key);
                if(profile.version==target.id)continue;
                // Change only version, including in profiles with fields unknown to this release.
                string file=SafeFs.Under(library.ProfileDir(entry.Key),"profile.json");
                var data=Json.Read<Dictionary<string,object>>(file);
                data["version"]=target.id;
                Json.Atomic(file,data);
            }
            if(Profiles().Any(p=>obsolete.Contains(p.version)))
                throw new InvalidDataException("A profile still uses a previous release.");

            var reachable=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach(var version in Versions().Where(v=>!obsolete.Contains(v.id)))
                foreach(var hash in Hashes(version))reachable.Add(hash);
            foreach(string file in Directory.EnumerateFiles(env.Dir("mods"),"*.json")){
                var mod=Json.Read<ModManifest>(file);
                if(mod==null||mod.schema!="gtavxr-suite2-mod-v1"||mod.id!=Path.GetFileNameWithoutExtension(file))
                    throw new InvalidDataException("Invalid mod record: "+file);
                var files=Refs(mod.files).Concat(Refs(mod.script_files)).Concat(Refs(mod.asi_files)).Concat(Refs(new[]{mod.archive}));
                foreach(var item in files)reachable.Add(item.sha256);
            }
            foreach(var item in installed.files)reachable.Add(item.sha256);
            foreach(var version in retiring)RemoveRuntime(version);
            foreach(string id in obsolete) {
                Ids.Check(id);
                SafeFs.DeleteName(SafeFs.Under(env.DataRoot,"embedded/"+id+".json"));
                SafeFs.DeleteName(SafeFs.Under(env.DataRoot,"versions/"+id+".json"));
            }
            var blobs=new Blobs(env);
            var verified=new List<string>();
            var altered=new List<Tuple<string,string,string>>();   // stored file, its expected hash, where it is kept
            foreach(string hash in retiring.SelectMany(Hashes).Distinct(StringComparer.OrdinalIgnoreCase))
                if(!reachable.Contains(hash)){
                    string file=blobs.PathOf(hash);
                    if(!File.Exists(file))continue;
                    SafeFs.NoLinks(file);
                    if(String.Equals(SafeFs.Hash(file),hash,StringComparison.OrdinalIgnoreCase))verified.Add(file);
                    else altered.Add(Tuple.Create(file,hash,SafeFs.Under(env.Dir("reports/damaged-store"),hash+"-"+Ids.New())));
                }
            // The report is written before anything moves, so an interruption never leaves an unreported copy aside.
            if(altered.Count!=0)
                Json.New(SafeFs.Under(env.Dir("reports"),"release-migration-kept-"+DateTime.UtcNow.ToString("yyyyMMdd'T'HHmmssfff'Z'")+"-"+Ids.New().Substring(0,8)+".json"),
                         new{target=target.id,reason="A stored file of a retired GTAVXR release no longer had its recorded content. It was kept aside, not deleted.",
                             files=altered.Select(a=>new{expected_sha256=a.Item2,found_sha256=SafeFs.Hash(a.Item1),kept=a.Item3}).ToList()});
            foreach(var a in altered){
                File.SetAttributes(a.Item1,FileAttributes.Normal);
                File.Move(a.Item1,a.Item3);
                Kept.Add(a.Item3);
            }
            foreach(string file in verified)SafeFs.DeleteName(file);
            SafeFs.DeleteName(path);
        }

        // Earlier public 1.0.5 candidates shipped this unused developer tool under the same version ID.
        internal void RetirePublicMonitor(VersionManifest target) {
            if(!EmbeddedRelease.HasSessionMinidump(target))return;
            const string hash="4BE694A9DD41A823D8BDF61CF3EB6DB765DC1EE4666C372280D2DD8D1E9A157F";
            const string leaf="native_fault_monitor.exe";
            string runtime=SafeFs.Under(env.DataRoot,"runtime/"+target.id+"/"+leaf);
            RemoveMonitorCopy(runtime,hash);
            string stored=new Blobs(env).PathOf(hash);
            if(!File.Exists(stored))return;
            if(Versions().Any(v=>Hashes(v).Contains(hash,StringComparer.OrdinalIgnoreCase)))return;
            foreach(string file in Directory.EnumerateFiles(env.Dir("mods"),"*.json")){
                var mod=Json.Read<ModManifest>(file);
                if(mod==null||mod.schema!="gtavxr-suite2-mod-v1")return;
                var refs=Refs(mod.files).Concat(Refs(mod.script_files)).Concat(Refs(mod.asi_files)).Concat(Refs(new[]{mod.archive}));
                if(refs.Any(f=>String.Equals(f.sha256,hash,StringComparison.OrdinalIgnoreCase)))return;
            }
            var layer=new GameLayer(env).State();
            if(layer!=null&&Refs(layer.files).Any(f=>String.Equals(f.sha256,hash,StringComparison.OrdinalIgnoreCase)))return;
            RemoveMonitorCopy(stored,hash);
        }

        static void RemoveMonitorCopy(string path,string hash){
            try{
                if(File.Exists(path)&&new FileInfo(path).Length==28160&&SafeFs.Hash(path)==hash)
                    SafeFs.DeleteName(path);
            }catch(IOException error){
                if(!Blobs.IsSharingViolation(error))throw;
                throw new StoredBlobBusyException(hash,path,"native_fault_monitor.exe",error);
            }
        }

        void RemoveRuntime(VersionManifest version) {
            string root=SafeFs.Under(env.DataRoot,"runtime/"+version.id);
            if(!Directory.Exists(root))return;
            var paths=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach(var file in Refs(version.runtime)){
                string target=SafeFs.Under(root,file.path);
                paths.Add(target);
                if(File.Exists(target))SafeFs.AssertFile(target,file.sha256,file.bytes);
            }
            RemoveRuntimeDirectory(root,paths);
        }

        static void RemoveRuntimeDirectory(string root,HashSet<string> paths) {
            foreach(string directory in Directory.GetDirectories(root)){
                SafeFs.NoLinks(directory);
                RemoveRuntimeDirectory(directory,paths);
            }
            foreach(string file in Directory.GetFiles(root)){
                SafeFs.NoLinks(file);
                if(!paths.Contains(file))throw new InvalidDataException("Unexpected file in a previous GTAVXR runtime folder: "+file+
                    ". GTAVXR did not delete it. Move it out of the GTAVXR folder (for example to Documents), then press Update GTAVXR again; the update finishes from where it stopped.");
                SafeFs.DeleteName(file);
            }
            Directory.Delete(root);
        }

        static IEnumerable<FileRef> Refs(IEnumerable<FileRef> files) {
            return (files??Enumerable.Empty<FileRef>()).Where(f=>f!=null);
        }

        static IEnumerable<string> Hashes(VersionManifest version) {
            return Refs(version.files).Concat(Refs(version.runtime)).Concat(Refs(version.replaces))
                .Concat(Refs(new[]{version.vr_settings_seed})).Select(f=>f.sha256)
                .Concat((version.replaces??new List<ReplaceRef>()).Select(f=>f.original_sha256))
                .Where(SafeFs.IsHash);
        }
    }
}
