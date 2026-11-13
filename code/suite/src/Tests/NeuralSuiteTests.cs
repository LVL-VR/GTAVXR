using System;
using System.IO;
using System.Linq;
using System.Text;
using System.Collections.Generic;

namespace GtavxrSuite {
    // 2.4.0 (167-neural A3, B3-B5): the player's PC decides the OpenXR runtime, the DLSS files and OFXR; a suite-mode version
    // needs no repository. Headless, owned fixture folders only.
    public static class NeuralSuiteTests {
        public static void Add(Action<string,Action> t,string fresh){
            t("DLSS detection by content: sample folders, search order, invalid files reported, hash never compared",()=>DlssSamples(Path.Combine(fresh,"dlss")));
            t("graphics presets: Native / Performance / Neural / Custom read and written in [Graphics], other lines kept",GraphicsPresets);
            t("OpenXR runtime selector: Automatic, a registered runtime or a manifest file; manifest and library_path checked",()=>RuntimeSelector(Path.Combine(fresh,"xr")));
            t("OFXR: status only for a GTAVXR game, recommended profile changes only an existing key and keeps a copy",()=>OfxrProfile(Path.Combine(fresh,"ofxr")));
            t("another PC: a suite-mode version plays with no repository, its own loader, the session VR file, the DLSS folder and a changed runtime logged",()=>OtherPc(Path.Combine(fresh,"otherpc")));
        }
        static void Check(bool c,string what){if(!c)throw new Exception("expected: "+what);}

        // ---------- DLSS ----------
        static void DlssSamples(string root){
            Directory.CreateDirectory(root);
            Func<string,string> dir=n=>{string d=Path.Combine(root,n);Directory.CreateDirectory(d);return d;};
            string good=dir("good"),srOnly=dir("sr-only"),notNv=dir("not-nvidia"),noExp=dir("no-exports"),notPe=dir("not-pe"),empty=dir("empty");
            string nested=Path.Combine(root,"search","Games","SomeGame","bin");Directory.CreateDirectory(nested);
            FakeDll(Path.Combine(good,Dlss.SrModel),"NVIDIA Corporation","NVIDIA DLSS","310.8.0.0",Dlss.Exports);
            FakeDll(Path.Combine(good,Dlss.NrModel),"NVIDIA Corporation","NVIDIA DLSS NR","310.8.0.0",Dlss.Exports);
            FakeDll(Path.Combine(srOnly,Dlss.SrModel),"NVIDIA Corporation","NVIDIA DLSS","3.7.20.0",Dlss.Exports);
            FakeDll(Path.Combine(notNv,Dlss.SrModel),"Other Corp","Upscaler","1.0.0.0",Dlss.Exports);
            FakeDll(Path.Combine(noExp,Dlss.SrModel),"NVIDIA Corporation","NVIDIA DLSS","310.8.0.0",Dlss.Exports.Take(2));
            File.WriteAllText(Path.Combine(notPe,Dlss.SrModel),new string('x',8192));
            FakeDll(Path.Combine(nested,Dlss.SrModel),"NVIDIA Corporation","NVIDIA DLSS","310.2.1.0",Dlss.Exports);
            FakeDll(Path.Combine(nested,Dlss.NrModel),"NVIDIA Corporation","NVIDIA DLSS NR","310.2.1.0",Dlss.Exports);
            var g=Dlss.InspectFolder(good,"chosen");
            Check(Dlss.NeuralUsable(g)&&g.sr.version=="310.8.0.0"&&g.sr.sha256!=null,"good folder: SR+NR valid, version read, hash recorded; got "+(g.sr.error??g.sr.version));
            Check(Dlss.Inspect(Path.Combine(notNv,Dlss.SrModel)).error=="DLSS_NOT_NVIDIA","another vendor's DLL refused");
            Check(Dlss.Inspect(Path.Combine(noExp,Dlss.SrModel)).error=="DLSS_NO_NGX_EXPORTS","a DLL without the four NGX exports refused");
            Check(Dlss.Inspect(Path.Combine(notPe,Dlss.SrModel)).error=="DLSS_NOT_PE","a non-PE file refused");
            var s=Dlss.InspectFolder(srOnly,"suite");Check(Dlss.Usable(s)&&!Dlss.NeuralUsable(s)&&s.nr==null,"SR-only folder usable, neural off");
            Check(Dlss.InspectFolder(empty,"chosen")==null,"a folder without nvngx_dlss.dll is not a DLSS folder");
            var roots=new List<string>{Path.Combine(root,"search")};
            var f=Dlss.Find(good,srOnly,roots);Check(f.source=="chosen"&&f.dir==Path.GetFullPath(good),"the chosen folder first");
            f=Dlss.Find(null,srOnly,roots);Check(f.source=="suite"&&f.dir==Path.GetFullPath(srOnly),"then the Suite dlss folder");
            f=Dlss.Find(null,empty,roots);Check(f!=null&&f.source=="found"&&f.dir==Path.GetFullPath(nested)&&Dlss.NeuralUsable(f),"then a bounded search of the player's folders (3 levels)");
            f=Dlss.Find(notNv,srOnly,roots);Check(f.source=="suite","a chosen folder that stopped working gives way to a usable one");
            f=Dlss.Find(noExp,empty,new List<string>());Check(f!=null&&f.source=="chosen"&&f.sr.error=="DLSS_NO_NGX_EXPORTS","with nothing usable the chosen folder is reported with its reason");
            Check(Dlss.Find(null,empty,new List<string>())==null,"nothing anywhere = no folder");
            // different bytes, same contract: both accepted (never compared to a hash)
            Check(Dlss.Inspect(Path.Combine(nested,Dlss.SrModel)).error==null&&Dlss.Inspect(Path.Combine(good,Dlss.SrModel)).sha256!=Dlss.Inspect(Path.Combine(nested,Dlss.SrModel)).sha256,"two different DLSS builds both valid");
        }

