using System;
using System.IO;
using System.Linq;
using System.Collections.Generic;
using System.Threading;

namespace GtavxrSuite {
    internal static class ReleaseLifecycleTests {
        static void Check(bool ok,string what) {
            if(!ok)throw new InvalidDataException("Lifecycle fixture: "+what);
        }

        static FileRef Blob(Env env,string name,string text) {
            string source=SafeFs.Under(env.DataRoot,"fixture-"+name);
            File.WriteAllText(source,text);
            var result=new Blobs(env).PutFile(source,name);
            File.Delete(source);
            return result;
        }

        internal static void Migration(string root) {
            var env=Env.ForFixture(root);
            var lib=new Library(env);
            var oldFile=Blob(env,"fixture-old.bin","old build");
            var shared=Blob(env,"fixture-shared.bin","shared resource");
            var newFile=Blob(env,"fixture-next.bin","new build");
            var oldRuntime=Blob(env,"old-runtime.dll","old bridge");
            var old=new VersionManifest{id="gtavxr-1-0-0",embedded_release=true,files=new List<FileRef>{oldFile,shared},
                runtime=new List<FileRef>{oldRuntime},source_sha256=oldFile.sha256};
            var next=new VersionManifest{id=EmbeddedRelease.VersionId,embedded_release=true,files=new List<FileRef>{newFile,shared},
                runtime=new List<FileRef>(),source_sha256=newFile.sha256};
            lib.SaveVersion(old);
            lib.SaveVersion(next);
            lib.SaveVersion(new VersionManifest{id="172",files=new List<FileRef>{shared}});
            string runtime=SafeFs.Under(env.Dir("runtime/"+old.id),oldRuntime.path);
            new Blobs(env).Materialize(oldRuntime,runtime);
            var p1=new Profile{id="first",version=old.id,name="First"};
            var p2=new Profile{id="second",version=old.id,name="Second",mods=new List<string>{"retained-mod"}};
            lib.SaveProfile(p1);lib.SaveProfile(p2);
            lib.SaveProfile(new Profile{id="dev",version="172",name="Dev"});
            lib.SaveMod(new ModManifest{id="retained-mod",archive=shared});
            string profile=SafeFs.Under(lib.ProfileDir(p2.id),"profile.json");
            var extra=Json.Read<Dictionary<string,object>>(profile);
            extra["custom-setting"]="keep this";
            Json.Atomic(profile,extra);
            string save=SafeFs.Under(lib.UserDir(p2.id),"SGTA50000");
            File.WriteAllText(save,"retained story data");
            string saveHash=SafeFs.Hash(save);
            string comparison=env.Dir("scratch/profile-comparison");
            UserTree.CopyInto(lib.ProfileDir(p2.id),UserTree.Snapshot(lib.ProfileDir(p2.id)),comparison);
            Check(DataLocation.SameProfileFiles(lib.ProfileDir(p2.id),comparison),"an exact copy of the profile matches");
            string otherSave=SafeFs.Under(comparison,"user/SGTA50000");
            File.WriteAllText(otherSave,"Retained story data");
            Check(!DataLocation.SameProfileFiles(lib.ProfileDir(p2.id),comparison),"same-size changed save is detected");
            var layer=new GameLayer(env);
            layer.Apply(old,new List<ModManifest>(),null,false);
            var migration=new ReleaseMigration(env);
            Check(migration.SelectProfile(next,p2.id)==p2.id,"selected profile retained");
            migration.Prepare(next,p2.id);
            try { migration.Finish(next);throw new Exception("premature cleanup"); }
            catch(InvalidDataException) {}
            Check(File.Exists(runtime)&&lib.Version(old.id)!=null,"old files survive until target verified");
            layer.Apply(next,new List<ModManifest>(),null,false);
            new ReleaseMigration(env).Finish(next);
            new ReleaseMigration(env).Finish(next);
            Check(lib.Profile(p1.id).version==next.id&&lib.Profile(p2.id).version==next.id&&lib.Profile("dev").version=="172",
                "two public profiles migrate in place and internal profile stays");
            Check(lib.Profiles().Count==3&&SafeFs.Hash(save)==saveHash&&
                  (string)Json.Read<Dictionary<string,object>>(profile)["custom-setting"]=="keep this",
                "no duplicate profiles, changed save, or lost unknown settings");
            var store=new Blobs(env);
            Check(!migration.Pending&&lib.Version(old.id)==null&&!File.Exists(runtime)&&
                !File.Exists(store.PathOf(oldFile.sha256))&&File.Exists(store.PathOf(shared.sha256))&&
                !File.Exists(SafeFs.Under(env.GameDir,oldFile.path))&&File.Exists(SafeFs.Under(env.GameDir,newFile.path)),
                "only exclusive old files are removed");
        }

        // An update to an older release (1.0.2) stopped at each phase, then this installer runs, with and without the newer
        // release already imported by a failed earlier attempt (the state Suite 1.0.4 left). The verified journal is set
        // aside (kept, never deleted), this installer migrates every profile, and saves, profile files, mods, foreign game
        // files and DLSS models stay exactly as they were. An older installer can still finish its own journal unless a
        // newer release is installed; invalid, newer or foreign journals stop with the journal intact.
        internal static void InterruptedOtherRelease(string root) {
            for(int phase=0;phase<4;phase++)
                for(int affected=0;affected<2;affected++)
                    InterruptedOtherRelease(Path.Combine(root,"phase"+phase+"-"+affected),phase,affected!=0);
            foreach(string kind in new[]{"newer","folder","corrupt","profile"}) RejectedJournal(Path.Combine(root,"reject-"+kind),kind);
        }

        sealed class OtherRelease {
            public Env env;public Library lib;public GameLayer layer;public VersionManifest v100,v102,next;
            public Profile p1,p2;public string save,saveHash,ini,iniText,foreign,model,journal;
        }

