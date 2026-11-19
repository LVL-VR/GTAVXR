using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;

namespace GtavxrSuite {
    // Settings > Check files. Every GTAVXR file of the installed version: the game-folder files of the installed state AND the
    // runtime files of runtime\<version> (bridge, loader, host...) that the game loads from the Suite library.
    public sealed class RepairReport {
        public List<string> checkedFiles=new List<string>();   // every file compared
        public List<string> repaired=new List<string>();       // damaged or missing, rewritten and verified again
        public List<string> unavailable=new List<string>();    // damaged, and no verified copy exists to restore it from
        public List<string> deferred=new List<string>();       // verification needs the package rebuild once the lock is released
        public List<string> aside=new List<string>();          // damaged copies actually moved to damaged-store
        public List<string> busy=new List<string>();           // damaged, but another program holds it: close it and retry
        public bool rebuilt;                                    // the stored copies were rebuilt from this launcher's own package
        public bool Complete { get { return unavailable.Count==0&&busy.Count==0&&deferred.Count==0; } }
    }

    public sealed partial class PlaySession {
        sealed class RepairItem { public string label,destination;public FileRef file;public bool game; }

        // Nothing outside the installed GTAVXR files changes: profiles, saves, mods' own records, DLSS models, OFXR and other
        // programs' files are never touched. A damaged stored copy is moved to reports\damaged-store (kept, not deleted).
        public RepairReport CheckFiles(CancellationToken cancel,long extraBytesForTest=0) {
            if(GameRunning())throw new InvalidOperationException("GAME_RUNNING");
            if(layer.Pending||Unfinished().Count!=0)throw new InvalidOperationException("RECOVERY_REQUIRED");
            var report=new RepairReport();
            var installed=layer.State();
            if(installed==null)return report;
            using(var library=GameScope.Acquire(env.DataRoot))
            using(var game=GameScope.Acquire(installed.game_dir)){
                if(GameRunning())throw new InvalidOperationException("GAME_RUNNING");
                if(layer.Pending||Unfinished().Count!=0)throw new InvalidOperationException("RECOVERY_REQUIRED");
                var version=lib.Version(installed.version);
                // 1.0.5 review: the version record must list the release's mandatory runtime. A null, empty, partial or
                // repeated list is rebuilt from this launcher's verified package when it is that release; otherwise every
                // missing name is reported and the check is incomplete (a correct game EXE alone is never success).
                var gaps=EmbeddedRelease.RuntimeGaps(version);
                StoredBlobBusyException busyBlob=null;
                bool triedRebuild=(gaps.Count!=0||PackageRecordIncomplete(version))&&IsThisPackage(version,cancel);
                if(triedRebuild){
                    try{
                        version=RebuildFromPackage(installed.game_dir,version.source_sha256,cancel,null);
                        report.rebuilt=true;
                        gaps=EmbeddedRelease.RuntimeGaps(version);
                    }catch(StoredBlobBusyException error){
                        busyBlob=error;
                    }catch(InvalidDataException error){
                        report.unavailable.Add("runtime/"+installed.version+" (the package could not be rebuilt: "+error.Message+")");
                    }
                }
                if(busyBlob!=null)report.busy.Add(StoredBlobLabel(busyBlob.Hash));
                var unverified=busyBlob==null?report.unavailable:report.deferred;
                foreach(var gap in gaps)unverified.Add("runtime/"+installed.version+"/"+gap+" (not in the version record)");
                var items=installed.files.Select(f=>new RepairItem{label=f.path,file=f,game=true,destination=SafeFs.Under(installed.game_dir,f.path)}).ToList();
                if(version==null)report.unavailable.Add("runtime/"+installed.version+" (the version record is missing)");
                else items.AddRange((version.runtime??new List<FileRef>()).Where(f=>f!=null&&!String.IsNullOrEmpty(f.path)&&SafeFs.IsHash(f.sha256))
                    .GroupBy(f=>f.path,StringComparer.OrdinalIgnoreCase).Select(g=>g.First()).Select(f=>new RepairItem{
                    label="runtime/"+version.id+"/"+f.path,file=f,destination=SafeFs.Under(RuntimeRoot(version),f.path)}));
                var damaged=new List<RepairItem>();
                foreach(var item in items){
                    cancel.ThrowIfCancellationRequested();
                    report.checkedFiles.Add(item.label);
                    if(!Matches(item.destination,item.file))damaged.Add(item);
                }
                if(damaged.Count==0)return report;
                // The stored copy of every damaged file must verify first; otherwise this launcher's own package rebuilds it.
                var missing=damaged.Where(item=>!StoredCopyVerifies(item.file)).ToList();
                if(missing.Count!=0&&!triedRebuild&&IsThisPackage(version,cancel)){
                    try{
                        RebuildFromPackage(installed.game_dir,version.source_sha256,cancel,null);
                        report.rebuilt=true;
                    }catch(StoredBlobBusyException error){
                        busyBlob=error;
                        report.busy.Add(StoredBlobLabel(error.Hash));
                    }
                    missing=missing.Where(item=>!StoredCopyVerifies(item.file)).ToList();
                }
                // Still damaged: the stored copy goes aside too, so importing the mod again or Update GTAVXR stores a fresh copy
                // instead of finding a same-size damaged one already in place.
                var packaged=version==null||version.files==null?null:new HashSet<string>(
                    version.files.Concat(version.runtime??new List<FileRef>()).Where(f=>f!=null).Select(f=>f.sha256),
                    StringComparer.OrdinalIgnoreCase);
                foreach(var item in missing){
                    if(busyBlob!=null&&String.Equals(busyBlob.Hash,item.file.sha256,StringComparison.OrdinalIgnoreCase))continue;
                    // A blocked rebuild leaves only the package's own replacements pending.
                    if(busyBlob!=null&&(packaged==null||packaged.Contains(item.file.sha256))){
                        report.deferred.Add(item.label);
                        continue;
                    }
                    try{
                        if(SetAsideStoredCopy(item.file))report.aside.Add(item.label);
                        report.unavailable.Add(item.label);
                    }catch(IOException error){
                        if(!Blobs.IsSharingViolation(error))throw;
                        string label=StoredBlobLabel(item.file.sha256);
                        if(!report.busy.Contains(label))report.busy.Add(label);
                    }
                }
                var restorable=damaged.Except(missing).ToList();
                // Room for every file before writing any of them (each is written beside its target, then swapped in).
                foreach(var volume in restorable.GroupBy(item=>Path.GetPathRoot(item.destination),StringComparer.OrdinalIgnoreCase))
                    SafeFs.Budget(volume.First().destination,checked(volume.Sum(item=>item.file.bytes)+extraBytesForTest));
                foreach(var item in restorable){
                    cancel.ThrowIfCancellationRequested();
                    try{
                        if(item.game)blobs.Place(item.file,item.destination);
                        else blobs.Materialize(item.file,item.destination);
                    }catch(IOException error){
                        if(error is DiskSpaceException||DiskSpaceException.IsDiskFull(error))throw;
                        report.busy.Add(item.label);continue;
                    }catch(UnauthorizedAccessException){report.busy.Add(item.label);continue;}
                    if(Matches(item.destination,item.file))report.repaired.Add(item.label);
                    else report.busy.Add(item.label);
                }
                return report;
            }
        }