        // ---------- graphics ----------
        static void GraphicsPresets(){
            var n=Graphics.Read("");Check(n.preset=="native"&&n.upscale=="off"&&!n.neural&&n.intensity==Graphics.DefaultIntensity,"no [Graphics] = Native");
            string ini="[VR]\r\nprofile=A\r\n";
            var neural=Graphics.For("neural",n);Check(neural.upscale==Graphics.DefaultLevel&&neural.neural,"Neural = SR quality + NR");
            string w=Graphics.Write(ini,neural);
            Check(w.StartsWith("[VR]\r\nprofile=A\r\n",StringComparison.Ordinal)&&w.Contains("[Graphics]\r\npreset=neural\r\nupscale=quality\r\nneural=1\r\nneural_intensity=50\r\n"),"keys written in their section, other lines and CRLF kept; got "+w.Replace("\r\n","|"));
            var r=Graphics.Read(w);Check(r.preset=="neural"&&r.upscale=="quality"&&r.neural,"read back as Neural");
            var perf=Graphics.For("performance",Graphics.Read(Graphics.Write(w,new GraphicsChoice{preset="neural",upscale="ultra",neural=true,intensity=80})));
            Check(perf.upscale=="ultra"&&!perf.neural&&perf.intensity==80,"Performance keeps the chosen level and intensity");
            // the VR menu turned neural off during play: the stored preset no longer matches and reads as Custom
            var changed=Graphics.Read(Ini.Set(w,"Graphics","neural","0"));Check(changed.preset=="custom"&&changed.upscale=="quality"&&!changed.neural,"a stored preset changed in game reads as Custom");
            Check(Graphics.Read(Ini.Set(w,"Graphics","upscale","bogus")).upscale=="off","an unknown level reads as off (like the EXE)");
            Check(Graphics.Read(Ini.Set(w,"Graphics","neural_intensity","300")).intensity==100,"intensity clamped to 0..100");
            bool threw=false;try{Graphics.Write(ini,new GraphicsChoice{preset="neural",upscale="max",neural=true,intensity=50});}catch(ArgumentException){threw=true;}
            Check(threw,"an unknown level is never written");
            Check(!Graphics.For("native",neural).NeedsDlss()&&Graphics.For("custom",neural).NeedsDlss(),"Native needs no DLSS; Custom keeps the current keys");
        }