        static OtherRelease Interrupted(string root) {
            var s=new OtherRelease{env=Env.ForFixture(root)};
            s.lib=new Library(s.env);s.layer=new GameLayer(s.env);
            var file100=Blob(s.env,"fixture-100.bin","release 1.0.0");
            var file102=Blob(s.env,"fixture-102.bin","release 1.0.2");
            var fileNext=Blob(s.env,"fixture-next.bin","this release");
            var shared=Blob(s.env,"fixture-shared.bin","shared resource");
            var runtime100=Blob(s.env,"runtime-100.dll","bridge 1.0.0");
            var runtime102=Blob(s.env,"runtime-102.dll","bridge 1.0.2");
            s.v100=new VersionManifest{id=EmbeddedRelease.PreviousVersionId,embedded_release=true,files=new List<FileRef>{file100,shared},
                runtime=new List<FileRef>{runtime100},source_sha256=file100.sha256};
            s.v102=new VersionManifest{id=EmbeddedRelease.Release102Id,embedded_release=true,files=new List<FileRef>{file102,shared},
                runtime=new List<FileRef>{runtime102},source_sha256=file102.sha256};
            s.next=new VersionManifest{id=EmbeddedRelease.VersionId,embedded_release=true,files=new List<FileRef>{fileNext,shared},
                runtime=new List<FileRef>(),source_sha256=fileNext.sha256};
            s.lib.SaveVersion(s.v100);
            new Blobs(s.env).Materialize(runtime100,SafeFs.Under(s.env.Dir("runtime/"+s.v100.id),runtime100.path));
            s.p1=new Profile{id="first",version=s.v100.id,name="First"};
            s.p2=new Profile{id="second",version=s.v100.id,name="Second",mods=new List<string>{"retained-mod"}};
            s.lib.SaveProfile(s.p1);s.lib.SaveProfile(s.p2);
            s.lib.SaveMod(new ModManifest{id="retained-mod",archive=shared});
            s.save=SafeFs.Under(s.lib.UserDir(s.p2.id),"SGTA50000");File.WriteAllText(s.save,"retained story data");s.saveHash=SafeFs.Hash(s.save);
            s.ini=s.lib.VrSettingsFile(s.p2.id);s.iniText="[Actions]\nphone=2\nvehicle_camera=10\n";File.WriteAllText(s.ini,s.iniText);
            s.foreign=SafeFs.Under(s.env.GameDir,"foreign-plugin.dll");File.WriteAllText(s.foreign,"player file");
            s.model=SafeFs.Under(s.env.Dir("dlss"),"nvngx_dlss.dll");File.WriteAllText(s.model,"player model");
            s.layer.Apply(s.v100,new List<ModManifest>(),null,false);
            // The 1.0.2 installer imported its release and published its journal.
            s.lib.SaveVersion(s.v102);
            new Blobs(s.env).Materialize(runtime102,SafeFs.Under(s.env.Dir("runtime/"+s.v102.id),runtime102.path));
            new ReleaseMigration(s.env).Prepare(s.v102,s.p2.id);
            s.journal=SafeFs.Under(s.env.DataRoot,"release-migration.json");
            Check(File.Exists(s.journal),"the interrupted update published its journal");
            return s;
        }

        static void InterruptedOtherRelease(string root,int phase,bool affected) {
            var s=Interrupted(root);
            string tag="phase "+phase+(affected?" after a failed newer attempt":"");
            if(phase==1){
                // The game files of 1.0.2 were being written when it stopped.
                try{s.layer.Apply(s.v102,new List<ModManifest>(),(what,done,total)=>{throw new IOException("interrupted apply");},false);throw new Exception("expected interruption");}
                catch(IOException error){Check(error.Message=="interrupted apply","simulated apply interrupt");}
                Check(s.layer.Pending,"interrupted game files keep their own journal");
                try{new ReleaseMigration(s.env).ResolvePending(s.next.id,s.next.source_sha256);throw new Exception("resolved over pending game files");}
                catch(InvalidOperationException error){Check(error.Message=="RECOVERY_REQUIRED"&&File.Exists(s.journal),tag+": game files are recovered first");}
                s.layer.Recover(null);
            }
            if(phase>=2)s.layer.Apply(s.v102,new List<ModManifest>(),null,false);
            if(phase==3){
                // Finish of 1.0.2 stopped after moving one profile and retiring 1.0.0's runtime and record.
                string record=SafeFs.Under(s.lib.ProfileDir(s.p1.id),"profile.json");
                var data=Json.Read<Dictionary<string,object>>(record);data["version"]=s.v102.id;Json.Atomic(record,data);
                SafeFs.DeleteName(SafeFs.Under(s.env.Dir("runtime/"+s.v100.id),s.v100.runtime[0].path));
                Directory.Delete(s.env.Dir("runtime/"+s.v100.id));
                SafeFs.DeleteName(SafeFs.Under(s.env.DataRoot,"versions/"+s.v100.id+".json"));
            }
            if(affected)s.lib.SaveVersion(s.next);   // Suite 1.0.4 imported its release before reading the journal
            byte[] journalBytes=File.ReadAllBytes(s.journal);
            // The older installer: it may finish its own journal only while no newer release is installed.
            try{new ReleaseMigration(s.env).CheckDowngrade(s.v102.id);Check(!affected,tag+": older installer allowed with a newer release");}
            catch(InvalidOperationException error){Check(affected&&error.Message.Contains("newest GTAVXR launcher"),tag+": older installer stops only with a newer release");}
            var migration=new ReleaseMigration(s.env);
            string kept=migration.ResolvePending(s.next.id,s.next.source_sha256);
            Check(kept!=null&&!File.Exists(s.journal)&&File.ReadAllBytes(kept).SequenceEqual(journalBytes)&&
                  Path.GetDirectoryName(kept)==s.env.Dir("reports"),tag+": the interrupted journal is kept aside unchanged");
            migration.CheckDowngrade(s.next.id);
            if(!affected)s.lib.SaveVersion(s.next);
            string chosen=migration.SelectProfile(s.next,s.p2.id);
            Check(chosen==s.p2.id,tag+": the selected profile is migrated");
            migration.Prepare(s.next,chosen);
            s.layer.Apply(s.next,new List<ModManifest>(),null,false);
            if(phase==2&&!affected){
                // An unexpected file in a retired runtime folder stops the cleanup with its exact path and keeps the
                // journal; once it is moved out, the same step finishes.
                string stray=SafeFs.Under(s.env.Dir("runtime/"+s.v102.id),"player-note.txt");File.WriteAllText(stray,"mine");
                try{new ReleaseMigration(s.env).Finish(s.next);throw new Exception("stray runtime file removed");}
                catch(InvalidDataException error){Check(error.Message.Contains(stray)&&File.Exists(stray)&&new ReleaseMigration(s.env).Pending,tag+": unexpected runtime file kept and named");}
                string outside=Path.Combine(Path.GetDirectoryName(Path.GetFullPath(s.env.DataRoot).TrimEnd('\\')),"player-note-"+Ids.New()+".txt");
                File.Move(stray,outside);
                Check(File.ReadAllText(outside)=="mine",tag+": the moved file is the player's own");
            }
            new ReleaseMigration(s.env).Finish(s.next);
            Check(!new ReleaseMigration(s.env).Pending&&File.Exists(kept),tag+": finished, journal kept");
            Check(s.lib.Profile(s.p1.id).version==s.next.id&&s.lib.Profile(s.p2.id).version==s.next.id&&s.lib.Profiles().Count==2,
                  tag+": every profile is on this release, none duplicated");
            Check(SafeFs.Hash(s.save)==s.saveHash&&File.ReadAllText(s.ini)==s.iniText&&s.lib.Mod("retained-mod")!=null&&
                  File.ReadAllText(s.foreign)=="player file"&&File.ReadAllText(s.model)=="player model",
                  tag+": saves, VR settings, mods, foreign game files and DLSS models unchanged");
            Check(s.lib.Version(s.v100.id)==null&&s.lib.Version(s.v102.id)==null&&!Directory.Exists(SafeFs.Under(s.env.DataRoot,"runtime/"+s.v102.id))&&
                  s.layer.State().version==s.next.id&&s.layer.Verify().Count==0,tag+": older releases retired, game verified");
            // Uninstalling everything afterwards is not blocked by the journal kept aside.
            var location=new DataLocation(s.env.DataRoot);
            CompleteRemoval.RemoveData(s.env,location,CompleteRemoval.Libraries(s.env,location));
            Check(!Directory.Exists(s.env.DataRoot)&&File.ReadAllText(s.foreign)=="player file",tag+": full removal still works");
        }

