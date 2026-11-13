using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;

namespace GtavxrSuite {
    public static class EmbeddedInstallTests {
        static void Check(bool pass, string name) {
            if (!pass) throw new InvalidDataException("FIXTURE:"+name);
        }

        static void Expect<T>(Action action, string text) where T : Exception {
            try { action(); }
            catch (T error) { if (text==null || error.Message==text) return; throw; }
            throw new InvalidDataException("FIXTURE_EXPECTED:"+text);
        }

        public static object Run(string fresh, string retailExe, bool shaderDeltas = false, string optionalArchive = null,
                                 string mvgaFixtures = null) {
            fresh=SafeFs.Root(fresh);
            if (Directory.Exists(fresh) || File.Exists(fresh)) throw new InvalidDataException("Fresh test path required");
            var env=Env.ForFixture(fresh);
            var session=new PlaySession(env);
            var layer=new GameLayer(env);
            var library=new Library(env);
            var passed=new List<string>();
            var original=SafeFs.Under(env.GameDir,"GTA5.exe");
            var newExe=SafeFs.Under(env.GameDir,"GTAVXR_SourceBaseline.exe");
            File.Copy(retailExe,original);
            byte lead;
            using (var stream=File.Open(original,FileMode.Open,FileAccess.ReadWrite)) {
                lead=(byte)stream.ReadByte();stream.Position=0;stream.WriteByte((byte)(lead^1));
            }
            Expect<InvalidDataException>(()=>session.InstallEmbedded(null,null,CancellationToken.None),"DELTA_BASE_MISMATCH");
            Check(layer.State()==null && !File.Exists(newExe),"same-size wrong retail leaves game unmodified");
            using (var stream=File.OpenWrite(original)) stream.WriteByte(lead);
            passed.Add("same-size wrong retail fails before activation");

            string retailRoot=Path.GetDirectoryName(SafeFs.Root(retailExe));
            var archives=shaderDeltas?new[]{"common.rpf","update/update.rpf","update/update2.rpf"}:
                                     new[]{"common.rpf","update/update2.rpf"};
            foreach(string archive in archives){
                string from=SafeFs.Under(retailRoot,archive);
                if(!File.Exists(from)){
                    Check(!shaderDeltas,"retail copy input "+archive);
                    continue;
                }
                string to=SafeFs.Under(env.GameDir,archive);
                Directory.CreateDirectory(Path.GetDirectoryName(to));
                File.Copy(from,to);
            }

            var hashes=new Dictionary<string,string>();
            foreach (string path in LegacyGamePolicy.Legacy3889().Keys) {
                string file=SafeFs.Under(env.GameDir,path);
                if (!File.Exists(file)) {
                    Directory.CreateDirectory(Path.GetDirectoryName(file));
                    File.WriteAllText(file,"RPF7 synthetic control "+path);
                }
                hashes[path]=SafeFs.Hash(file);
            }
            if(!shaderDeltas) env.RetailOverride=new Dictionary<string,string>(hashes);
            string common=SafeFs.Under(env.GameDir,"common.rpf");
            if(File.Exists(common))hashes.Add("common.rpf",SafeFs.Hash(common));
            string save=SafeFs.Under(env.UserDataDir,"settings.xml");
            File.WriteAllText(save,EngineTests.SettingsXml);
            string saveHash=SafeFs.Hash(save);
            const string saveRelative="Profiles/ABCD/SGTA50000";
            string storySave=SafeFs.Under(env.UserDataDir,saveRelative);
            Directory.CreateDirectory(Path.GetDirectoryName(storySave));
            File.WriteAllBytes(storySave,EngineTests.FakeSave("Franklin and Lamar (2.0%)",8192));
            string storyHash=SafeFs.Hash(storySave);
            string personal=SafeFs.Under(env.GameDir,"fixture-user-file.txt");
            File.WriteAllText(personal,"Fixture unmanaged file");
            hashes["fixture-user-file.txt"]=SafeFs.Hash(personal);
            var originalDirs=Directory.GetDirectories(env.GameDir,"*",SearchOption.AllDirectories);
            using (var cancel=new CancellationTokenSource()) {
                cancel.Cancel();
                Expect<OperationCanceledException>(()=>session.InstallEmbedded(null,null,cancel.Token),null);
            }
            Check(!File.Exists(newExe),"cancelled fixture has no installation");
            passed.Add("cancel before activation");

            if (shaderDeltas) {
                RejectDamagedShaderBase(env, session, layer, library, newExe, hashes["update/update2.rpf"]);
                passed.Add("same-size altered RPF shader is rejected before import or activation");
                if(optionalArchive!=null) {
                    string source=Path.Combine(Path.GetDirectoryName(retailExe),"GTA5.exe");
                    string collision=SafeFs.Under(env.GameDir,"common/shaders/win32_40/default.fxc");
                    string owner=Path.GetDirectoryName(collision);
                    Directory.CreateDirectory(owner);
                    string stage=SafeFs.Under(fresh,"collision-stage");
                    EmbeddedRelease.Reconstruct(env.GameDir,stage,CancellationToken.None);
                    File.Copy(SafeFs.Under(stage,"common/shaders/win32_40/default.fxc"),collision);
                    string collisionHash=SafeFs.Hash(collision);
                    Expect<GameConflictException>(()=>session.InstallEmbedded(null,null,CancellationToken.None),null);
                    Check(SafeFs.Hash(collision)==collisionHash && layer.State()==null && !layer.Pending &&
                          SafeFs.Hash(original)==SafeFs.Hash(source),"identical unmanaged file is never adopted");
                    layer.Uninstall(null);
                    Check(SafeFs.Hash(collision)==collisionHash,"uninstall does not delete an identical unmanaged file");
                    File.Delete(collision);
                    Directory.Delete(owner);
                    Directory.Delete(Path.GetDirectoryName(owner));
                    Directory.Delete(Path.GetDirectoryName(Path.GetDirectoryName(owner)));
                    passed.Add("identical pre-existing file stays owned by the player through install and uninstall");
                }
                Expect<IOException>(()=>session.InstallEmbedded(null, progress=>{
                    if(layer.Pending && progress.step=="game" && progress.done>0)
                        throw new IOException("FIXTURE_INTERRUPTED_FIRST_INSTALL");
                }, CancellationToken.None),"FIXTURE_INTERRUPTED_FIRST_INSTALL");
                Check(layer.Pending && File.Exists(newExe),"first installation interrupted after writing game files");
                Check(new PlaySession(env).Recover()=="OK" && !layer.Pending,"recover first install");
                Check(layer.State().files.Count==1103,"recovered full first installation");
                VerifyFiles(env.GameDir,layer.State().files);
                passed.Add("fresh installation recovers the complete base from the durable journal");
            }
            string profileId=session.InstallEmbedded(null,null,CancellationToken.None);
            var version=library.Version(EmbeddedRelease.VersionId);
            Check(version.embedded_release && layer.State().files.Count==1103 && version.runtime.Count==15,"full base and runtime");
            var targetHash=version.files.Single(f=>f.path==version.launch.exe).sha256;
            Check(SafeFs.Hash(newExe)==targetHash,"installed target identity");
            string runtime=SafeFs.Under(env.DataRoot,"runtime/"+version.id);
            var table=version.runtime.Single(f=>f.path=="scripthost/ScriptHookVDotNet.gtavxr-memory.txt");
            Check(SafeFs.Hash(SafeFs.Under(runtime,table.path))==table.sha256,"memory table");
            passed.Add("embedded install exact EXE and same-release Host table");

            var profile=library.Profile(profileId);
            string calibration=library.VrSettingsFile(profileId);
            string calibrationHash=SafeFs.Hash(calibration);
            Check(profile.user.Any(f=>f.relative==saveRelative && f.sha256==storyHash) &&
                  SafeFs.Hash(SafeFs.Under(library.UserDir(profileId),saveRelative))==storyHash,
                  "Legacy save is copied into the initial VR profile");
            passed.Add("classic first install copies Legacy story saves into the initial profile");
            if(optionalArchive!=null) {
                Check(!library.Mods().Any(m=>m.family=="mvga"),"MVGA is not redistributed");
                var imports=new Importers(env);
                string id=imports.ImportArchive(optionalArchive).id;
                Check(imports.ImportArchive(optionalArchive).id==id,"MVGA import is idempotent");
                passed.Add("Mods > Add reproduces the optional MVGA profile from the original user's ZIP");
            }
            if(mvgaFixtures!=null) {
                var imports=new Importers(env);
                var originalMvga=library.Mods().Single(m=>m.family=="mvga");
                var repacked=imports.ImportArchive(SafeFs.Under(mvgaFixtures,"repacked.zip"));
                Check(repacked.id==originalMvga.id && repacked.files.Count==31 &&
                      repacked.files.All(f=>originalMvga.files.Any(o=>o.path==f.path&&o.sha256==f.sha256)),"repacked MVGA output identity");
                Expect<InvalidDataException>(()=>imports.ImportArchive(SafeFs.Under(mvgaFixtures,"changed.zip")),"MVGA_INPUT:content/hbaosettings.xml");
                foreach(string bad in new[]{"duplicate.zip","missing.zip"})
                    Expect<InvalidDataException>(()=>imports.ImportArchive(SafeFs.Under(mvgaFixtures,bad)),"MVGA_MEMBER:content/hbaosettings.xml");
                Expect<InvalidDataException>(()=>imports.ImportArchive(SafeFs.Under(mvgaFixtures,"ambiguous.zip")),"MVGA_ARCHIVE_AMBIGUOUS");
                Check(library.Mods().Count(m=>m.family=="mvga")==1,"invalid optional archive never changes the library selection");
                passed.Add("MVGA accepts repacked identical members and rejects changed, duplicate, missing or ambiguous inputs");
            }
            var mvga=library.Mods().Single(m=>m.family=="mvga");
            profile.mods.Add(mvga.id);
            library.SaveProfile(profile);
            session=new PlaySession(env);
            Expect<IOException>(()=>session.InstallEmbedded(profileId,progress=>{
                if(progress.step=="game"&&progress.done>0)throw new IOException("FIXTURE_INTERRUPTED_ACTIVATION");
            },CancellationToken.None),"FIXTURE_INTERRUPTED_ACTIVATION");
            Check(layer.Pending,"durable apply journal after injected cut");
            Check(new PlaySession(env).Recover()=="OK" && !layer.Pending,"recover interrupted install");
            Check(layer.State().files.Count==1134 && layer.State().mods.Contains(mvga.id),"recovered optional content");
            if (shaderDeltas) VerifyFiles(env.GameDir,layer.State().files);
            passed.Add("durable apply journal recovers an interrupted activation after reopening");
            session.InstallEmbedded(profileId,null,CancellationToken.None);
            Check(layer.State().files.Count==1134 && layer.State().mods.Contains(mvga.id),"MVGA activation");
            Check(SafeFs.Hash(calibration)==calibrationHash && SafeFs.Hash(save)==saveHash,"profile and player data preserved");
            passed.Add("reopen and activate optional MVGA without changing saves/calibration");

            // Observe the normal Play process request while refusing to start it. No game process is created.
            string runtimeStub=SafeFs.Under(fresh,"openxr/fixture.dll");
            NeuralSuiteTests.FakeDll(runtimeStub,"Fixture","OpenXR fixture","1.0.0.0",new[] { "xrNegotiateLoaderRuntimeInterface" });
            Json.New(env.OpenXrManifest,new { runtime=new { name="Fixture OpenXR", library_path="fixture.dll" } });
            bool sawLaunch=false;
            DateTime executableWrite=File.GetLastWriteTimeUtc(newExe);
            session.Starter=process=>{
                sawLaunch=process.FileName==newExe && process.Arguments.Contains("-gtavxrMvga");
                throw new IOException("FIXTURE_DO_NOT_START_GAME");
            };
            Expect<IOException>(()=>session.Play(profileId,null),"FIXTURE_DO_NOT_START_GAME");
            Check(sawLaunch && File.GetLastWriteTimeUtc(newExe)==executableWrite && SafeFs.Hash(save)==saveHash,"play uses prepared install");
            passed.Add("Play reaches the expected launch contract without reinstalling; fixture intercepts process creation");
            if(mvgaFixtures!=null) {
                VerifyOptionalPlay(env,session,library,version,profileId,calibration,calibrationHash,newExe);
                passed.Add("optional mod and graphics failures offer a one-session retry and keep the profile");
                VerifyRetailUpdate(env,session,profileId,executableWrite,newExe);
                passed.Add("retail timestamp and unrelated container changes revalidate without reinstalling or changing Legacy");
                RejectDamagedShaderBase(env,session,layer,library,newExe,hashes["update/update2.rpf"],profileId);
                passed.Add("a changed consumed shader blocks revalidation even when the bundle is cached");
            }
            if(optionalArchive!=null) {
                var alternate=new Importers(env).CreateProfile("Alternate",version.id,false,null);
                sawLaunch=false;
                session.Starter=process=>{
                    sawLaunch=process.FileName==newExe && !process.Arguments.Contains("-gtavxrMvga");
                    throw new IOException("FIXTURE_DO_NOT_START_GAME");
                };
                Expect<IOException>(()=>session.Play(alternate.id,null),"FIXTURE_DO_NOT_START_GAME");
                Check(sawLaunch && layer.State().files.Count==1103 && File.GetLastWriteTimeUtc(newExe)==executableWrite,
                      "profile switch removes only optional files and never reapplies the executable");
                passed.Add("classic profile switching works through Play without rerunning delta installation");
                var host=version.runtime.Single(f=>f.path=="GTAVXRHost.dll");
                string hostPath=SafeFs.Under(runtime,host.path);
                using(var stream=File.Open(hostPath,FileMode.Open,FileAccess.ReadWrite)) {
                    int first=stream.ReadByte();stream.Position=0;stream.WriteByte((byte)(first^1));
                }
                sawLaunch=false;
                Expect<InvalidDataException>(()=>session.Play(alternate.id,null),"INSTALL_RUNTIME_CHANGED:GTAVXRHost.dll");
                Check(!sawLaunch,"changed runtime is rejected before the process request");
                session.InstallEmbedded(alternate.id,null,CancellationToken.None);
                SafeFs.AssertFile(hostPath,host.sha256,host.bytes);
                passed.Add("same-size runtime change blocks Play and Install repairs the module");
                string installedPath=SafeFs.Under(env.DataRoot,"embedded/"+version.id+".json");
                var installed=Json.Read<InstalledEmbeddedRelease>(installedPath);
                var stale=Json.Read<InstalledEmbeddedRelease>(installedPath);
                stale.bundle_sha256=new String('A',64);
                version.source_sha256=stale.bundle_sha256;
                library.SaveVersion(version);
                Json.Atomic(installedPath,stale);
                try { Expect<InvalidDataException>(()=>session.Play(alternate.id,null),"INSTALL_REQUIRED"); }
                finally {
                    version.source_sha256=installed.bundle_sha256;
                    library.SaveVersion(version);
                    Json.Atomic(installedPath,installed);
                }
                passed.Add("a matching receipt for an older embedded bundle cannot start this release");
                profileId=alternate.id;
            }

            if (shaderDeltas) {
                Expect<IOException>(()=>layer.Uninstall((what,done,total)=>{
                    if(what=="remove" && done>0)throw new IOException("FIXTURE_INTERRUPTED_UNINSTALL");
                }),"FIXTURE_INTERRUPTED_UNINSTALL");
                Check(layer.Pending && new PlaySession(env).Recover()=="OK" && !layer.Pending,"recover interrupted uninstall");
                passed.Add("interrupted uninstall resumes from its journal");
            } else layer.Uninstall(null);
            Check(!File.Exists(newExe) && hashes.All(x=>SafeFs.Hash(SafeFs.Under(env.GameDir,x.Key))==x.Value) &&
                  SafeFs.Hash(calibration)==calibrationHash && SafeFs.Hash(save)==saveHash &&
                  SafeFs.Hash(storySave)==storyHash,"uninstall restoration");
            if (shaderDeltas) {
                Check(layer.State()==null && Directory.GetFiles(env.GameDir,"*",SearchOption.AllDirectories).Length==hashes.Count,
                      "uninstall restores complete game file inventory");
                Check(new HashSet<string>(originalDirs).SetEquals(Directory.GetDirectories(env.GameDir,"*",SearchOption.AllDirectories)),
                      "uninstall restores complete game directory inventory");
            }
            passed.Add("uninstall removes managed files and preserves retail, profiles and calibration");
            if(optionalArchive!=null) {
                Expect<InvalidDataException>(()=>session.Play(profileId,null),"INSTALL_REQUIRED");
                var other=library.Version(EmbeddedRelease.VersionId);
                other.id="fixture-alternate";
                other.embedded_release=false;
                layer.Apply(other,new List<ModManifest>(),null,false);
                Expect<InvalidDataException>(()=>session.Play(profileId,null),"INSTALL_REQUIRED");
                Check(layer.State().version==other.id,"Play does not change the other installed version");
                layer.Uninstall(null);
                passed.Add("Play requires a fresh Install after explicit uninstall even when another version is installed");
            }
            return new { status="PASS_PRIVATE_EMBEDDED_INSTALL_FIXTURE", cases=passed, exe_sha256=targetHash,
                         gta_launched=false, real_game_touched=false };
        }