        // ---------- OpenXR runtime ----------
        static void RuntimeSelector(string root){
            Directory.CreateDirectory(root);
            string vdDir=Path.Combine(root,"Virtual Desktop Streamer","OpenXR");Directory.CreateDirectory(vdDir);
            File.WriteAllText(Path.Combine(vdDir,"virtualdesktop-openxr.dll"),"runtime");
            string vd=Path.Combine(vdDir,"virtualdesktop-openxr.json");
            File.WriteAllText(vd,"{\"file_format_version\":\"1.0.0\",\"runtime\":{\"name\":\"Virtual Desktop\",\"library_path\":\"virtualdesktop-openxr.dll\"}}");
            string steamDir=Path.Combine(root,"SteamVR");Directory.CreateDirectory(steamDir);string steamLib=Path.Combine(steamDir,"vrclient_x64.dll");File.WriteAllText(steamLib,"runtime");
            string steam=Path.Combine(steamDir,"steamxr_win64.json");
            File.WriteAllText(steam,"{\"file_format_version\":\"1.0.0\",\"runtime\":{\"name\":\"SteamVR\",\"library_path\":"+Json.Encode(steamLib)+"}}");
            string other=Path.Combine(root,"other.json");string otherLib=Path.Combine(root,"other_runtime.dll");File.WriteAllText(otherLib,"runtime");
            File.WriteAllText(other,"{\"file_format_version\":\"1.0.0\",\"runtime\":{\"name\":\"Acme XR\",\"library_path\":\"other_runtime.dll\"}}");
            string broken=Path.Combine(root,"broken.json");File.WriteAllText(broken,"{\"file_format_version\":\"1.0.0\",\"runtime\":{\"library_path\":\"gone.dll\"}}");
            string bad=Path.Combine(root,"bad.json");File.WriteAllText(bad,"{\"file_format_version\":\"1.0.0\"}");
            var a=XrRuntimes.Resolve(XrRuntimes.Auto,vd);
            Check(a.error==null&&a.active&&a.name=="Virtual Desktop"&&a.library==Path.GetFullPath(Path.Combine(vdDir,"virtualdesktop-openxr.dll")),"Automatic = the active runtime, relative library_path resolved");
            var s=XrRuntimes.Resolve(steam,vd);Check(s.error==null&&!s.active&&s.name=="SteamVR"&&s.library==steamLib,"a pinned runtime is used even when another one is active");
            Check(XrRuntimes.Resolve(XrRuntimes.Auto,null).error=="XR_NO_ACTIVE_RUNTIME","Automatic with no active runtime");
            Check(XrRuntimes.Resolve(Path.Combine(root,"missing.json"),vd).error=="XR_MANIFEST_MISSING","a pinned manifest that is gone");
            Check(XrRuntimes.Inspect(broken,vd).error=="XR_LIBRARY_MISSING","a manifest whose library is missing");
            Check(XrRuntimes.Inspect(bad,vd).error=="XR_MANIFEST_BAD","a manifest without runtime.library_path");
            var list=XrRuntimes.List(vd,new List<string>{steam,vd,broken,other});
            Check(list.Count==4&&list[0].manifest==vd&&list[0].active&&list.Count(r=>r.active)==1,"the active runtime first, each runtime once");
            Check(list.Single(r=>r.manifest==broken).error=="XR_LIBRARY_MISSING","an unusable registered runtime is listed with its reason");
            Check(list.Single(r=>r.manifest==other).name=="Acme XR"&&XrRuntimes.StatusProcesses("Acme XR")==null,"another runtime keeps its own name and gets a neutral chip");
            Check(XrRuntimes.StatusProcesses("Virtual Desktop")[0]=="VirtualDesktop.Streamer"&&XrRuntimes.StatusProcesses("SteamVR")[0]=="vrserver"&&XrRuntimes.StatusProcesses("Meta Quest Link")[0]=="OVRServer_x64","status process per runtime");
            Check(XrRuntimes.Name(@"C:\Program Files\Oculus\Support\oculus-runtime\oculus_openxr_64.json",null)=="Meta Quest Link","Meta runtime named");
        }