        // 1.0.5 review (Codex): Finish of 1.0.4 had migrated the profiles, removed 1.0.2's runtime and deleted the 1.0.2
        // record, then stopped on an altered 1.0.2 blob; another 1.0.2 blob, valid and exclusive, was still to retire. The
        // 1.0.4 journal is the last record of those files. Resuming with this installer hands that work over durably, keeps
        // it through every interruption, retires the valid exclusive blob, keeps the altered one aside and reports it, and
        // keeps whatever an active version, a mod or the installed game uses.
        internal static void MigrationHandover(string root) {
            foreach(string stop in new[]{"none","after-resolve","before-move","after-prepare","before-apply"})
                MigrationHandover(Path.Combine(root,stop),stop);
        }

        static void MigrationHandover(string root,string stop) {
            var env=Env.ForFixture(root);var lib=new Library(env);var layer=new GameLayer(env);var store=new Blobs(env);
            var altered=Blob(env,"old-altered.bin","1.0.2 file later altered");
            var valid=Blob(env,"old-valid.bin","1.0.2 exclusive verified file");
            var modShared=Blob(env,"old-mod.bin","1.0.2 file a mod also uses");
            var nextShared=Blob(env,"old-next.bin","1.0.2 file 1.0.5 also uses");
            var file104=Blob(env,"file-104.bin","1.0.4 exclusive file");
            var fileNext=Blob(env,"file-next.bin","1.0.5 file");
            var v102=new VersionManifest{id=EmbeddedRelease.Release102Id,embedded_release=true,source_sha256=valid.sha256,
                files=new List<FileRef>{altered,valid,modShared,nextShared},runtime=new List<FileRef>()};
            var v104=new VersionManifest{id=EmbeddedRelease.Release104Id,embedded_release=true,source_sha256=file104.sha256,
                files=new List<FileRef>{file104},runtime=new List<FileRef>()};
            var next=new VersionManifest{id=EmbeddedRelease.VersionId,embedded_release=true,source_sha256=fileNext.sha256,
                files=new List<FileRef>{fileNext,nextShared},runtime=new List<FileRef>()};
            lib.SaveVersion(v102);
            var p=new Profile{id="player",version=v102.id,name="Player"};lib.SaveProfile(p);
            string save=SafeFs.Under(lib.UserDir(p.id),"SGTA50000");File.WriteAllText(save,"story");string saveHash=SafeFs.Hash(save);
            lib.SaveMod(new ModManifest{id="keeps-a-file",archive=modShared});
            string foreign=SafeFs.Under(env.GameDir,"foreign-plugin.dll");File.WriteAllText(foreign,"player file");
            layer.Apply(v102,new List<ModManifest>(),null,false);
            // The 1.0.4 installer: its journal, its game files, then a Finish that stopped after deleting the 1.0.2 record.
            lib.SaveVersion(v104);
            new ReleaseMigration(env).Prepare(v104,p.id);
            layer.Apply(v104,new List<ModManifest>(),null,false);
            string record=SafeFs.Under(lib.ProfileDir(p.id),"profile.json");
            var data=Json.Read<Dictionary<string,object>>(record);data["version"]=v104.id;Json.Atomic(record,data);
            SafeFs.DeleteName(SafeFs.Under(env.DataRoot,"versions/"+v102.id+".json"));
            string alteredBlob=store.PathOf(altered.sha256);File.SetAttributes(alteredBlob,FileAttributes.Normal);File.WriteAllText(alteredBlob,"changed blob, same place");
            string journal=SafeFs.Under(env.DataRoot,"release-migration.json"),handover=SafeFs.Under(env.DataRoot,"release-migration-handover.json");
            Check(File.Exists(journal)&&lib.Version(v102.id)==null,stop+": the 1.0.4 journal is the last record of 1.0.2");
            // This installer.
            var migration=new ReleaseMigration(env);
            string kept=migration.ResolvePending(next.id,next.source_sha256);
            Check(kept!=null&&!File.Exists(journal)&&File.Exists(handover),stop+": the owed retirement is handed over before the journal is set aside");
            if(stop=="before-move"){
                // Interrupted after writing the handover and before setting the journal aside: the next run repeats it.
                File.Move(kept,journal);
                kept=new ReleaseMigration(env).ResolvePending(next.id,next.source_sha256);
                Check(kept!=null&&!File.Exists(journal)&&File.Exists(handover),stop+": the repeated step completes");
            }
            if(stop=="after-resolve"){
                Check(new ReleaseMigration(env).Pending,stop+": a handover alone keeps the update unfinished (Play stays blocked)");
                Check(new ReleaseMigration(env).ResolvePending(next.id,next.source_sha256)==null&&File.Exists(handover),stop+": resuming keeps the handover");
            }
            migration=new ReleaseMigration(env);
            migration.CheckDowngrade(next.id);
            lib.SaveVersion(next);
            byte[] handoverBytes=File.ReadAllBytes(handover);
            migration.Prepare(next,migration.SelectProfile(next,p.id));
            var planned=Json.Read<Dictionary<string,object>>(journal);
            Check(!File.Exists(handover)&&((System.Collections.ICollection)planned["carried"]).Count==1,stop+": the new journal holds the owed 1.0.2 retirement");
            if(stop=="after-prepare"){
                // Interrupted after the new journal and before the handover was removed: the next Prepare absorbs it again.
                File.WriteAllBytes(handover,handoverBytes);
                new ReleaseMigration(env).Prepare(next,p.id);
                Check(!File.Exists(handover)&&((System.Collections.ICollection)Json.Read<Dictionary<string,object>>(journal)["carried"]).Count==1,stop+": absorbed once, not twice");
            }
            if(stop=="before-apply"){
                try{new ReleaseMigration(env).Finish(next);throw new Exception("premature cleanup");}catch(InvalidDataException){}
                Check(File.Exists(journal)&&File.Exists(store.PathOf(valid.sha256)),stop+": nothing retired before 1.0.5 is verified");
            }
            layer.Apply(next,new List<ModManifest>(),null,false);
            var finish=new ReleaseMigration(env);
            finish.Finish(next);
            // The remaining files after success, not just the absence of an error.
            var blobsLeft=Directory.EnumerateFiles(env.Dir("blobs"),"*",SearchOption.AllDirectories).Select(Path.GetFileName).ToList();
            var keptFiles=Directory.EnumerateFiles(env.Dir("reports/damaged-store")).ToList();
            var reports=Directory.EnumerateFiles(env.Dir("reports"),"release-migration-kept-*.json").ToList();
            Check(!File.Exists(journal)&&!File.Exists(handover)&&!finish.Pending,stop+": update finished");
            Check(!blobsLeft.Contains(valid.sha256)&&!blobsLeft.Contains(file104.sha256),stop+": the valid exclusive 1.0.2 blob and the 1.0.4 blob are retired");
            Check(blobsLeft.Contains(modShared.sha256)&&blobsLeft.Contains(nextShared.sha256)&&blobsLeft.Contains(fileNext.sha256),
                  stop+": blobs a mod or 1.0.5 uses stay");
            Check(!blobsLeft.Contains(altered.sha256)&&keptFiles.Count==1&&File.ReadAllText(keptFiles[0])=="changed blob, same place"&&
                  finish.Kept.Count==1&&finish.Kept[0]==keptFiles[0],stop+": the altered blob is kept aside, not deleted");
            Check(reports.Count==1&&File.ReadAllText(reports[0]).Contains(altered.sha256)&&File.ReadAllText(reports[0]).Contains(keptFiles[0].Replace("\\","\\\\")),
                  stop+": and reported with its expected hash and where it is");
            Check(lib.Version(v102.id)==null&&lib.Version(v104.id)==null&&lib.Profile(p.id).version==next.id&&SafeFs.Hash(save)==saveHash&&
                  File.ReadAllText(foreign)=="player file"&&lib.Mod("keeps-a-file")!=null&&layer.Verify().Count==0&&File.Exists(kept),
                  stop+": profile on 1.0.5, save, mod and foreign file unchanged, game verified, old journal kept aside");
        }