        static void VerifyFiles(string root,IEnumerable<FileRef> files) {
            foreach(var file in files) Check(SafeFs.Hash(SafeFs.Under(root,file.path))==file.sha256,"installed file "+file.path);
        }

        static void VerifyOptionalPlay(Env env,PlaySession session,Library library,VersionManifest version,string profileId,
                                       string calibration,string calibrationHash,string exe) {
            var blobs=new Blobs(env);
            string synthetic=SafeFs.Under(env.Dir("scratch"),"broken-managed.dll");
            File.WriteAllText(synthetic,"A malformed DLL");
            var bad=new ModManifest{id="fixture-bad-script",name="Broken script",kind="script",loadable=true,script_api="v2",
                script_files=new List<FileRef>{blobs.PutFile(synthetic,"Broken.dll")}};
            library.SaveMod(bad);
            var profile=library.Profile(profileId);
            profile.mods.Add(bad.id);library.SaveProfile(profile);
            byte[] previous=File.ReadAllBytes(calibration);
            File.WriteAllText(calibration,Graphics.Write(File.ReadAllText(calibration),Graphics.For("neural",Graphics.Read(""))));
            string saved=SafeFs.Hash(calibration);
            var expected=profile.mods.ToArray();
            OptionalComponentException choice=null;
            bool started=false;
            session.Starter=_=>{started=true;throw new IOException("UNEXPECTED_LAUNCH");};
            try { session.Play(profileId,null); }
            catch(OptionalComponentException error){choice=error;}
            Check(!started&&choice!=null&&choice.Retry.OmittedMods.SequenceEqual(new[]{bad.id})&&choice.Retry.GraphicsFallback=="native",
                  "optional failure before process staging");
            string script=SafeFs.Under(env.GameDir,"sCrIpThOoKv.DlL");
            File.WriteAllText(script,"Fixture newer Script Hook V");
            string identity=SafeFs.Hash(script);
            session.Starter=process=>{
                started=true;
                Check(process.FileName==exe&&process.Arguments.Contains("-gtavxrMvga")&&
                      !process.Arguments.Contains("-gtavxrScriptHost")&&!File.Exists(script),"only the affected optional was omitted and retail Script Hook V was aside");
                string vr=process.EnvironmentVariables["GTAVXR_VR_SETTINGS"];
                Check(!Graphics.Read(File.ReadAllText(vr)).NeedsDlss()&&process.EnvironmentVariables["GTAVXR_DLSS_DIR"]==null,
                      "base VR fallback applied for one session");
                throw new IOException("FIXTURE_DO_NOT_START_GAME");
            };
            Expect<IOException>(()=>session.Play(profileId,null,choice.Retry),"FIXTURE_DO_NOT_START_GAME");
            Check(started&&SafeFs.Hash(script)==identity&&SafeFs.Hash(calibration)==saved&&library.Profile(profileId).mods.SequenceEqual(expected),
                  "retail script and saved profile restored");
            File.Delete(script);
            File.WriteAllBytes(calibration,previous);
            Check(SafeFs.Hash(calibration)==calibrationHash,"original test calibration restored");
            profile.mods.Remove(bad.id);library.SaveProfile(profile);
            var settings=env.LoadSettings();
            var dlss=env.Dir("fixture-dlss");
            NeuralSuiteTests.FakeDll(SafeFs.Under(dlss,Dlss.SrModel),"NVIDIA Corporation","DLSS renamed metadata","311.8.0.0",Dlss.Exports);
            settings.dlss_dir=dlss;env.SaveSettings(settings);
            File.WriteAllText(calibration,Graphics.Write(File.ReadAllText(calibration),Graphics.For("neural",Graphics.Read(""))));
            choice=null;
            try { session.Play(profileId,null); }
            catch(OptionalComponentException error){choice=error;}
            Check(choice!=null&&choice.Retry.GraphicsFallback=="performance"&&choice.Retry.OmittedMods.Count==0,
                  "SR can remain enabled when NR is unavailable");
            session.Starter=process=>{
                var effective=Graphics.Read(File.ReadAllText(process.EnvironmentVariables["GTAVXR_VR_SETTINGS"]));
                Check(effective.upscale=="quality"&&!effective.neural&&process.EnvironmentVariables["GTAVXR_DLSS_DIR"]==dlss,
                      "SR-only session retains selected quality");
                throw new IOException("FIXTURE_DO_NOT_START_GAME");
            };
            Expect<IOException>(()=>session.Play(profileId,null,choice.Retry),"FIXTURE_DO_NOT_START_GAME");
            NeuralSuiteTests.FakeDll(SafeFs.Under(dlss,Dlss.NrModel),"NVIDIA Corporation","Neural metadata renamed","311.8.0.0",Dlss.Exports);
            string layer=SafeFs.Under(env.Dir("ofxr"),"manifest.json");
            var profileDir=SafeFs.Under(env.OfxrRoot,"Profiles");Directory.CreateDirectory(profileDir);
            string ofxr=SafeFs.Under(profileDir,Ofxr.GameExe+".ini");
            File.WriteAllText(ofxr,"nr_enabled=on");
            env.OfxrLayer=()=>layer;
            choice=null;
            try { session.Play(profileId,null); }
            catch(OptionalComponentException error){choice=error;}
            Check(choice!=null&&choice.Message=="OFXR_NEURAL_ALREADY_ACTIVE"&&choice.Retry.GraphicsFallback=="performance",
                  "known OFXR neural conflict permits SR only");
            env.OfxrLayer=()=>null;
            File.WriteAllBytes(calibration,previous);
        }