        // ---------- OFXR ----------
        static void OfxrProfile(string root){
            string profiles=Path.Combine(root,"Profiles");Directory.CreateDirectory(profiles);
            string layer=Path.Combine(root,"layer.json");File.WriteAllText(layer,"{}");
            var none=Ofxr.Read(root,null,null);Check(!none.layer&&none.status==null&&none.profile==null,"no layer = not installed");
            var st=Ofxr.Read(root,layer,null);Check(st.layer&&st.status=="no-data"&&st.profile==null,"layer, no profile yet, no GTAVXR data");
            Check(Ofxr.Recommend(st.profile)=="OFXR_NO_PROFILE","no profile = nothing changed");
            string js=Path.Combine(root,"status.json");
            File.WriteAllText(js,"{\"session\":{\"process\":\"Cyberpunk2077.exe\"},\"generating\":true}");
            Check(Ofxr.Read(root,layer,null).status=="no-data","status of another game is not shown");
            File.WriteAllText(js,"{\"session\":{\"process\":\"GTAVXR_SourceBaseline.exe\"},\"armed\":true,\"generating\":true,\"app_fps\":45.5,\"output_fps\":90}");
            var g=Ofxr.Read(root,layer,null);Check(g.status=="generating"&&Math.Abs(g.app_fps-45.5)<0.01&&Math.Abs(g.output_fps-90)<0.01,"status of the GTAVXR game shown");
            File.SetLastWriteTimeUtc(js,DateTime.UtcNow.AddMinutes(-5));Check(Ofxr.Read(root,layer,null).status=="no-data","an old status file is ignored");
            string prof=Path.Combine(profiles,"GTAVXR_SourceBaseline.ini");
            File.WriteAllText(prof,"[General]\r\nnr_enabled = True\r\nframegen = 1\r\n");
            Check(Ofxr.Read(root,layer,null).profile==prof,"the GTAVXR profile OFXR created is found");
            Check(Ofxr.Recommend(prof)==null,"recommended profile written");
            Check(File.ReadAllText(prof)=="[General]\r\nnr_enabled = False\r\nframegen = 1\r\n","only nr_enabled changed, in its own form; got "+File.ReadAllText(prof).Replace("\r\n","|"));
            Check(File.ReadAllText(prof+".gtavxr-before")=="[General]\r\nnr_enabled = True\r\nframegen = 1\r\n","the previous profile kept beside it");
            string noKey=Path.Combine(root,"nokey.ini");File.WriteAllText(noKey,"[General]\nframegen=1\n");
            Check(Ofxr.Recommend(noKey)=="OFXR_NO_KEY"&&File.ReadAllText(noKey)=="[General]\nframegen=1\n"&&!File.Exists(noKey+".gtavxr-before"),"a profile without the key is not touched");
            string odd=Path.Combine(root,"odd.ini");File.WriteAllText(odd,"nr_enabled=maybe\n");
            Check(Ofxr.Recommend(odd)=="OFXR_UNKNOWN_FORMAT"&&File.ReadAllText(odd)=="nr_enabled=maybe\n","an unknown value is not touched");
        }