        // A handover of another game folder or with a release that is not older stops the update and stays intact.
        internal static void ForeignHandover(string root) {
            var env=Env.ForFixture(root);var lib=new Library(env);
            var fileNext=Blob(env,"file-next.bin","1.0.5 file");
            var next=new VersionManifest{id=EmbeddedRelease.VersionId,embedded_release=true,source_sha256=fileNext.sha256,files=new List<FileRef>{fileNext},runtime=new List<FileRef>()};
            string handover=SafeFs.Under(env.DataRoot,"release-migration-handover.json");
            foreach(var bad in new object[]{
                new{from="gtavxr-1-0-4",game=SafeFs.Root(env.GameDir)+"-elsewhere",carried=new[]{new{id="gtavxr-1-0-2",embedded_release=true}}},
                new{from="gtavxr-1-0-4",game=SafeFs.Root(env.GameDir),carried=new[]{new{id="gtavxr-1-0-9",embedded_release=true}}},
                new{from="gtavxr-1-0-4",game=SafeFs.Root(env.GameDir),carried=new[]{new{id="gtavxr-1-0-2",embedded_release=false}}}}){
                Json.Atomic(handover,bad);byte[] before=File.ReadAllBytes(handover);
                try{new ReleaseMigration(env).ResolvePending(next.id,next.source_sha256);throw new Exception("accepted a foreign handover");}
                catch(InvalidDataException){}
                Check(File.ReadAllBytes(handover).SequenceEqual(before)&&new ReleaseMigration(env).Pending,"a foreign handover stays exactly where it was");
            }
        }

        static void RejectedJournal(string root,string kind) {
            var s=Interrupted(root);
            if(kind=="newer"){
                var data=Json.Read<Dictionary<string,object>>(s.journal);data["target"]="gtavxr-1-0-9";Json.Atomic(s.journal,data);
            }else if(kind=="folder"){
                var data=Json.Read<Dictionary<string,object>>(s.journal);data["game"]=s.env.GameDir+"-elsewhere";Json.Atomic(s.journal,data);
            }else if(kind=="corrupt")File.WriteAllText(s.journal,"{\"target\":");
            else{
                string record=SafeFs.Under(s.lib.ProfileDir(s.p2.id),"profile.json");
                var data=Json.Read<Dictionary<string,object>>(record);data["version"]="172";Json.Atomic(record,data);
            }
            byte[] before=File.ReadAllBytes(s.journal);
            try{new ReleaseMigration(s.env).ResolvePending(s.next.id,s.next.source_sha256);throw new Exception("accepted "+kind+" journal");}
            catch(InvalidOperationException error){Check((kind=="newer"&&error.Message.Contains("1.0.9"))||(kind=="folder"&&error.Message.Contains("-elsewhere")),"stop for "+kind+": "+error.Message);}
            catch(InvalidDataException error){Check(kind=="corrupt"||kind=="profile","data stop for "+kind+": "+error.Message);}
            Check(File.ReadAllBytes(s.journal).SequenceEqual(before)&&!Directory.EnumerateFiles(s.env.Dir("reports"),"release-migration-*").Any(),
                  kind+": the journal stays exactly where it was");
        }

        // Settings > Check files covers the game folder AND runtime\<version>. Damaged files are restored and verified;
        // a damaged stored copy, a file held by another program, no disk space and an interrupted run never report success;
        // a retry finishes; profiles, saves, mods, DLSS models and OFXR files are never touched.
        internal static void RepairBoth(string root) {
            var env=Env.ForFixture(root);
            var lib=new Library(env);
            var game=Blob(env,"GTAVXR_SourceBaseline.exe","verified image");
            var extra=Blob(env,"gtavxr-extra.dll","second game file");
            var bridge=Blob(env,"gtavxr_native_openxr.dll","verified bridge");
            var loader=Blob(env,"openxr_loader.dll","verified loader");
            var v=new VersionManifest{id="repair-fixture",files=new List<FileRef>{game,extra},runtime=new List<FileRef>{bridge,loader}};
            lib.SaveVersion(v);
            var layer=new GameLayer(env);
            layer.Apply(v,new List<ModManifest>(),null,false);
            var session=new PlaySession(env);
            string runtime=session.RuntimeDir(v);
            string bridgePath=SafeFs.Under(runtime,bridge.path),gamePath=SafeFs.Under(env.GameDir,game.path),extraPath=SafeFs.Under(env.GameDir,extra.path);
            var profile=new Profile{id="keep",version=v.id,name="Keep"};lib.SaveProfile(profile);
            string save=SafeFs.Under(lib.UserDir(profile.id),"SGTA50000");File.WriteAllText(save,"story");
            string model=SafeFs.Under(env.Dir("dlss"),"nvngx_dlss.dll");File.WriteAllText(model,"model");
            string ofxr=SafeFs.Under(env.Dir("ofxr"),"state.ini");File.WriteAllText(ofxr,"nr_enabled=1");
            Func<bool> untouched=()=>File.ReadAllText(save)=="story"&&File.ReadAllText(model)=="model"&&File.ReadAllText(ofxr)=="nr_enabled=1"&&lib.Profile("keep")!=null;
            var none=CancellationNone();
            var ok=session.CheckFiles(none);
            Check(ok.Complete&&ok.repaired.Count==0&&ok.checkedFiles.Count==4&&ok.checkedFiles.Contains("runtime/repair-fixture/"+bridge.path),"all four files checked, game folder and runtime");
            // Codex's counterexample: a wrong runtime DLL was reported as fine.
            File.WriteAllText(bridgePath,"wrong-bridge");
            File.Delete(gamePath);
            var fixedBoth=session.CheckFiles(none);
            Check(fixedBoth.Complete&&fixedBoth.repaired.Count==2&&File.ReadAllText(bridgePath)=="verified bridge"&&File.ReadAllText(gamePath)=="verified image",
                  "a wrong runtime file and a missing game file are restored and verified");
            // A damaged stored copy: nothing to restore from; never success, the damaged file is left as found.
            string stored=new Blobs(env).PathOf(bridge.sha256);
            File.SetAttributes(stored,FileAttributes.Normal);File.WriteAllText(stored,"VERIFIED BRIDGE");
            File.WriteAllText(bridgePath,"wrong-bridge");
            var corrupt=session.CheckFiles(none);
            Check(!corrupt.Complete&&corrupt.unavailable.Contains("runtime/repair-fixture/"+bridge.path)&&File.ReadAllText(bridgePath)=="wrong-bridge",
                  "a damaged stored copy is named, never reported repaired");
            Check(!File.Exists(stored)&&Directory.GetFiles(SafeFs.Under(env.DataRoot,"reports/damaged-store")).Any(f=>File.ReadAllText(f)=="VERIFIED BRIDGE"),
                  "the damaged stored copy is kept aside, so a fresh copy can be stored");
            File.WriteAllText(stored,"verified bridge");
            // A file held by another program: reported busy, no temporary file left; the retry finishes.
            File.WriteAllText(extraPath,"damaged");
            using(new FileStream(extraPath,FileMode.Open,FileAccess.Read,FileShare.Read)){
                var held=session.CheckFiles(none);
                Check(!held.Complete&&held.busy.Contains(extra.path)&&held.repaired.Contains(runtimeLabel(v,bridge)),"a held file is reported busy while the others are restored");
            }
            Check(!Directory.EnumerateFiles(env.GameDir,".gtavxr-*.tmp").Any()&&!Directory.EnumerateFiles(runtime,".gtavxr-*.tmp").Any(),"no temporary copy left beside a held file");
            var retry=session.CheckFiles(none);
            Check(retry.Complete&&retry.repaired.SequenceEqual(new[]{extra.path})&&File.ReadAllText(extraPath)=="second game file","the retry finishes");
            // No disk space: nothing written. An interrupted run changes nothing; the retry finishes.
            File.WriteAllText(gamePath,"damaged");
            try{session.CheckFiles(none,1L<<50);throw new Exception("no space accepted");}
            catch(DiskSpaceException){}
            Check(File.ReadAllText(gamePath)=="damaged","no write without room for every file");
            using(var stop=new CancellationTokenSource()){
                stop.Cancel();
                try{session.CheckFiles(stop.Token);throw new Exception("cancel ignored");}catch(OperationCanceledException){}
            }
            Check(File.ReadAllText(gamePath)=="damaged","an interrupted run changes nothing");
            var after=session.CheckFiles(none);
            Check(after.Complete&&after.repaired.SequenceEqual(new[]{game.path})&&layer.Verify().Count==0&&untouched(),
                  "the retry restores the game file; profiles, saves, DLSS models and OFXR files untouched");
        }

