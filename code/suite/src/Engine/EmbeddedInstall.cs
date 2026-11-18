using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;

namespace GtavxrSuite {
    sealed class PreparedSelection {
        public List<ModManifest> mods { get; set; }
        public Dictionary<string,string> settings_xml { get; set; }
    }

    sealed class InstalledEmbeddedRelease {
        public string bundle_sha256 { get; set; }
        public string game { get; set; }
        public List<string> retail_stamp { get; set; }
    }

    public sealed partial class PlaySession {
        public string InstallEmbedded(string profileId, Action<PlayProgress> progress, CancellationToken cancel) {
            return InstallEmbedded(profileId,progress,cancel,new PlayOptions());
        }

        public string InstallEmbedded(string profileId,Action<PlayProgress> progress,CancellationToken cancel,PlayOptions options) {
            if (GameRunning()) throw new InvalidOperationException("GAME_RUNNING");
            if (layer.Pending || Unfinished().Count != 0) throw new InvalidOperationException("RECOVERY_REQUIRED");
            using (var library = GameScope.Acquire(env.DataRoot))
            using (var game = GameScope.Acquire(env.GameDir))
            using (var guard = new FileStream(SafeFs.Under(env.DataRoot,"embedded-install.lock"),
                                              FileMode.OpenOrCreate, FileAccess.ReadWrite, FileShare.None))
            using (var lease = env.DevHost?PlayLease.Acquire(env.DevRepo,Ids.New(),"GTAVXR Suite: Install"):null) {
                if(GameRunning()||layer.Pending||Unfinished().Count!=0)throw new InvalidOperationException("RECOVERY_REQUIRED");
                return InstallSelected(profileId,progress,cancel,options);
            }
        }

        string InstallSelected(string profileId, Action<PlayProgress> progress, CancellationToken cancel,PlayOptions options) {
            cancel.ThrowIfCancellationRequested();
            var migration=new ReleaseMigration(env);
            string bundleIdentity=EmbeddedRelease.DescriptorIdentity(cancel);
            // An update from another release that stopped half way is resolved first, before this release is imported:
            // its journal is verified and, when it is older, set aside for this installer's own plan.
            migration.ResolvePending(EmbeddedRelease.VersionId,bundleIdentity);
            migration.CheckDowngrade(EmbeddedRelease.VersionId);
            var imports = new Importers(env);
            var version = lib.Version(EmbeddedRelease.VersionId);
            bool rebuild=version==null||!version.embedded_release||version.source_sha256!=bundleIdentity||
                PackageRecordIncomplete(version)||EmbeddedRelease.RuntimeGaps(version).Count!=0;
            if(!rebuild){
                foreach(var file in version.files.Concat(version.runtime).GroupBy(f=>f.sha256).Select(group=>group.First())){
                    cancel.ThrowIfCancellationRequested();
                    if(!StoredCopyVerifies(file)){rebuild=true;break;}
                }
            }
            if(rebuild)version=RebuildFromPackage(env.GameDir,bundleIdentity,cancel,progress);
            else EmbeddedRelease.VerifyRetailInputs(env.GameDir,cancel);
            migration.CheckLibraries(version,profileId);
            string migrateProfile=migration.SelectProfile(version,profileId);
            var previous = lib.Profile(migrateProfile??profileId);
            bool newProfile = previous==null || previous.version!=version.id;
            var profile = newProfile ? new Profile { version=version.id, mods=previous==null?new List<string>():previous.mods,
                                                     options=previous==null?null:previous.options } : previous;
            MaterializeRuntime(version);
            var diagnostics=new List<string>();
            var omitted=new List<string>(options.OmittedMods);
            var inputs=CompatibleMods(profile,version,options,diagnostics,omitted);
            if(diagnostics.Count!=0)
                throw new OptionalComponentException(diagnostics,new PlayOptions { OmittedMods=omitted });
            var prepared = BuildSelection(profile,version,inputs,progress);
            var deploymentMods=prepared.mods.Where(m=>m.kind!="script").ToList();
            var verifiedFiles=deploymentMods.SelectMany(m=>m.files??new List<FileRef>());
            foreach(var file in verifiedFiles.GroupBy(f=>f.sha256).Select(group=>group.First())){
                cancel.ThrowIfCancellationRequested();
                blobs.Verify(file.sha256,file.bytes);
            }
            cancel.ThrowIfCancellationRequested();
            var verified = LegacyGamePolicy.VerifyFiles(env.GameDir,env.RetailOverride??version.retail);
            if (verified.verified.Single(row=>row.path=="GTA5.exe").sha256!=EmbeddedRelease.RetailIdentity)
                throw new InvalidDataException("DELTA_BASE_MISMATCH");
            if (GameRunning()) throw new InvalidOperationException("GAME_RUNNING");
            migration.Prepare(version,previous==null?null:previous.id);
            layer.Apply(version,deploymentMods,
                        (what,done,total)=>Step(progress,"game",done,total),false);
            if (newProfile && migrateProfile==null) {
                if (previous==null) profile=imports.CreateProfile(L.Lang=="en"?"VR Story":"Historia VR",version.id,true,null);
                else profile=imports.CopyForEmbeddedRelease(previous,version.id);
            }
            env.Dir("embedded");
            Json.Atomic(InstalledPath(env,version.id),new InstalledEmbeddedRelease {
                bundle_sha256=version.source_sha256, game=SafeFs.Root(env.GameDir), retail_stamp=RetailStamp()
            });
            if(!env.Fixture)SelfInstall.RefreshInstalledLauncher();
            migration.Finish(version);
            migration.RetirePublicMonitor(version);
            LastUpdateKept=migration.Kept??new List<string>();
            return migrateProfile??profile.id;
        }
        // Altered files of retired releases the last update kept aside (reports/damaged-store) instead of deleting.
        public List<string> LastUpdateKept { get; private set; }