        // The installed version is the release this launcher carries, with the same verified descriptor.
        static bool IsThisPackage(VersionManifest version,CancellationToken cancel){
            return version!=null&&version.embedded_release&&version.id==EmbeddedRelease.VersionId&&EmbeddedRelease.Available&&
                   version.source_sha256==EmbeddedRelease.DescriptorIdentity(cancel);
        }
        static bool PackageRecordIncomplete(VersionManifest v){
            return v!=null&&v.id==EmbeddedRelease.VersionId&&v.embedded_release&&
                (v.files==null||v.launch==null||String.IsNullOrEmpty(v.launch.exe)||
                 v.files.Any(f=>f==null||String.IsNullOrEmpty(f.path)||!SafeFs.IsHash(f.sha256)||f.bytes<=0)||
                 !v.files.Any(f=>String.Equals(f.path,v.launch.exe,StringComparison.OrdinalIgnoreCase)));
        }

        sealed class EmbeddedFile : FileRef {
            public string relative {get;set;}
        }

        sealed class ReconstructedRelease {
            public string embedded_identity {get;set;}
            public List<EmbeddedFile> files {get;set;}
            public List<EmbeddedFile> payloads {get;set;}
        }

        // Called under the library and GTA directory locks. The reconstructed descriptor lists the exact bytes we may repair.
        VersionManifest RebuildFromPackage(string game,string bundle,CancellationToken cancel,Action<PlayProgress> progress){
            string stage=SafeFs.Under(env.Dir("scratch"),"embedded-"+Ids.New());
            Step(progress,"game",0,0);
            try{
                EmbeddedRelease.Reconstruct(game,stage,cancel);
                var package=Json.Read<ReconstructedRelease>(SafeFs.Under(stage,"release.json"));
                if(package.embedded_identity!=bundle)
                    throw new InvalidDataException("The rebuilt package is not the installed version.");
                string seed=SafeFs.Under(stage,"trial-profile/vr-settings.ini");
                var references=package.files.Concat(package.payloads).ToList();
                if(File.Exists(seed))references.Add(new EmbeddedFile{relative="trial-profile/vr-settings.ini",
                    sha256=SafeFs.Hash(seed),bytes=new FileInfo(seed).Length});
                string reports=env.Dir("reports/damaged-store");
                foreach(var file in references.GroupBy(f=>f.sha256,StringComparer.OrdinalIgnoreCase).Select(group=>group.First())){
                    cancel.ThrowIfCancellationRequested();
                    file.path=file.relative;
                    blobs.RepairPackageCopy(SafeFs.Under(stage,file.relative),file,reports);
                }
                var version=new Importers(env).ImportEmbedded(stage,(done,total)=>{
                    cancel.ThrowIfCancellationRequested();
                    Step(progress,"game",done,total);
                });
                if(version.id!=EmbeddedRelease.VersionId||version.source_sha256!=bundle)
                    throw new InvalidDataException("The rebuilt package is not the installed version.");
                return version;
            }finally{ if(Directory.Exists(stage))Directory.Delete(stage,true); }
        }

        static string StoredBlobLabel(string sha){
            return "blobs/"+sha.Substring(0,2)+"/"+sha;
        }

        static bool Matches(string path,FileRef file){
            try{
                var info=new FileInfo(path);
                return info.Exists&&info.Length==file.bytes&&String.Equals(SafeFs.Hash(path),file.sha256,StringComparison.OrdinalIgnoreCase);
            }catch(IOException){return false;}catch(UnauthorizedAccessException){return false;}
        }

        bool StoredCopyVerifies(FileRef file){
            try{blobs.Verify(file.sha256,file.bytes);return true;}
            catch(InvalidDataException){return false;}catch(IOException){return false;}
        }

        bool SetAsideStoredCopy(FileRef file){
            string stored=blobs.PathOf(file.sha256);
            if(!File.Exists(stored))return false;
            string aside=SafeFs.Under(env.Dir("reports/damaged-store"),file.sha256+"-"+Ids.New());
            File.SetAttributes(stored,FileAttributes.Normal);
            File.Move(stored,aside);
            return true;
        }
    }
}