        // The runtime folder replaced by a junction to a folder outside the library: refused, nothing written there. Creating
        // the junction is the fixture, not the behaviour: where Windows refuses it (for example "Acceso denegado"), the case is
        // recorded as not exercised with that exact reason, never as a pass or a failure of Check files.
        internal static void RepairJunction(string root) {
            var env=Env.ForFixture(root);
            var lib=new Library(env);
            var game=Blob(env,"GTAVXR_SourceBaseline.exe","verified image");
            var bridge=Blob(env,"gtavxr_native_openxr.dll","verified bridge");
            var v=new VersionManifest{id="repair-junction",files=new List<FileRef>{game},runtime=new List<FileRef>{bridge}};
            lib.SaveVersion(v);
            new GameLayer(env).Apply(v,new List<ModManifest>(),null,false);
            var session=new PlaySession(env);
            string runtime=session.RuntimeDir(v),bridgePath=SafeFs.Under(runtime,bridge.path);
            var none=CancellationNone();
            Check(session.CheckFiles(none).Complete,"fixture starts complete");
            string outside=SafeFs.Under(Path.GetDirectoryName(SafeFs.Root(env.DataRoot)),"outside-runtime-"+Ids.New());
            Directory.Move(runtime,outside);
            var link=System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo("cmd.exe","/c mklink /J \""+runtime+"\" \""+outside+"\""){
                UseShellExecute=false,CreateNoWindow=true,RedirectStandardOutput=true,RedirectStandardError=true});
            var errorText=link.StandardError.ReadToEndAsync();
            string output=link.StandardOutput.ReadToEnd();link.WaitForExit();
            if(link.ExitCode!=0||!Directory.Exists(runtime)||(File.GetAttributes(runtime)&FileAttributes.ReparsePoint)==0){
                if(Directory.Exists(runtime)&&(File.GetAttributes(runtime)&FileAttributes.ReparsePoint)!=0)Directory.Delete(runtime,false);
                if(!Directory.Exists(runtime))Directory.Move(outside,runtime);
                Check(session.CheckFiles(none).Complete&&File.ReadAllText(bridgePath)=="verified bridge","fixture put back after the blocked junction");
                throw new NotExercisedException("fixture blocked: mklink /J exit "+link.ExitCode+": "+(output+" "+errorText.Result).Trim());
            }
            string outsideBridge=SafeFs.Under(outside,bridge.path);File.WriteAllText(outsideBridge,"wrong-bridge");
            try{session.CheckFiles(none);throw new Exception("junction followed");}
            catch(InvalidDataException error){Check(error.Message.Contains("Reparse point"),"a linked runtime folder is refused: "+error.Message);}
            Check(File.ReadAllText(outsideBridge)=="wrong-bridge"&&!Directory.EnumerateFiles(outside,".gtavxr-*.tmp").Any(),"nothing written through the link");
            Directory.Delete(runtime,false);Directory.Move(outside,runtime);
            var restored=session.CheckFiles(none);
            Check(restored.Complete&&File.ReadAllText(bridgePath)=="verified bridge","with the folder back, the retry restores it");
        }