        static string InstalledPath(Env env,string versionId) {
            Ids.Check(versionId);
            return SafeFs.Under(env.DataRoot,"embedded/"+versionId+".json");
        }

        internal static void ForgetEmbeddedInstallation(Env env) {
            string path=InstalledPath(env,EmbeddedRelease.VersionId);
            if(File.Exists(path)) SafeFs.DeleteName(path);
        }

        List<string> RetailStamp() {
            return LegacyGamePolicy.Legacy3889().Keys.Concat(new[]{"common.rpf"}).OrderBy(path=>path,StringComparer.Ordinal).Select(path=>{
                var file=new FileInfo(SafeFs.Under(env.GameDir,path));
                if (!file.Exists) throw new InvalidDataException("INSTALL_BASE_CHANGED:"+path);
                return path+"|"+file.Length+"|"+file.LastWriteTimeUtc.Ticks;
            }).ToList();
        }

        void RequireInstalledEmbedded(VersionManifest version) {
            string path=InstalledPath(env,version.id);
            if (!File.Exists(path) || layer.State()==null) throw new InvalidDataException("INSTALL_REQUIRED");
            var installed=Json.Read<InstalledEmbeddedRelease>(path);
            if (installed.bundle_sha256!=version.source_sha256 ||
                !String.Equals(installed.game,SafeFs.Root(env.GameDir),StringComparison.OrdinalIgnoreCase))
                throw new InvalidDataException("INSTALL_REQUIRED");
            if (version.id==EmbeddedRelease.VersionId && EmbeddedRelease.Available &&
                version.source_sha256!=EmbeddedRelease.DescriptorIdentity(CancellationToken.None))
                throw new InvalidDataException("INSTALL_REQUIRED");
            var stamps=RetailStamp();
            if(installed.retail_stamp==null) throw new InvalidDataException("INSTALL_REQUIRED");
            string changed=stamps.Except(installed.retail_stamp,StringComparer.Ordinal).FirstOrDefault();
            if(changed!=null) throw new InvalidDataException("INSTALL_BASE_CHANGED:"+changed.Split('|')[0]);
            string retailExe=SafeFs.Under(env.GameDir,"GTA5.exe");
            if(!String.Equals(SafeFs.Hash(retailExe),EmbeddedRelease.RetailIdentity,StringComparison.Ordinal))
                throw new InvalidDataException("DELTA_BASE_MISMATCH");
            // 1.0.5 review: a record without the release's mandatory runtime never starts the game (Check files rebuilds it).
            var gaps=EmbeddedRelease.RuntimeGaps(version);
            if(gaps.Count!=0) throw new InvalidDataException("INSTALL_RUNTIME_INCOMPLETE:"+gaps[0]);
            if(PackageRecordIncomplete(version))throw new InvalidDataException("INSTALL_REQUIRED");
            string runtime=RuntimeRoot(version);
            foreach(var file in version.runtime) {
                try { SafeFs.AssertFile(SafeFs.Under(runtime,file.path),file.sha256,file.bytes); }
                catch(InvalidDataException error) { throw new InvalidDataException("INSTALL_RUNTIME_CHANGED:"+file.path,error); }
            }
            var active=layer.State();
            if(active!=null&&active.version==version.id) {
                var exe=version.files.Single(f=>f.path==version.launch.exe);
                SafeFs.AssertFile(SafeFs.Under(env.GameDir,exe.path),exe.sha256,exe.bytes);
            }
        }

        PreparedSelection BuildSelection(Profile profile, VersionManifest version, List<ModManifest> inputs, Action<PlayProgress> progress) {
            var prepared=new PreparedSelection { mods=inputs,
                                                 settings_xml=new Dictionary<string,string>() };
            var sources=inputs.Where(m=>m.kind=="content-source").ToList();
            if (sources.Count==0) return prepared;
            var content=new ContentSources(env);
            Step(progress,"content",0,0);
            var built=content.Build(profile,version,sources,(done,total)=>Step(progress,"content",done,total));
            prepared.settings_xml=content.Plan(profile,sources).SettingsXml;
            prepared.mods=inputs.Where(m=>m.kind!="content-source").ToList();
            if (built!=null) prepared.mods.Add(built);
            if (prepared.mods.Count(m=>m.kind=="content-native")>1) throw new InvalidDataException("CONTENT_PROFILE_TWICE");
            return prepared;
        }
    }
}
