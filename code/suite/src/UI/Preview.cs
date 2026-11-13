using System;
using System.IO;
using System.Linq;
using System.Text;
using System.Windows;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Collections.Generic;

namespace GtavxrSuite {
    // Offscreen pictures of every screen (no window is ever shown) over a demo library with example data.
    public static class Preview {
        public static readonly string[] Views={"play","play-version","play-test","play-testprofile","running","newprofile","newprofile-test","try-mod","flight-result","recovery","mods","settings","firstrun","play-content","play-personalize-wov","play-personalize-plus","play-graphics"};
        internal static void EnsureApp(){
            if(Application.Current==null){var app=new Application{ShutdownMode=ShutdownMode.OnExplicitShutdown};app.Resources.MergedDictionaries.Add(Theme.Resources());}
        }
        public static object RenderAll(string folder){
            folder=SafeFs.Root(folder);if(Directory.Exists(folder))throw new InvalidDataException("Fresh folder required");Directory.CreateDirectory(folder);
            var rows=new List<object>();
            foreach(string lang in new[]{"es","en"})foreach(string v in Views){
                string png=Path.Combine(folder,v+"-"+lang+".png");Render(v,lang,png,Path.Combine(folder,"demo-"+v+"-"+lang));
                rows.Add(new{view=v,locale=lang,png=Path.GetFileName(png),sha256=SafeFs.Hash(png)});
            }
            return new{status="PASS_OFFSCREEN_RENDER_NO_WINDOW",count=rows.Count,rows=rows,window_shown=false,gta_launched=false};
        }
        public static object Render(string view,string lang,string png){return Render(view,lang,png,Path.Combine(Path.GetDirectoryName(png),"demo-"+Ids.New()));}
        static object Render(string view,string lang,string png,string demoRoot){
            EnsureApp();
            var env=Demo.Build(demoRoot,lang,view);
            var st=new AppState(env){Demo=true,StreamerRunning=true,GameFound=true,RuntimeName="Virtual Desktop"};
            st.Recovery=view=="recovery";
            st.Reload();
            var shell=new Shell(st);
            bool onPlay=view.StartsWith("play")||view=="running"||view.StartsWith("newprofile")||view=="recovery"||view=="flight-result";
            if(onPlay){
                shell.Section="play";
                PlayView.EditorOpen=true;PlayView.EditorTab=view=="play-version"?"version":view=="play-graphics"?"vr":view=="play-testprofile"||view.StartsWith("play-gate-")?"profile":"mods";
                if(view=="play-test"||view=="play-testprofile"||view=="flight-result"){var t=st.Profiles.FirstOrDefault(p=>p.test);if(t!=null)st.Settings.selected_profile=t.id;}
                PlayView.PersonalizeMod=view=="play-personalize-wov"?"demo-wov":view=="play-personalize-plus"?"demo-wovplus":null;
            }else shell.Section=view=="try-mod"?"mods":view;
            if(view=="firstrun")st.Settings.first_run_done=false;
            shell.Refresh();
            if(view=="newprofile")shell.NewProfile();
            if(view=="newprofile-test")shell.NewProfile(true,null);
            if(view=="try-mod")shell.TryMod(st.Mods.First(m=>m.kind=="mvga-native"));
            if(view=="flight-result")shell.FlightResult(FlightSample(st));
            if(view=="running")shell.ShowOverlay(PlayingSample(st),false);
            var root=shell.Root;double w=1280,h=view.StartsWith("play-gate-")?1240:view.StartsWith("play-personalize-")||view=="play-content"?1500:view=="play-graphics"||view=="settings"?1100:860;   // tall enough to show the picture picker / the options
            root.Measure(new Size(w,h));root.Arrange(new Rect(0,0,w,h));root.UpdateLayout();
            var bmp=new RenderTargetBitmap((int)w,(int)h,96,96,PixelFormats.Pbgra32);bmp.Render(root);
            var enc=new PngBitmapEncoder();enc.Frames.Add(BitmapFrame.Create(bmp));
            using(var f=new FileStream(png,FileMode.CreateNew,FileAccess.Write))enc.Save(f);
            return new{status="PASS",view=view,locale=lang,png=png,window_shown=false};
        }
        // A picture of a real library (a COPY of its data folder): the selected profile's Mods tab, or a mod's Personalizar page.
        public static object RenderLibrary(string dataCopy,string lang,string mod,string png){
            EnsureApp();
            var env=Env.Product(dataCopy);var s=env.LoadSettings();s.language=lang;env.SaveSettings(s);
            var st=new AppState(env){Demo=true,StreamerRunning=true,GameFound=true,RuntimeName="Virtual Desktop"};st.Reload();
            var shell=new Shell(st){Section="play"};PlayView.EditorOpen=true;PlayView.EditorTab="mods";PlayView.PersonalizeMod=mod==""?null:mod;
            shell.Refresh();
            var root=shell.Root;double w=1280,h=1500;
            root.Measure(new Size(w,h));root.Arrange(new Rect(0,0,w,h));root.UpdateLayout();
            var bmp=new RenderTargetBitmap((int)w,(int)h,96,96,PixelFormats.Pbgra32);bmp.Render(root);
            var enc=new PngBitmapEncoder();enc.Frames.Add(BitmapFrame.Create(bmp));
            using(var f=new FileStream(png,FileMode.CreateNew,FileAccess.Write))enc.Save(f);
            return new{status="PASS",png=png,window_shown=false};
        }
        static UIElement PlayingSample(AppState st){
            var p=st.Profiles.First(x=>x.test);var v=st.Version(p.version);
            var panel=new PlayingPanel(p,v){OnAbort=()=>{},OnClose=()=>true};panel.Playing();panel.Clock(new TimeSpan(0,23,41));return panel.Root;
        }
        static SessionRecord FlightSample(AppState st){
            var p=st.Profiles.First(x=>x.test);string id="20261019T120000-demo";
            st.Lib.KeepCopy(p,"history/"+id,id);
            return new SessionRecord{id=id,profile=p.id,test=true,phase="CLOSED",result="played",prev_version="125-dual",prev_mods=new List<string>(),
                quarantined=new List<string>{"GTA5_crash.log","stray/cache.bin"},changed_game_files=new List<string>(),missing_game_files=new List<string>{"x64e.rpf"},retail_check="ok"};
        }
    }