        // 1.0.5 review: a correct game EXE with a null, empty, partial or repeated runtime list in the installed version
        // record is never "complete". This fixture record is not this launcher's package, so nothing can rebuild it: every
        // missing name is reported. Profiles, saves, DLSS models and OFXR files are untouched.
        internal static void RepairInventory(string root) {
            var env=Env.ForFixture(root);
            var lib=new Library(env);
            var game=Blob(env,"GTAVXR_SourceBaseline.exe","verified image");
            var loader=Blob(env,"openxr_loader.dll","verified loader");
            var bridge=Blob(env,"gtavxr_native_openxr.dll","verified bridge");
            var v=new VersionManifest{id=EmbeddedRelease.VersionId,embedded_release=true,source_sha256=new string('A',64),
                files=new List<FileRef>{game},runtime=null,launch=new LaunchContract{session_mode="suite",settings_mode="suite"}};
            lib.SaveVersion(v);
            new GameLayer(env).Apply(v,new List<ModManifest>(),null,false);
            var session=new PlaySession(env);
            var profile=new Profile{id="keep",version=v.id,name="Keep"};lib.SaveProfile(profile);
            string save=SafeFs.Under(lib.UserDir(profile.id),"SGTA50000");File.WriteAllText(save,"story");
            string model=SafeFs.Under(env.Dir("dlss"),"nvngx_dlss.dll");File.WriteAllText(model,"model");
            Func<bool> untouched=()=>File.ReadAllText(save)=="story"&&File.ReadAllText(model)=="model"&&lib.Profile("keep")!=null;
            var none=CancellationNone();
            Func<RepairReport,string,bool> lacks=(r,name)=>r.unavailable.Contains("runtime/"+v.id+"/"+name+" (not in the version record)");
            Check(EmbeddedRelease.InstalledRuntime.Length==14,"the released runtime excludes the development monitor");
            var nul=session.CheckFiles(none);
            Check(!nul.Complete&&!nul.rebuilt&&nul.checkedFiles.SequenceEqual(new[]{game.path})&&lacks(nul,"(runtime list missing)")&&
                  EmbeddedRelease.InstalledRuntime.All(n=>lacks(nul,n)),"runtime:null with a correct EXE: incomplete, every mandatory file named");
            v.runtime=new List<FileRef>();lib.SaveVersion(v);
            var empty=session.CheckFiles(none);
            Check(!empty.Complete&&EmbeddedRelease.InstalledRuntime.All(n=>lacks(empty,n)),"empty runtime: incomplete");
            v.runtime=new List<FileRef>{loader};lib.SaveVersion(v);
            Directory.CreateDirectory(session.RuntimeDir(v));
            var partial=session.CheckFiles(none);
            Check(!partial.Complete&&lacks(partial,"gtavxr_native_openxr.dll")&&!lacks(partial,"openxr_loader.dll")&&
                  partial.repaired.Contains("runtime/"+v.id+"/openxr_loader.dll"),"loader only: the loader is restored, the bridge and the rest are named");
            v.runtime=new List<FileRef>{loader,loader,bridge};lib.SaveVersion(v);
            var twice=session.CheckFiles(none);
            Check(!twice.Complete&&lacks(twice,"openxr_loader.dll (listed twice)")&&twice.checkedFiles.Count(f=>f.EndsWith("openxr_loader.dll"))==1,
                  "a repeated runtime path is a gap, checked once");
            // An earlier embedded release: at least its bridge and, in suite mode, its loader.
            var older=new VersionManifest{id=EmbeddedRelease.Release104Id,embedded_release=true,runtime=new List<FileRef>{loader},
                launch=new LaunchContract{session_mode="suite",settings_mode="suite"}};
            Check(EmbeddedRelease.RuntimeGaps(older).SequenceEqual(new[]{"gtavxr_native_openxr.dll"}),"an earlier release without its bridge is incomplete");
            older.runtime.Add(bridge);
            Check(EmbeddedRelease.RuntimeGaps(older).Count==0&&EmbeddedRelease.RuntimeGaps(new VersionManifest{id="x",runtime=null}).Count==0,
                  "an earlier release with bridge and loader, or a non-embedded version, has no gap");
            // The complete inventory of this release passes.
            v.runtime=EmbeddedRelease.InstalledRuntime.Select(n=>n=="openxr_loader.dll"?loader:n=="gtavxr_native_openxr.dll"?bridge:
                Blob(env,n.Replace('/','-'),"runtime "+n)).Select((f,i)=>new FileRef{path=EmbeddedRelease.InstalledRuntime[i],sha256=f.sha256,bytes=f.bytes}).ToList();
            lib.SaveVersion(v);
            var full=session.CheckFiles(none);
            Check(full.Complete&&full.checkedFiles.Count==1+EmbeddedRelease.InstalledRuntime.Length&&full.unavailable.Count==0,
                "the full inventory is checked and complete; got "+String.Join(";",full.unavailable));
            Check(untouched(),"profiles, saves and DLSS models untouched");
        }
        static string runtimeLabel(VersionManifest v,FileRef f){return "runtime/"+v.id+"/"+f.path;}
        static System.Threading.CancellationToken CancellationNone(){return System.Threading.CancellationToken.None;}

        internal static void Removal(string root) {
            var env=Env.ForFixture(root);
            var lib=new Library(env);
            var made=Blob(env,"fixture-owned.dll","GTAVXR");
            var version=new VersionManifest{id="gtavxr-1-0-0",files=new List<FileRef>{made}};
            lib.SaveVersion(version);
            new GameLayer(env).Apply(version,new List<ModManifest>(),null,false);
            string normal=SafeFs.Under(env.GameDir,"GTA5.exe");
            string save=SafeFs.Under(env.UserDataDir,"player-save");
            File.WriteAllText(normal,"retail");
            File.WriteAllText(save,"normal story");
            var location=new DataLocation(env.DataRoot);
            var roots=CompleteRemoval.Libraries(env,location);
            string aside=env.Dir("sessions/removal-fixture/aside");
            string kept=SafeFs.Under(aside,"user-plugin.asi");
            File.WriteAllText(kept,"player file stored during recovery");
            try { CompleteRemoval.RemoveData(env,location,roots);throw new Exception("removed retained player file"); }
            catch(IOException error){Check(error.Message.Contains(aside),"identify retained player files");}
            Check(File.Exists(kept)&&File.Exists(SafeFs.Under(env.GameDir,made.path)),"removal leaves both layers intact while recovery files remain");
            File.Move(kept,SafeFs.Under(env.GameDir,"user-plugin.asi"));
            CompleteRemoval.RemoveData(env,location,roots);
            Check(!Directory.Exists(env.DataRoot)&&!File.Exists(SafeFs.Under(env.GameDir,made.path))&&
                File.ReadAllText(normal)=="retail"&&File.ReadAllText(save)=="normal story"&&
                File.Exists(SafeFs.Under(env.GameDir,"user-plugin.asi")),"full data cleanup preserves GTA, regular saves and player files");
        }

        internal static void MovedGame(string root) {
            var env=Env.ForFixture(root);
            var lib=new Library(env);
            var payload=Blob(env,"fixture-owned.dll","GTAVXR");
            env.RetailOverride=new Dictionary<string,string>();
            Directory.CreateDirectory(SafeFs.Under(env.GameDir,"update"));
            foreach(string file in new[]{"GTA5.exe","update/update.rpf","update/update2.rpf"}){
                string retail=SafeFs.Under(env.GameDir,file);
                File.WriteAllText(retail,"retail base "+file);
                env.RetailOverride.Add(file,SafeFs.Hash(retail));
            }
            var v=new VersionManifest{id="relocate",files=new List<FileRef>{payload}};
            lib.SaveVersion(v);
            var layer=new GameLayer(env);
            layer.Apply(v,new List<ModManifest>(),null,false);
            string original=env.GameDir,moved=original+"-moved";
            Directory.Move(original,moved);
            string owned=SafeFs.Under(moved,payload.path);
            File.WriteAllText(owned,"foreign bytes");
            try{layer.Relocate(moved);throw new Exception("accepted changed payload");}
            catch(InvalidDataException){}
            Check(layer.State().game_dir==original&&File.ReadAllText(owned)=="foreign bytes","failed relocation leaves both records and files intact");
            new Blobs(env).Materialize(payload,owned);
            layer.Relocate(moved);
            Check(layer.State().game_dir==moved&&layer.Verify().Count==0,"relocation adopts verified files at the new location");
            env.GameDir=moved;
            string unknown=SafeFs.Under(moved,"similar-owned.dll");
            File.WriteAllText(unknown,"external");
            var location=new DataLocation(env.DataRoot);
            var destinations=CompleteRemoval.Libraries(env,location);
            using(var scope=GameScope.Acquire(moved)){
                try{CompleteRemoval.RemoveData(env,location,destinations);throw new Exception("uninstalled locked game");}
                catch(IOException){}
                Check(File.Exists(owned)&&Directory.Exists(env.DataRoot),"busy install survives removal");
            }
            try{layer.Uninstall((step,done,total)=>{throw new IOException("interrupted uninstall");});throw new Exception("expected interruption");}
            catch(IOException error){Check(error.Message=="interrupted uninstall","simulated removal interrupt");}
            Check(layer.Pending,"uninstall journal retained");
            CompleteRemoval.RemoveData(env,location,destinations);
            Check(File.Exists(SafeFs.Under(moved,"GTA5.exe"))&&File.Exists(unknown)&&!File.Exists(owned)
                  &&!Directory.Exists(env.DataRoot),"moved game cleanup retires only owned content");
        }