        static void VerifyRetailUpdate(Env env,PlaySession session,string profileId,DateTime exeTime,string exe){
            string path=SafeFs.Under(env.GameDir,"update/update2.rpf");
            var info=new FileInfo(path);
            var time=info.LastWriteTimeUtc;
            var archive=new Content.GameFiles(env.GameDir).Archive("update/update2.rpf");
            long end=0,offset=-1;
            foreach(var entry in archive.Files().OrderBy(item=>item.Offset)){
                if(end>0&&entry.Offset>end){offset=end;break;}
                end=Math.Max(end,entry.Offset+(entry.Size==0?entry.Uncompressed:entry.Size));
            }
            Check(offset>=0&&offset<info.Length,"one unconsumed padding byte for the compatibility fixture");
            int original;
            using(var stream=File.Open(path,FileMode.Open,FileAccess.ReadWrite)){
                stream.Position=offset;original=stream.ReadByte();stream.Position=offset;stream.WriteByte((byte)(original^1));
            }
            Expect<InvalidDataException>(()=>session.Play(profileId,null),"INSTALL_BASE_CHANGED:update/update2.rpf");
            string changed=SafeFs.Hash(path);
            Check(session.InstallEmbedded(profileId,null,CancellationToken.None)==profileId&&SafeFs.Hash(path)==changed&&
                  File.GetLastWriteTimeUtc(exe)==exeTime,"revalidation accepts an unchanged set of consumed members");
            using(var stream=File.OpenWrite(path)){stream.Position=offset;stream.WriteByte((byte)original);}
            File.SetLastWriteTimeUtc(path,time);
            session.InstallEmbedded(profileId,null,CancellationToken.None);
            string baseExe=SafeFs.Under(env.GameDir,"GTA5.exe");
            var baseTime=File.GetLastWriteTimeUtc(baseExe);
            using(var stream=File.Open(baseExe,FileMode.Open,FileAccess.ReadWrite)){
                stream.Position=stream.Length-1;original=stream.ReadByte();stream.Position=stream.Length-1;
                stream.WriteByte((byte)(original^1));
            }
            File.SetLastWriteTimeUtc(baseExe,baseTime);
            try { Expect<InvalidDataException>(()=>session.Play(profileId,null),"DELTA_BASE_MISMATCH"); }
            finally{
                using(var stream=File.OpenWrite(baseExe)){stream.Position=stream.Length-1;stream.WriteByte((byte)original);}
                File.SetLastWriteTimeUtc(baseExe,baseTime);
            }
        }