    // Example library for previews: the real version names, example saves and mods.
    public static class Demo {
        public static Env Build(string root,string lang,string view){
            var env=Env.ForFixture(root);var lib=new Library(env);var blobs=new Blobs(env);
            string src=Path.Combine(root,"src");Directory.CreateDirectory(src);
            Func<string,string,FileRef> put=(rel,text)=>{string p=Path.Combine(src,Ids.New());File.WriteAllText(p,text);return blobs.PutFile(p,rel);};
            var retail=LegacyGamePolicy.Legacy3889();
            var stable=new VersionManifest{id="125-dual",build="125",title="Dual",channel="stable",notes=new List<string>{lang=="en"?"The approved version.":"La versión aprobada."},retail=retail,
                files=new List<FileRef>{put("GTAVXR_SourceBaseline.exe","x")},runtime=new List<FileRef>(),mvga="none",launch=new LaunchContract{args=new List<string>()}};
            var test=new VersionManifest{id="155v2",build="155",title=lang=="en"?"Vehicles":"Vehículos",channel="test",retail=retail,files=new List<FileRef>{put("GTAVXR_SourceBaseline.exe","y")},runtime=new List<FileRef>(),
                notes=new List<string>{lang=="en"?"Open doors, bonnet, boot and ramps by grabbing them with your free hand.":"Abre puertas, capó, maletero y rampas agarrándolos con la mano libre."},mvga="switch",mvga_arg="-gtavxrMvga",launch=new LaunchContract{args=new List<string>()}};
            lib.SaveVersion(stable);lib.SaveVersion(test);
            var mvga=new ModManifest{id="mvga-2-65-vr",name="Make Visuals Great Again",version=lang=="en"?"2.65 · adapted to VR":"2.65 · adaptado a VR",category="graphics",loadable=true,kind="mvga-native",family="mvga",
                files=new List<FileRef>{new FileRef{path="gtavxr_mvga147/common/x",sha256=new string('A',64),bytes=33554432}},args=new List<string>{"-gtavxrMvga"}};
            var drive=new ModManifest{id="mod-drivev",name="DriveV",version="7.5",category="driving",loadable=false,kind="archive",family="drivev",files=new List<FileRef>(),archive=new FileRef{path="DriveV.zip",sha256=new string('B',64),bytes=22999437}};
            var forests=new ModManifest{id="mod-forests",name="Forests of San Andreas",version="5.7",category="map",loadable=false,kind="archive",family="forests",files=new List<FileRef>(),archive=new FileRef{path="f.zip",sha256=new string('C',64),bytes=412000000}};
            lib.SaveMod(mvga);lib.SaveMod(drive);lib.SaveMod(forests);
            var imp=new Importers(env);
            Func<string,string,string,int,bool,Profile> mk=(name,ver,title,color,vr)=>{
                string u=Path.Combine(root,"u-"+Ids.New());Directory.CreateDirectory(Path.Combine(u,"Default"));
                if(title!=null)File.WriteAllBytes(Path.Combine(u,"Default","SGTA50015"),EngineTests.FakeSave(title,4096));
                File.WriteAllText(Path.Combine(u,"settings.xml"),EngineTests.SettingsXml);
                string ini=null;if(vr){ini=Path.Combine(root,Ids.New()+".ini");File.WriteAllText(ini,"[VR]\n");}
                var p=imp.CreateFrom(name,ver,u,UserTree.Snapshot(u),ini,"demo");p=lib.Profile(p.id);p.color=color;lib.SaveProfile(p);return p;
            };
            var a=mk(lang=="en"?"VR Story":"Historia VR","125-dual",lang=="en"?"(Autosave) Complications (22.5%) - x":"(Autoguardado) Cambios (22.5%) - x",0,true);
            var b=mk(lang=="en"?"No mods":"Sin mods","125-dual",lang=="en"?"Franklin and Lamar (4.1%) - x":"Franklin y Lamar (4.1%) - x",2,true);
            var c=mk(lang=="en"?"Test 155":"Prueba 155","155v2",lang=="en"?"Pulling Favors (12.0%) - x":"Favores (12.0%) - x",1,false);c=lib.Profile(c.id);c.mods.Add(mvga.id);lib.SaveProfile(c);lib.SetTest(c.id,true);
            a=lib.Profile(a.id);a.last_played_utc=DateTime.UtcNow.AddDays(-2).ToString("o");
            if(view.StartsWith("play-gate-"))a.image=view.Substring("play-gate-".Length);   // --render play-gate-<id>: that profile picture
            lib.SaveProfile(a);
            b=lib.Profile(b.id);b.image="amber";lib.SaveProfile(b);c=lib.Profile(c.id);c.image="noir";lib.SaveProfile(c);
            string selected=a.id;
            if(view=="play-content"||view.StartsWith("play-personalize-")){
                // a 166 profile with the content mods the Suite builds at Play (World of Variety, WoV Plus, V4EVER) and a few changed options
                var world=new VersionManifest{id="166",build="166",title=lang=="en"?"Content mods":"Mods de contenido",channel="stable",content="profile",retail=retail,files=new List<FileRef>{put("GTAVXR_SourceBaseline.exe","z")},runtime=new List<FileRef>(),
                    notes=new List<string>{"x"},mvga="switch",mvga_arg="-gtavxrMvga",launch=new LaunchContract{args=new List<string>()}};
                lib.SaveVersion(world);
                Func<string,string,string,string,ModManifest> cm=(id,name,ver,cat)=>{var m=new ModManifest{id=id,name=name,version=ver,category=Content.ContentCatalog.Mod(cat).category??"map",loadable=true,kind="content-source",family="content-src-"+cat,
                    files=new List<FileRef>(),args=new List<string>(),added_utc=DateTime.UtcNow.ToString("o"),tested_utc=DateTime.UtcNow.ToString("o"),source=new ContentSourceInfo{format=cat=="v4ever"?"pack":"oiv",catalog=cat,pack=cat=="v4ever"?"v4evermod":null}};lib.SaveMod(m);return m;};
                var wov=cm("demo-wov","World of Variety","8.0","wov");var plus=cm("demo-wovplus","World Of Variety Plus","10.1","wovplus");var v4=cm("demo-v4ever","V4EVER","1.0","v4ever");
                var d=mk(lang=="en"?"Living city":"Ciudad viva","166",lang=="en"?"Pulling Favors (12.0%) - x":"Favores (12.0%) - x",3,true);d=lib.Profile(d.id);
                d.mods=new List<string>{wov.id,plus.id,v4.id};
                d.options=new Dictionary<string,string>{{"wov.traffic.deformation","off"},{"wov.peds.umbrellas","off"},{"wov.variety","1.5"},{"wovplus.extra_density","on"},{"wovplus.extra_density.peds_density","120"}};
                lib.SaveProfile(d);
                File.WriteAllText(lib.VrSettingsFile(d.id),"[VR]\r\nmods_peds166=70\r\nmods_traffic166=80\r\n");
                selected=d.id;
            }
            if(view=="play-graphics"||view=="settings"){
                // 167-neural: a suite-mode version with the per-eye DLSS chain, a profile on the Neural preset, example DLSS files
                var nv=new VersionManifest{id="167-neural",build="167",title=lang=="en"?"Neural graphics (DLSS)":"Graficos neurales (DLSS)",channel="test",retail=retail,files=new List<FileRef>{put("GTAVXR_SourceBaseline.exe","n")},runtime=new List<FileRef>(),
                    notes=new List<string>{"x"},mvga="none",neural=new NeuralContract{dlss_dir_env="GTAVXR_DLSS_DIR"},launch=new LaunchContract{args=new List<string>(),session_mode="suite",settings_mode="suite"}};
                lib.SaveVersion(nv);
                var g=mk(lang=="en"?"Neural":"Neural","167-neural",lang=="en"?"Pulling Favors (12.0%) - x":"Favores (12.0%) - x",4,true);
                File.WriteAllText(lib.VrSettingsFile(g.id),Graphics.Write("[VR]\r\n",new GraphicsChoice{preset="neural",upscale="quality",neural=true,intensity=60}));
                string dl=env.Dir("dlss");
                NeuralSuiteTests.FakeDll(Path.Combine(dl,Dlss.SrModel),"NVIDIA Corporation","NVIDIA DLSS","310.8.0.0",Dlss.Exports);
                NeuralSuiteTests.FakeDll(Path.Combine(dl,Dlss.NrModel),"NVIDIA Corporation","NVIDIA DLSS NR","310.8.0.0",Dlss.Exports);
                if(view=="play-graphics")selected=g.id;
            }
            Json.Atomic(Path.Combine(env.Dir("game"),"state.json"),new GameState{schema="gtavxr-suite2-game-v1",game_dir=env.GameDir,version="125-dual",mods=new List<string>(),files=new List<FileRef>{new FileRef{path="x",sha256=new string('D',64),bytes=251658240}},dirs=new List<string>(),updated_utc=DateTime.UtcNow.ToString("o")});
            env.GameDir=@"C:\Program Files (x86)\Steam\steamapps\common\Grand Theft Auto V";
            env.OpenXrManifest=@"C:\Program Files\Virtual Desktop Streamer\OpenXR\virtualdesktop-openxr.json";
            env.SaveSettings(new SuiteSettings{language=lang,first_run_done=true,dev_mode=view!="play"&&view!="mods"&&view!="play-content"&&!view.StartsWith("play-personalize-"),selected_profile=selected,game_dir=env.GameDir});
            return env;
        }
    }