        internal static void DivergentLibraries(string root) {
            var prior=Env.ForFixture(root);
            prior.SaveSettings(prior.LoadSettings());
            var original=new Library(prior);
            var old=new Profile{id="same-profile",version=EmbeddedRelease.PreviousVersionId};
            string oldUser=original.UserDir(old.id);
            string slot="Default/SGTA50015";
            string oldFile=SafeFs.Under(oldUser,slot);
            Directory.CreateDirectory(Path.GetDirectoryName(oldFile));
            File.WriteAllText(oldFile,"Earlier campaign");
            old.user=UserTree.Snapshot(oldUser,true);
            original.SaveProfile(old);
            string active=Path.Combine(root,"Active library");
            var location=new DataLocation(prior.DataRoot);
            location.CopyAndSelect(prior,active);
            var env=Env.Product(active);
            env.GameDir=prior.GameDir;
            var library=new Library(env);
            var current=library.Profile(old.id);
            string activeFile=SafeFs.Under(library.UserDir(current.id),slot);
            File.WriteAllText(activeFile,"Different campaign");
            current.user=UserTree.Snapshot(library.UserDir(current.id),true);
            library.SaveProfile(current);
            var migration=new ReleaseMigration(env);
            var version=new VersionManifest{id=EmbeddedRelease.VersionId,embedded_release=true};
            try{migration.CheckLibraries(version,current.id,location);throw new Exception("divergent save was ignored");}
            catch(IOException error){Check(error.Message.Contains(old.id),"reported the differing source profile");}
            new Importers(env).MergeStorySaves(current.id,prior.DataRoot,old.id);
            migration.CheckLibraries(version,current.id,location);
            Check(File.ReadAllText(activeFile)=="Different campaign"&&File.ReadAllText(oldFile)=="Earlier campaign"&&
                  library.Profile(current.id).user.Any(file=>file.relative.StartsWith(StoryStart.Archive)&&file.sha256==SafeFs.Hash(oldFile)),
                  "migration can continue after the verified merge preserves both saves");
        }

        internal static void Channel(string root) {
            const string tag="v1.0.3";
            string hash=new string('A',64);
            string json=Json.Encode(new{tag_name=tag,draft=false,prerelease=false,assets=new[]{
                new{name="GTAVXR.exe",state="uploaded",size=1234,digest="sha256:"+hash,
                    browser_download_url=PublicReleaseChannel.Releases+"/download/"+tag+"/GTAVXR.exe"}
            }});
            Check(PublicReleaseChannel.Parse(json,new Version("1.0.2")).Hash==hash,"official newer release with hash");
            Check(PublicReleaseChannel.Parse(json,new Version("1.0.3"))==null,"current release");
            try{PublicReleaseChannel.Parse(json.Replace("github.com","example.com"),new Version("1.0.2"));
                throw new Exception("accepted untrusted endpoint");}
            catch(InvalidDataException){}
            Directory.CreateDirectory(root);
            string file=SafeFs.Under(root,"GTAVXR.exe");
            File.WriteAllText(file,"isolated asset");
            var update=new PublicReleaseChannel.Update{Hash=SafeFs.Hash(file),Bytes=new FileInfo(file).Length};
            PublicReleaseChannel.VerifyDownload(update,file);
            File.WriteAllText(file,"Isolated asset");
            try{PublicReleaseChannel.VerifyDownload(update,file);throw new Exception("accepted changed download");}
            catch(InvalidDataException){}
        }

