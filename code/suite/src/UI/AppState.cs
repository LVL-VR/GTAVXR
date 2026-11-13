using System;
using System.IO;
using System.Linq;
using System.Collections.Generic;
using System.Threading.Tasks;
using System.Windows.Threading;

namespace GtavxrSuite {
    // What the window shows, read from the Suite data. Reload() after every change.
    public sealed class AppState {
        public Env Env {get;private set;}
        public Library Lib {get;private set;}
        public Importers Imp {get;private set;}
        public PlaySession Session {get;private set;}
        public SuiteSettings Settings {get;private set;}
        public List<Profile> Profiles=new List<Profile>();
        public List<VersionManifest> Versions=new List<VersionManifest>();
        public List<ModManifest> Mods=new List<ModManifest>();
        public GameState Installed;
        public LeaseHolder Lease;
        public bool StreamerRunning,GameFound,Recovery;
        public string RuntimeName;      // the OpenXR runtime a Play would use now (null = none usable)
        public XrRuntime Runtime;       // 2.4.0: that runtime as resolved from the player's choice
        public bool? RuntimeUp;         // its process is up; null = no indicator for this runtime (neutral chip)
        public bool Busy;               // a play session or a long task is running
        public bool Demo;               // preview rendering: no live detection
        public event Action Changed;

        public AppState(Env env){Env=env;Lib=new Library(env);Imp=new Importers(env);Session=new PlaySession(env);}

        public void Reload(){
            Settings=Env.LoadSettings();L.Lang=Settings.language=="en"?"en":"es";
            Versions=Lib.Versions();Mods=Lib.Mods();Profiles=Lib.Profiles();
            try{Installed=new GameLayer(Env).State();}catch(Exception){Installed=null;}
            if(!Demo){
                GameFound=Env.GameDir!=null&&File.Exists(Path.Combine(Env.GameDir,"GTA5.exe"));
                try{Runtime=XrRuntimes.Resolve(Settings.openxr_runtime,Env.XrActive!=null?Env.XrActive():Env.OpenXrManifest);}catch(Exception){Runtime=new XrRuntime{error="XR_MANIFEST_BAD"};}
                RuntimeName=Runtime.error==null?Runtime.name:null;
                RuntimeUp=RuntimeName==null?(bool?)false:XrRuntimes.Running(RuntimeName);
                StreamerRunning=RuntimeUp==true;
                Lease=Env.DevHost?PlayLease.Holder(Env.DevRepo):null;
                try{Recovery=Session.Unfinished().Count>0||new GameLayer(Env).Pending;}catch(Exception){Recovery=true;}
            }
            if(Settings.selected_profile==null||!Visible().Any(p=>p.id==Settings.selected_profile)){
                var first=Visible().FirstOrDefault();Settings.selected_profile=first==null?null:first.id;
            }
            if(Changed!=null)Changed();
        }
        public void Save(){Env.SaveSettings(Settings);}
        // A view subscribes to its own result. The inspection and hashing stay off the dispatcher.
        DlssFolder dlss;
        string dlssKey;
        DateTime dlssAt=DateTime.MinValue;
        Task<DlssFolder> dlssRequest;
        int dlssGeneration;
        public void ShowDlss(Action<DlssFolder,bool> show){
            string key=Settings==null?null:Settings.dlss_dir;
            if(Demo){show(FindDlss(key),false);return;}
            if(dlssKey==key&&(DateTime.UtcNow-dlssAt).TotalSeconds<30){show(dlss,false);return;}
            if(dlssRequest==null||dlssKey!=key){
                dlssKey=key;dlssAt=DateTime.MinValue;
                dlssGeneration++;
                dlssRequest=Task.Run(()=>FindDlss(key));
            }
            var request=dlssRequest;
            int generation=dlssGeneration;
            var dispatcher=Dispatcher.CurrentDispatcher;
            show(null,true);
            request.ContinueWith(done=>dispatcher.BeginInvoke(DispatcherPriority.Background,new Action(()=>{
                if(generation!=dlssGeneration||(Settings==null?null:Settings.dlss_dir)!=key)return;
                if(dlssRequest==request){dlss=done.Result;dlssAt=DateTime.UtcNow;dlssRequest=null;}
                show(done.Result,false);
            })),TaskScheduler.Default);
        }
        DlssFolder FindDlss(string key){
            try{return Dlss.Find(key,Env.Dir("dlss"),Env.DlssSearchRoots!=null?Env.DlssSearchRoots():new List<string>());}
            catch(Exception){return null;}
        }
        public OfxrState OfxrNow(){
            try{return Ofxr.Read(Env.OfxrRoot,Env.OfxrLayer!=null?Env.OfxrLayer():null,Settings==null?null:Settings.ofxr_tray);}catch(Exception){return new OfxrState();}
        }
        public bool Dev{get{return Settings!=null&&Settings.dev_mode;}}
        public VersionManifest Version(string id){return Versions.FirstOrDefault(v=>v.id==id);}
        public ModManifest Mod(string id){return Mods.FirstOrDefault(m=>m.id==id);}
        public bool IsTest(Profile p){var v=Version(p.version);return v!=null&&v.channel=="test";}
        // Test-version profiles exist only in developer mode.
        public List<Profile> Visible(){return Profiles.Where(p=>Dev||!IsTest(p)).ToList();}
        public Profile Selected{get{return Profiles.FirstOrDefault(p=>Settings!=null&&p.id==Settings.selected_profile);}}
        public void Select(string id){Settings.selected_profile=id;Save();if(Changed!=null)Changed();}
        public List<ModManifest> ActiveMods(Profile p){
            var v=Version(p.version);var list=new List<ModManifest>();if(v==null)return list;
            foreach(string id in p.mods){var m=Mod(id);if(m!=null&&m.WorksWith(v))list.Add(m);}
            if(v.mvga=="required"&&!list.Any(m=>m.kind=="mvga-native")){var mv=Mods.FirstOrDefault(m=>m.kind=="mvga-native"&&m.loadable);if(mv!=null)list.Add(mv);}
            return list;
        }
        public SaveInfo LatestSave(Profile p){try{return SaveInfo.Latest(Lib.UserDir(p.id));}catch(Exception){return null;}}
        public string ModsShort(Profile p){
            var a=ActiveMods(p);if(a.Count==0)return "—";
            return String.Join(" ",a.Select(m=>m.kind=="mvga-native"?"MVGA":new string(m.name.Where(Char.IsLetterOrDigit).Take(6).ToArray())));
        }
    }
}