        // ---------- another PC ----------
        static void OtherPc(string fresh){
            var env=Env.ForFixture(fresh);var lib=new Library(env);var blobs=new Blobs(env);
            env.GameProcessNames=new[]{"GTAVXR_FixtureGame"};
            Directory.CreateDirectory(Path.Combine(env.GameDir,"update"));
            var retail=new Dictionary<string,string>();
            foreach(string n in new[]{"GTA5.exe","update/update.rpf","update/update2.rpf"}){string p=SafeFs.Under(env.GameDir,n);File.WriteAllText(p,"FIXTURE "+n);retail[n]=SafeFs.Hash(p);}
            env.RetailOverride=retail;
            string user=env.UserDataDir;Directory.CreateDirectory(user);
            File.WriteAllText(Path.Combine(user,"settings.xml"),EngineTests.SettingsXml);
            // this PC's runtime: a manifest and its library (the fixture's active runtime)
            string xrDir=Path.GetDirectoryName(env.OpenXrManifest);string xrLib=Path.Combine(xrDir,"fixture_runtime.dll");File.WriteAllText(xrLib,"runtime v1");
            File.WriteAllText(env.OpenXrManifest,"{\"file_format_version\":\"1.0.0\",\"runtime\":{\"name\":\"Fixture XR\",\"library_path\":\"fixture_runtime.dll\"}}");
            // another PC: no GTAVXR repository at all
            Directory.Delete(env.DevRepo,true);
            string src=Path.Combine(fresh,"src");Directory.CreateDirectory(src);
            Func<string,string,FileRef> put=(rel,text)=>{string p=Path.Combine(src,Guid.NewGuid().ToString("N"));File.WriteAllText(p,text);return blobs.PutFile(p,rel);};
            var exe=blobs.PutFile(Program.SelfPath,"GTAVXR_FixtureGame.exe");
            var runtime=new List<FileRef>{put("gtavxr_native_openxr.dll","bridge"),put("gtavxr_trainer.dll","trainer"),put("openxr_loader.dll","loader"),put("nvngx.dll_gtavxr.dll","forwarder"),put("nvngx.dll_gtavxr_sr.dll","forwarder")};
            Func<string,string,VersionManifest> mk=(id,mode)=>new VersionManifest{id=id,build=id.Substring(0,3),title="Fixture "+id,channel="stable",added_utc=DateTime.UtcNow.ToString("o"),notes=new List<string>(),
                retail=retail,files=new List<FileRef>{exe,put("common/data/one.txt",id)},runtime=runtime,mvga="none",
                launch=new LaunchContract{exe="GTAVXR_FixtureGame.exe",args=new List<string>{"--fixture-game","{LOGFILE}","-gtavxrNativeXR"},disable_layers=new List<string>(),session_mode=mode,settings_mode=mode,display="windowed1920x1080_stereo0",residual="exact-one-thread"}};
            var neural=mk("167-neural","suite");
            neural.neural=new NeuralContract{dlss_dir_env="GTAVXR_DLSS_DIR",models=new List<string>{Dlss.SrModel,Dlss.NrModel},settings_section="Graphics",keys=new List<string>{"upscale","neural","neural_intensity"},
                paths_env=new List<string>{"GTAVXR_LOADER_PATH","GTAVXR_VR_SETTINGS","GTAVXR_SESSIONS_ROOT"}};
            var old=mk("125-dual","legacy-repo");
            lib.SaveVersion(neural);lib.SaveVersion(old);
            var imp=new Importers(env);
            string ua=Path.Combine(fresh,"ua");Directory.CreateDirectory(Path.Combine(ua,"Default"));File.WriteAllText(Path.Combine(ua,"settings.xml"),EngineTests.SettingsXml);
            File.WriteAllBytes(Path.Combine(ua,"Default","SGTA50015"),EngineTests.FakeSave("(Autoguardado) Neural (30.0%) - n",8192));
            string ia=Path.Combine(fresh,"n.ini");File.WriteAllText(ia,"[VR]\r\nprofile=N\r\n");
            var np=imp.CreateFrom("Neural","167-neural",ua,UserTree.Snapshot(ua),ia,"test");
            var q=imp.CreateFrom("Old","125-dual",ua,UserTree.Snapshot(ua),null,"test");
            string vr=lib.VrSettingsFile(np.id);
            File.WriteAllText(vr,Graphics.Write(File.ReadAllText(vr),Graphics.For("neural",null)),new UTF8Encoding(false));
            // the player's DLSS files in the Suite dlss folder
            string dlss=env.Dir("dlss");
            FakeDll(Path.Combine(dlss,Dlss.SrModel),"NVIDIA Corporation","NVIDIA DLSS","310.8.0.0",Dlss.Exports);
            FakeDll(Path.Combine(dlss,Dlss.NrModel),"NVIDIA Corporation","NVIDIA DLSS NR","310.8.0.0",Dlss.Exports);
            // a developer's variables in the Suite's own environment never reach the game
            string gxBefore=Environment.GetEnvironmentVariable("GX_DEV_ROOT"),dlBefore=Environment.GetEnvironmentVariable("GTAVXR_DLSS_DIR");
            Environment.SetEnvironmentVariable("GX_DEV_ROOT",Path.Combine(fresh,"developer"));Environment.SetEnvironmentVariable("GTAVXR_DLSS_DIR",Path.Combine(fresh,"developer-dlss"));
            try{
                var ps=new PlaySession(env);
                bool refused=false;try{ps.Play(q.id,null);}catch(InvalidDataException e){refused=e.Message=="LEGACY_REPO_ONLY";}
                Check(refused&&ps.Unfinished().Count==0,"a 125-167 version is refused on another PC before anything changes");
                var r=ps.Play(np.id,null);
                Check(r.phase=="CLOSED"&&r.result=="played"&&r.exit_code==0,"suite-mode version played, got "+r.phase+"/"+r.result+"/"+r.error);
                var rep=Json.Read<Dictionary<string,object>>(Path.Combine(r.native_session,"fixture-report.json"));
                string runtimeDir=Path.Combine(env.Dir("runtime"),"167-neural");
                Check(String.Equals((string)rep["sessions_root"],env.Dir("sessions"),StringComparison.OrdinalIgnoreCase)&&((string)rep["session_dir"]).StartsWith(env.Dir("sessions"),StringComparison.OrdinalIgnoreCase),"sessions in the Suite folder");
                Check(String.Equals((string)rep["vr_settings"],r.vr_settings,StringComparison.OrdinalIgnoreCase)&&r.vr_settings.StartsWith(env.DataRoot,StringComparison.OrdinalIgnoreCase),"the session copy of the profile's VR file");
                Check(((string)rep["vr_seen"]).Contains("profile=N")&&((string)rep["vr_seen"]).Contains("preset=neural"),"the game read the profile's VR and graphics settings");
                Check(String.Equals((string)rep["loader"],Path.Combine(runtimeDir,"openxr_loader.dll"),StringComparison.OrdinalIgnoreCase)&&(string)rep["loader_seen"]=="loader","the version's own OpenXR loader");
                Check(String.Equals((string)rep["xr_json"],env.OpenXrManifest,StringComparison.OrdinalIgnoreCase),"the runtime of this PC");
                Check(String.Equals((string)rep["dlss_dir"],Path.GetFullPath(dlss),StringComparison.OrdinalIgnoreCase)&&r.dlss_note==null,"the DLSS folder given to the game");
                Check(String.IsNullOrEmpty((string)rep["gx_dev_root"]),"no developer folder reaches the game");
                Check(File.ReadAllText(lib.VrSettingsFile(np.id)).Contains("changed=1"),"VR settings changed in game kept in the profile");
                // (the fixture game itself builds its fixture Env, which makes empty folders there; no file may appear)
                Check(!Directory.Exists(env.DevRepo)||Directory.GetFiles(env.DevRepo,"*",SearchOption.AllDirectories).Length==0,"nothing written where a repository would be");
                Func<SessionRecord,string,string> sha=(rec,kind)=>{var c=rec.components.FirstOrDefault(x=>x.kind==kind);return c==null?null:c.sha256;};
                Check(sha(r,"openxr_manifest")!=null&&sha(r,"openxr_runtime")!=null&&sha(r,"dlss_sr")!=null&&sha(r,"dlss_nr")!=null,"runtime and DLSS files recorded");
                // the runtime updated itself (Virtual Desktop does): logged, never refused
                File.WriteAllText(xrLib,"runtime v2 after an update");
                var r2=ps.Play(np.id,null);
                Check(r2.phase=="CLOSED"&&r2.result=="played","a changed runtime still plays");
                Check(sha(r2,"openxr_runtime")!=sha(r,"openxr_runtime"),"the new runtime bytes are recorded");
                // Native preset: no DLSS variable at all
                vr=lib.VrSettingsFile(np.id);File.WriteAllText(vr,Graphics.Write(File.ReadAllText(vr),Graphics.For("native",null)),new UTF8Encoding(false));
                var r3=ps.Play(np.id,null);rep=Json.Read<Dictionary<string,object>>(Path.Combine(r3.native_session,"fixture-report.json"));
                Check(r3.dlss_dir==null&&String.IsNullOrEmpty((string)rep["dlss_dir"]),"Native: the chain stays off (no DLSS folder given)");
                // Neural with an SR-only folder the player chose: SR runs, the reason is kept
                string srOnly=Path.Combine(fresh,"sr-only");Directory.CreateDirectory(srOnly);FakeDll(Path.Combine(srOnly,Dlss.SrModel),"NVIDIA Corporation","NVIDIA DLSS","3.7.20.0",Dlss.Exports);
                var st=env.LoadSettings();st.dlss_dir=srOnly;env.SaveSettings(st);
                File.WriteAllText(vr,Graphics.Write(File.ReadAllText(vr),Graphics.For("neural",null)),new UTF8Encoding(false));
                var r4=ps.Play(np.id,null);
                Check(String.Equals(r4.dlss_dir,Path.GetFullPath(srOnly),StringComparison.OrdinalIgnoreCase)&&r4.dlss_note=="no_neural_model","SR-only folder: SR on, neural reported missing");
                // a pinned runtime that is gone: refused before anything changes
                st=env.LoadSettings();st.openxr_runtime=Path.Combine(fresh,"gone","runtime.json");env.SaveSettings(st);
                string why=null;try{ps.Play(np.id,null);}catch(InvalidDataException e){why=e.Message;}
                Check(why=="XR_RUNTIME:XR_MANIFEST_MISSING"&&ps.Unfinished().Count==0,"a missing pinned runtime is refused up front, got "+why);
                st=env.LoadSettings();st.openxr_runtime=XrRuntimes.Auto;env.SaveSettings(st);
            }finally{
                Environment.SetEnvironmentVariable("GX_DEV_ROOT",gxBefore);Environment.SetEnvironmentVariable("GTAVXR_DLSS_DIR",dlBefore);
            }
        }