        internal static object FromPreviousPublicRelease(string root) {
            root=SafeFs.Root(root);
            string fixture=Path.GetFileName(root).ToUpperInvariant();
            string source;
            switch(fixture){
                case "FIXTURE-PUBLIC-100":source=EmbeddedRelease.PreviousVersionId;break;
                case "FIXTURE-PUBLIC-102":source=EmbeddedRelease.Release102Id;break;
                case "FIXTURE-HUD-103":source=EmbeddedRelease.Release103Id;break;
                case "FIXTURE-CANDIDATE-103":source=EmbeddedRelease.Release103Id;break;
                case "FIXTURE-CANDIDATE-104":source=EmbeddedRelease.Release104Id;break;
                case "FIXTURE-CORRECTED-104":source=EmbeddedRelease.Release104Id;break;
                case "FIXTURE-FIX-104":source=EmbeddedRelease.Release104Id;break;
                case "FIXTURE-FIX-104V2":source=EmbeddedRelease.Release104Id;break;
                case "FIXTURE-R4":source=EmbeddedRelease.VersionId;break;
                case "FIXTURE-R5":source=EmbeddedRelease.VersionId;break;
                default:throw new InvalidDataException("Expected an isolated fixture root.");
            }
            string library=SafeFs.Under(root,"suite-data");
            string marker=SafeFs.Under(library,"versions/"+source+".json");
            if(!File.Exists(marker)||!File.Exists(SafeFs.Under(root,"game/GTA5.exe")))
                throw new InvalidDataException("Expected the owned public release fixture.");
            var env=Env.ForFixture(root);
            var store=new Library(env);
            var oldVersion=store.Version(source);
            var sameVersion=source==EmbeddedRelease.VersionId;
            // Profiles of a release with VR action remaps (1.0.4 and this release) keep them byte for byte across the update.
            var keepsRemaps=sameVersion||source==EmbeddedRelease.Release104Id;
            var oldImage=oldVersion.files.Single(f=>f.path==oldVersion.launch.exe).sha256;
            var oldBridge=oldVersion.runtime.Single(f=>f.path=="gtavxr_native_openxr.dll").sha256;
            var previousMonitor=oldVersion.runtime.SingleOrDefault(f=>f.path=="native_fault_monitor.exe");
            if(sameVersion)Check((previousMonitor!=null)==(fixture=="FIXTURE-R4"),"source monitor matches fixture");
            var before=store.Profiles().ToList();
            Check(before.Count>0&&before.All(p=>p.version==source),"source fixture profiles");
            Check(ReleaseMigration.HasPrevious(EmbeddedRelease.VersionId,store.Versions(),before),
                "a changed bundle is visible as an update, including the same version");
            var copy=before[0];
            new Importers(env).Duplicate(copy.id,"Second public profile");
            before=store.Profiles().ToList();
            // An update over the same version keeps each profile's VR controls and settings byte for byte.
            // The profile gets real remaps in the game's own [VR actions] section (vehicle camera on a double
            // Y, phone on a single B) and changed [VR] settings. Copies before and after the update go to
            // bindings/, where the release validation reads them with the game's settings parser.
            var settings=new Dictionary<string,string>();var options=new Dictionary<string,string>();
            string evidence=SafeFs.Under(root,"bindings");
            foreach(var p in before){
                string ini=store.VrSettingsFile(p.id);
                if(keepsRemaps){
                    string text=File.Exists(ini)?File.ReadAllText(ini):"";
                    foreach(var entry in SeededVrSettings)
                        text=SetIni(text,entry[0],entry[1],entry[2]);
                    File.WriteAllText(ini,text,new System.Text.UTF8Encoding(false));
                    p.has_vr_settings=true;
                    store.SaveProfile(p);
                    Directory.CreateDirectory(evidence);
                    File.Copy(ini,SafeFs.Under(evidence,p.id+".before.ini"));
                }
                settings[p.id]=File.Exists(ini)?SafeFs.Hash(ini):"";
                options[p.id]=p.options==null?"":String.Join("\n",p.options.OrderBy(o=>o.Key,StringComparer.Ordinal).Select(o=>o.Key+"="+o.Value));
            }
            var hashes=before.ToDictionary(p=>p.id,p=>UserTree.Snapshot(store.UserDir(p.id))
                .ToDictionary(f=>f.relative,f=>f.sha256));
            var session=new PlaySession(env);
            var layer=new GameLayer(env);
            string picked=before[0].id;
            try {
                session.InstallEmbedded(picked,step=>{
                    if(layer.Pending&&step.step=="game"&&step.done>0)
                        throw new IOException("FIXTURE_INTERRUPTED_UPDATE");
                },System.Threading.CancellationToken.None);
                throw new InvalidOperationException("Expected interruption.");
            } catch(IOException error) { if(error.Message!="FIXTURE_INTERRUPTED_UPDATE")throw; }
            Check(layer.Pending&&new ReleaseMigration(env).Pending!=sameVersion&&File.Exists(marker),
                "update survives interrupted deployment with the appropriate journal");
            Check(session.Recover()=="OK","game-layer recovery");
            Check(session.InstallEmbedded(picked,null,System.Threading.CancellationToken.None)==picked,"resume preserves selected ID");
            Check(store.Profiles().Count==before.Count&&store.Profiles().All(p=>p.version==EmbeddedRelease.VersionId),
                "all profiles migrate without duplication");
            foreach(var entry in hashes)
                Check(UserTree.Snapshot(store.UserDir(entry.Key)).All(f=>entry.Value.ContainsKey(f.relative)&&entry.Value[f.relative]==f.sha256)&&
                      UserTree.Snapshot(store.UserDir(entry.Key)).Count==entry.Value.Count,"all story files survive unchanged");
            if(keepsRemaps)
                foreach(var p in store.Profiles()){
                    string ini=store.VrSettingsFile(p.id);
                    string text=File.Exists(ini)?File.ReadAllText(ini):"";
                    Check(settings.ContainsKey(p.id)&&p.has_vr_settings&&File.Exists(ini)&&SafeFs.Hash(ini)==settings[p.id]&&
                          SeededVrSettings.All(e=>GetIni(text,e[0],e[1])==e[2])&&
                          options[p.id]==String.Join("\n",(p.options??new Dictionary<string,string>()).OrderBy(o=>o.Key,StringComparer.Ordinal).Select(o=>o.Key+"="+o.Value)),
                          "update keeps VR controls and settings: "+p.id);
                    File.Copy(ini,SafeFs.Under(evidence,p.id+".after.ini"));
                }
            var installed=store.Version(EmbeddedRelease.VersionId);
            var currentImage=installed.files.Single(f=>f.path==installed.launch.exe);
            var currentBridge=installed.runtime.Single(f=>f.path=="gtavxr_native_openxr.dll");
            Check(currentImage.sha256!=oldImage&&(sameVersion?currentBridge.sha256==oldBridge:currentBridge.sha256!=oldBridge)&&
                  SafeFs.Hash(SafeFs.Under(env.GameDir,currentImage.path))==currentImage.sha256&&
                  SafeFs.Hash(SafeFs.Under(library,"runtime/"+installed.id+"/"+currentBridge.path))==currentBridge.sha256,
                  "new game and expected bridge bytes verified");
            if(sameVersion){
                Check(!installed.runtime.Any(f=>f.path=="native_fault_monitor.exe")&&
                      !File.Exists(SafeFs.Under(library,"runtime/"+installed.id+"/native_fault_monitor.exe"))&&
                      (previousMonitor==null||!File.Exists(new Blobs(env).PathOf(previousMonitor.sha256))),
                      "old monitor and its exclusive stored copy are absent");
            }
            Check((sameVersion||(!File.Exists(marker)&&!Directory.Exists(SafeFs.Under(library,"runtime/"+source))))&&
                !new ReleaseMigration(env).Pending&&layer.Verify().Count==0,"replacement verified");
            string retail=SafeFs.Under(root,"game/GTA5.exe");
            string hash=SafeFs.Hash(retail);
            var location=new DataLocation(env.DataRoot);
            CompleteRemoval.RemoveData(env,location,CompleteRemoval.Libraries(env,location));
            Check(!Directory.Exists(library)&&SafeFs.Hash(retail)==hash&&
                  !File.Exists(SafeFs.Under(root,"game/GTAVXR_SourceBaseline.exe")),"complete deletion preserves retail");
            return new{status="PASS_MISSION_SAFE_RELEASE_MIGRATION",from=source,to=EmbeddedRelease.VersionId,profiles=before.Count,interrupted=true,
                complete_data_removal=true,gta_launched=false,real_game_touched=false,
                bindings=keepsRemaps?before.Select(p=>p.id).ToArray():new string[0]};
        }

        // Remaps and settings that differ from the game's defaults: section, key, value.
        static readonly string[][] SeededVrSettings={
            new[]{"VR actions","vehicle_camera","10"},new[]{"VR actions","phone","2"},
            new[]{"VR","snap_degrees","45"},new[]{"VR","smooth","0"},new[]{"VR","cutscene_view","1"}};

        // Sets key=value inside [section], as the profile file API does: an existing key is replaced in
        // place, a new one goes at the end of its section, a missing section is added at the end.
        internal static string SetIni(string text,string section,string key,string value){
            string nl=text.Contains("\r\n")?"\r\n":"\n";
            var lines=text.Length==0?new List<string>():text.Replace("\r\n","\n").Split('\n').ToList();
            if(lines.Count>0&&lines[lines.Count-1].Length==0)lines.RemoveAt(lines.Count-1);
            int start=lines.FindIndex(l=>IsSection(l,section));
            if(start<0){lines.Add("["+section+"]");lines.Add(key+"="+value);}
            else{
                int end=start+1;
                while(end<lines.Count&&!lines[end].TrimStart().StartsWith("["))end++;
                int at=-1;
                for(int i=start+1;i<end;i++)if(IsKey(lines[i],key)){at=i;break;}
                if(at>=0)lines[at]=key+"="+value;
                else{int last=end;while(last>start+1&&lines[last-1].Trim().Length==0)last--;lines.Insert(last,key+"="+value);}
            }
            return String.Join(nl,lines)+nl;
        }
        // The value the profile file API reads: the first [section] and its first key.
        internal static string GetIni(string text,string section,string key){
            var lines=text.Replace("\r\n","\n").Split('\n');
            int start=Array.FindIndex(lines,l=>IsSection(l,section));
            if(start<0)return null;
            for(int i=start+1;i<lines.Length&&!lines[i].TrimStart().StartsWith("[");i++)
                if(IsKey(lines[i],key))return lines[i].Substring(lines[i].IndexOf('=')+1).Trim();
            return null;
        }
        static bool IsSection(string line,string section){return String.Equals(line.Trim(),"["+section+"]",StringComparison.OrdinalIgnoreCase);}
        static bool IsKey(string line,string key){int eq=line.IndexOf('=');return eq>0&&String.Equals(line.Substring(0,eq).Trim(),key,StringComparison.OrdinalIgnoreCase);}
    }
}