        static void RejectDamagedShaderBase(Env env,PlaySession session,GameLayer layer,Library library,string newExe,string archiveHash,
                                            string installedProfile=null) {
            string path=SafeFs.Under(env.GameDir,"update/update2.rpf");
            var files=new Content.GameFiles(env.GameDir);
            var entry=files.Archive("update/update2.rpf").Find("common/shaders/win32_nvstereo_final/adaptivedof.fxc");
            Check(entry!=null && entry.Size>0 && entry.Size<=8*1024*1024,"retail damage fixture member");
            DateTime originalWrite=File.GetLastWriteTimeUtc(path);
            var original=new byte[entry.Size];
            using(var stream=File.Open(path,FileMode.Open,FileAccess.ReadWrite)) {
                stream.Position=entry.Offset;
                Content.GtaKeys.ReadFull(stream,original);
                stream.Position=entry.Offset;
                stream.Write(new byte[original.Length],0,original.Length);
            }
            try {
                Expect<InvalidDataException>(()=>session.InstallEmbedded(installedProfile,null,CancellationToken.None),
                                             "RETAIL_SHADER_BASE_MISMATCH:common/shaders/win32_40/adaptiveDof.fxc");
                if(installedProfile==null){
                    Check(layer.State()==null && !layer.Pending && !File.Exists(newExe) &&
                          library.Version(EmbeddedRelease.VersionId)==null,"invalid shader base before activation");
                    Check(!Directory.EnumerateFileSystemEntries(env.Dir("scratch")).Any(),"invalid shader staging cleaned");
                }else Check(!layer.Pending && layer.State().version==EmbeddedRelease.VersionId && File.Exists(newExe),
                            "invalid update preserves installed version without activation");
            } finally {
                using(var stream=File.OpenWrite(path)) {
                    stream.Position=entry.Offset;
                    stream.Write(original,0,original.Length);
                }
                File.SetLastWriteTimeUtc(path,originalWrite);
            }
            Check(SafeFs.Hash(path)==archiveHash,"retail damage fixture restored");
        }
    }
}