        // ---------- a minimal 64-bit DLL image (no NVIDIA bytes): one read-only section, an export table and a version resource ----------
        public static void FakeDll(string path,string company,string product,string version,IEnumerable<string> exports){
            const int fileAlign=0x200,secRva=0x1000,secRaw=0x200,secSize=0xE00;
            var names=exports.OrderBy(x=>x,StringComparer.Ordinal).ToList();int n=names.Count;
            var s=new byte[secSize];
            int funcs=40,nameTab=funcs+4*n,ords=nameTab+4*n,p=ords+2*n;
            Func<string,int> put=txt=>{int at=p;var b=Encoding.ASCII.GetBytes(txt);Array.Copy(b,0,s,p,b.Length);p+=b.Length+1;return at;};
            int dll=put(Path.GetFileName(path));
            var nameAt=names.Select(put).ToList();
            int code=(p+15)&~15;s[code]=0xC3;int expSize=code;     // the "functions" sit after the export block (not forwarders)
            W32(s,12,secRva+dll);W32(s,16,1);W32(s,20,n);W32(s,24,n);W32(s,28,secRva+funcs);W32(s,32,secRva+nameTab);W32(s,36,secRva+ords);
            for(int i=0;i<n;i++){W32(s,funcs+4*i,secRva+code);W32(s,nameTab+4*i,secRva+nameAt[i]);W16(s,ords+2*i,i);}
            int res=(code+16+15)&~15;
            var vi=VersionInfo(company,product,version);
            int d2=res+24,d3=res+48,de=res+72,data=res+88;
            ResDir(s,res,16,0x80000000u|(uint)(d2-res));ResDir(s,d2,1,0x80000000u|(uint)(d3-res));ResDir(s,d3,0x409,(uint)(de-res));
            W32(s,de,secRva+data);W32(s,de+4,vi.Length);
            if(data+vi.Length>secSize)throw new InvalidDataException("fixture DLL too large");
            Array.Copy(vi,0,s,data,vi.Length);
            int resSize=data+vi.Length-res;
            var f=new byte[secRaw+secSize];
            f[0]=(byte)'M';f[1]=(byte)'Z';W32(f,0x3c,0x40);
            int pe=0x40;f[pe]=(byte)'P';f[pe+1]=(byte)'E';
            W16(f,pe+4,0x8664);W16(f,pe+6,1);W16(f,pe+20,0xF0);W16(f,pe+22,0x2022);
            int o=pe+24;
            W16(f,o,0x20b);f[o+2]=14;W32(f,o+8,secSize);W32(f,o+20,secRva);
            W64(f,o+24,0x180000000L);W32(f,o+32,0x1000);W32(f,o+36,fileAlign);
            W16(f,o+40,6);W16(f,o+48,6);
            W32(f,o+56,secRva+0x1000);W32(f,o+60,secRaw);W16(f,o+68,2);W16(f,o+70,0x160);
            W64(f,o+72,0x100000);W64(f,o+80,0x1000);W64(f,o+88,0x100000);W64(f,o+96,0x1000);
            W32(f,o+108,16);
            W32(f,o+112,secRva);W32(f,o+116,expSize);
            W32(f,o+128,secRva+res);W32(f,o+132,resSize);
            int sh=o+0xF0;var nm=Encoding.ASCII.GetBytes(".rdata");Array.Copy(nm,0,f,sh,nm.Length);
            W32(f,sh+8,secSize);W32(f,sh+12,secRva);W32(f,sh+16,secSize);W32(f,sh+20,secRaw);W32(f,sh+36,unchecked((int)0x40000040));
            Array.Copy(s,0,f,secRaw,secSize);
            File.WriteAllBytes(path,f);
        }
        static byte[] VersionInfo(string company,string product,string version){
            var v=version.Split('.').Select(x=>Int32.Parse(x)).Concat(new[]{0,0,0,0}).Take(4).ToArray();
            var ffi=new byte[52];
            W32(ffi,0,unchecked((int)0xFEEF04BD));W32(ffi,4,0x10000);W32(ffi,8,(v[0]<<16)|v[1]);W32(ffi,12,(v[2]<<16)|v[3]);W32(ffi,16,(v[0]<<16)|v[1]);W32(ffi,20,(v[2]<<16)|v[3]);
            W32(ffi,24,0x3F);W32(ffi,32,0x40004);W32(ffi,36,2);
            Func<string,string,byte[]> str=(k,val)=>Block(k,Encoding.Unicode.GetBytes(val+"\0"),val.Length+1,1);
            var table=Block("040904b0",null,0,1,str("CompanyName",company),str("FileDescription",product),str("FileVersion",version),str("ProductName",product),str("ProductVersion",version));
            var sfi=Block("StringFileInfo",null,0,1,table);
            var vfi=Block("VarFileInfo",null,0,1,Block("Translation",new byte[]{0x09,0x04,0xB0,0x04},4,0));
            return Block("VS_VERSION_INFO",ffi,52,0,sfi,vfi);
        }
        static byte[] Block(string key,byte[] value,int valueLength,int type,params byte[][] children){
            var b=new List<byte>{0,0};b.AddRange(BitConverter.GetBytes((ushort)valueLength));b.AddRange(BitConverter.GetBytes((ushort)type));
            b.AddRange(Encoding.Unicode.GetBytes(key+"\0"));
            while(b.Count%4!=0)b.Add(0);
            if(value!=null)b.AddRange(value);
            foreach(var c in children){while(b.Count%4!=0)b.Add(0);b.AddRange(c);}
            var len=BitConverter.GetBytes((ushort)b.Count);b[0]=len[0];b[1]=len[1];
            return b.ToArray();
        }
        static void ResDir(byte[] s,int at,int id,uint target){W16(s,at+14,1);W32(s,at+16,id);W32(s,at+20,unchecked((int)target));}
        static void W16(byte[] b,int o,int v){b[o]=(byte)v;b[o+1]=(byte)(v>>8);}
        static void W32(byte[] b,int o,int v){for(int i=0;i<4;i++)b[o+i]=(byte)(v>>(8*i));}
        static void W64(byte[] b,int o,long v){for(int i=0;i<8;i++)b[o+i]=(byte)(v>>(8*i));}
    }
}