    // Read-only check of the real machine: imports the real versions into a FRESH Suite folder and reports what Play would do.
    // Never writes to the game folder, the saves folder, the repository or the real Suite data. No GTA.
    public static class DryRun {
        public static object Run(string fresh,string dummy){
            fresh=SafeFs.Root(fresh);if(Directory.Exists(fresh))throw new InvalidDataException("Fresh folder required");
            var env=Env.Product(fresh);
            env.GameDir=Detect.GameCandidates().FirstOrDefault();env.OpenXrManifest=Detect.OpenXrRuntime();env.DevRepo=Detect.DevRepo();
            if(env.GameDir==null||env.DevRepo==null)throw new InvalidDataException("Game or repository not found");
            var imp=new Importers(env);var lib=new Library(env);
            var report=new Dictionary<string,object>();
            report["game"]=env.GameDir;report["openxr"]=env.OpenXrManifest;report["importable"]=imp.ImportableReleases();
            var retailProof=LegacyGamePolicy.Inspect(env.GameDir);report["retail"]=retailProof.status;
            var userBefore=UserTree.Snapshot(env.UserDataDir).Count;
            FirstRun.Bring(env,true);
            var versions=lib.Versions();var plans=new List<object>();
            foreach(var p in lib.Profiles()){
                var v=lib.Version(p.version);var ps=new PlaySession(env);var mods=ps.ActiveMods(p,v);
                var plan=new GameLayer(env).Preview(v,mods);
                var args=new List<string>();foreach(string a in v.launch.args)args.Add(a=="{LOGFILE}"?"<session>\\native-runtime.log":a);
                foreach(var m in mods)if(m.args!=null)foreach(string a in m.args)if(!args.Contains(a))args.Add(a);
                plans.Add(new{profile=p.name,version=v.id,channel=v.channel,mods=mods.Select(m=>m.name).ToList(),install_add=plan.add,install_remove=plan.remove,install_keep=plan.keep,
                    install_mb=Math.Round(plan.bytes/1048576.0,1),saves=p.user.Count,vr_settings=p.has_vr_settings,latest_save=(SaveInfo.Latest(lib.UserDir(p.id))??new SaveInfo()).mission,
                    args=String.Join(" ",args),disable_layers=v.launch.disable_layers,runtime=v.runtime.Select(f=>f.path).ToList()});
            }
            report["versions"]=versions.Select(v=>new{v.id,v.build,v.title,v.channel,files=v.files.Count,v.mvga,mb=Math.Round(v.Bytes()/1048576.0,1)}).ToList();
            report["mods"]=lib.Mods().Select(m=>new{m.id,m.name,files=m.files.Count,m.loadable}).ToList();
            report["profiles"]=plans;
            report["user_folder_untouched"]=UserTree.Snapshot(env.UserDataDir).Count==userBefore;
            report["game_written"]=false;report["gta_launched"]=false;report["status"]="PASS_DRY_RUN_READ_ONLY";
            return report;
        }
    }
}
