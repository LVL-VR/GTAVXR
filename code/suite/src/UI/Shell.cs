using System;
using System.IO;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Shell;
using System.Windows.Threading;
using System.Threading.Tasks;
using System.Windows.Media.Animation;
using System.Collections.Generic;

namespace GtavxrSuite {
    // The window frame: title bar with the three sections, the current view, and an overlay layer for dialogs,
    // the playing screen and short notices.
    public sealed class Shell {
        public readonly AppState S;
        public readonly Grid Root=new Grid{Background=Theme.WindowBrush()};
        readonly ContentControl view=new ContentControl();
        readonly Grid overlay=new Grid{Visibility=Visibility.Collapsed};
        readonly Border toast=new Border{Visibility=Visibility.Collapsed};
        readonly StackPanel nav=new StackPanel{Orientation=Orientation.Horizontal,Margin=new Thickness(22,0,0,0)};
        public string Section="play";
        PlayView play;
        public Window Window;
        public Shell(AppState state){
            S=state;
            Root.RowDefinitions.Add(new RowDefinition{Height=new GridLength(TitleHeight)});Root.RowDefinitions.Add(new RowDefinition());
            Root.Children.Add(TitleBar());
            Grid.SetRow(view,1);Root.Children.Add(view);
            Grid.SetRowSpan(overlay,2);Root.Children.Add(overlay);
            toast.HorizontalAlignment=HorizontalAlignment.Center;toast.VerticalAlignment=VerticalAlignment.Bottom;toast.Margin=new Thickness(0,0,0,28);
            toast.Background=Theme.B(Theme.Ticket);toast.CornerRadius=new CornerRadius(8);toast.Padding=new Thickness(18,10,18,10);
            Grid.SetRowSpan(toast,2);Root.Children.Add(toast);
            S.Changed+=()=>Refresh();
        }

        public const double TitleHeight=60;
        UIElement TitleBar(){
            var bar=new DockPanel{Background=Theme.B(Theme.C("#020306")),LastChildFill=false};
            // The logo, and the terminal's name on the wayfinding line.
            FrameworkElement mark=Assets.Logo!=null?(FrameworkElement)U.Img(Assets.Logo,48):U.VBadge(34);
            var place=U.Label("Terminal Los Santos",Theme.Mist);place.VerticalAlignment=VerticalAlignment.Center;place.Margin=new Thickness(0,2,0,0);
            var brand=U.Row(12,mark,new System.Windows.Shapes.Rectangle{Width=1,Height=22,Fill=Theme.B(Theme.Line),VerticalAlignment=VerticalAlignment.Center},place);
            brand.Margin=new Thickness(14,0,0,0);brand.VerticalAlignment=VerticalAlignment.Center;
            bar.Children.Add(brand);nav.VerticalAlignment=VerticalAlignment.Center;bar.Children.Add(nav);
            var ctl=new StackPanel{Orientation=Orientation.Horizontal,VerticalAlignment=VerticalAlignment.Stretch};
            foreach(var kind in new[]{"min","max","close"}){
                var b=new Button{Style=(Style)Application.Current.Resources["Plain"],Width=46,Background=Brushes.Transparent};
                var path=new System.Windows.Shapes.Path{Stroke=Theme.B(Theme.Mist),StrokeThickness=1.2,Width=10,Height=10,Stretch=Stretch.Uniform,
                    Data=Geometry.Parse(kind=="min"?"M0,5 L10,5":kind=="max"?"M0,0 L10,0 L10,10 L0,10 Z":"M0,0 L10,10 M10,0 L0,10")};
                b.Content=new Grid{Children={path}};((Grid)b.Content).Background=Brushes.Transparent;
                string k=kind;b.Click+=(o,e)=>{if(Window==null)return;if(k=="min")Window.WindowState=WindowState.Minimized;else if(k=="max")Window.WindowState=Window.WindowState==WindowState.Maximized?WindowState.Normal:WindowState.Maximized;else Window.Close();};
                b.MouseEnter+=(o,e)=>b.Background=Theme.B(k=="close"?Theme.C("#C42B1C"):Theme.Flap);b.MouseLeave+=(o,e)=>b.Background=Brushes.Transparent;
                WindowChrome.SetIsHitTestVisibleInChrome(b,true);ctl.Children.Add(b);
            }
            DockPanel.SetDock(ctl,Dock.Right);bar.Children.Add(ctl);
            // A thin line of sky under the bar, like light through the terminal glass.
            var g=new Grid();g.Children.Add(bar);
            g.Children.Add(new System.Windows.Shapes.Rectangle{Height=1,Fill=Theme.SkyLine(),VerticalAlignment=VerticalAlignment.Bottom,IsHitTestVisible=false});
            return g;
        }
        void BuildNav(){
            nav.Children.Clear();
            if(S.Settings==null||!S.Settings.first_run_done)return;
            foreach(var k in new[]{"play","mods","settings"}){
                var rb=new RadioButton{Content=L.S("nav."+k),Style=(Style)Application.Current.Resources["Nav"],IsChecked=Section==k,GroupName="nav",Margin=new Thickness(0,0,4,0)};
                string kk=k;rb.Checked+=(o,e)=>{if(Section!=kk){Section=kk;Refresh();}};
                WindowChrome.SetIsHitTestVisibleInChrome(rb,true);nav.Children.Add(rb);
            }
        }
        public void Refresh(){
            BuildNav();
            if(S.Settings!=null&&!S.Settings.first_run_done){view.Content=new FirstRunView(S,this).Build();lastScrollKey=null;return;}
            // build168 Suite: a change / toggle rebuilds the section; the same section and profile keep their scroll position
            string scrollKey=Section+"|"+(S.Selected==null?"":S.Selected.id);
            var oldScroll=SectionScroll(view.Content as DependencyObject);
            double keepOffset=(oldScroll!=null&&scrollKey==lastScrollKey)?oldScroll.VerticalOffset:-1;
            lastScrollKey=scrollKey;
            switch(Section){
                case "mods":view.Content=new ModsView(S,this).Build();break;
                case "settings":view.Content=new SettingsView(S,this).Build();break;
                default:
                    string before=S.Selected==null?null:S.Selected.id;
                    play=new PlayView(S,this);view.Content=play.Build();
                    if(lastSelected!=null&&before!=lastSelected)play.FlipSelected();
                    lastSelected=before;break;
            }
            var newRoot=view.Content as FrameworkElement;
            if(newRoot!=null&&keepOffset>0){
                double off=keepOffset;
                RoutedEventHandler restore=null;
                restore=(o,e)=>{
                    newRoot.Loaded-=restore;
                    var scroll=SectionScroll(newRoot);
                    if(scroll!=null)scroll.ScrollToVerticalOffset(off);
                };
                newRoot.Loaded+=restore;
            }
        }
        static ScrollViewer SectionScroll(DependencyObject root){
            if(root==null)return null;
            var viewer=root as ScrollViewer;
            if(viewer!=null)return viewer;
            for(int index=0;index<VisualTreeHelper.GetChildrenCount(root);index++){
                viewer=SectionScroll(VisualTreeHelper.GetChild(root,index));
                if(viewer!=null)return viewer;
            }
            return null;
        }
        string lastSelected;
        string lastScrollKey;

        // ---------- overlays ----------
        public void ShowOverlay(UIElement card,bool dim=true){
            overlay.Children.Clear();
            overlay.Background=dim?Theme.B(Theme.C("#000000"),0.62):Theme.WindowBrush();
            overlay.Children.Add(card);overlay.Visibility=Visibility.Visible;
        }
        public void CloseOverlay(){overlay.Children.Clear();overlay.Visibility=Visibility.Collapsed;}
        public Border Card(UIElement content,double width=560){
            return new Border{Child=content,Width=width,Background=Theme.PanelBrush(),BorderBrush=Theme.RimBrush(),BorderThickness=new Thickness(1.5),
                CornerRadius=new CornerRadius(14),Padding=new Thickness(26,22,26,22),HorizontalAlignment=HorizontalAlignment.Center,VerticalAlignment=VerticalAlignment.Center};
        }
        public void Confirm(string title,string body,string yes,Action onYes,bool danger){
            var buttons=U.Row(10,U.Btn(yes,danger?"Danger":"Primary",()=>{CloseOverlay();Guard(onYes);}),U.Btn(L.S("cancel"),"Ghost",CloseOverlay));
            buttons.HorizontalAlignment=HorizontalAlignment.Right;buttons.Margin=new Thickness(0,8,0,0);
            ShowOverlay(Card(U.Col(10,U.Text(title,24,Theme.Text,true,FontWeights.SemiBold),U.Text(body,15,Theme.Mist),buttons)));
        }
        public void Info(string title,string body){
            var ok=U.Btn(L.S("ok"),"Primary",CloseOverlay);ok.HorizontalAlignment=HorizontalAlignment.Right;
            ShowOverlay(Card(U.Col(10,U.Text(title,24,Theme.Text,true,FontWeights.SemiBold),U.Text(body,15,Theme.Mist),ok)));
        }
        public void Toast(string text){
            toast.Child=U.Text(text,14,Theme.Ink,true,FontWeights.SemiBold);toast.Visibility=Visibility.Visible;
            var t=new DispatcherTimer{Interval=TimeSpan.FromSeconds(3.2)};t.Tick+=(o,e)=>{t.Stop();toast.Visibility=Visibility.Collapsed;};t.Start();
        }
        public void Guard(Action a){
            try{a();}catch(Exception e){Info(L.S("err.title"),Explain(e));S.Reload();}
        }
        public static string Explain(Exception e){
            if(e is GameConflictException)return L.S("err.conflict")+"\n"+e.Message;
            string m=e.Message??"";
            if(m=="GAME_RUNNING")return L.S("err.running");
            if(m=="VERSION_MISSING")return L.S("err.version");
            if(m=="INSTALL_REQUIRED")return L.S("dist.required");
            if(m.StartsWith("INSTALL_RUNTIME_CHANGED:",StringComparison.Ordinal))return L.S("dist.runtime")+"\n"+m.Substring("INSTALL_RUNTIME_CHANGED:".Length);
            if(m=="DELTA_BASE_MISMATCH"||m.StartsWith("INSTALL_BASE_CHANGED",StringComparison.Ordinal))return L.S("dist.base")+"\n"+m;
            if(m=="MVGA_REQUIRED")return L.S("err.mvga");
            if(m=="CONTENT_PROFILE_TWICE")return L.S("err.content2");
            if(m=="NO_CONTENT_MODS")return L.S("err.nocontent");
            if(m=="RECOVERY_REQUIRED")return L.S("rec.body");
            if(m=="NO_SPACE")return L.S("err.space");
            if(m=="NOT_LEGACY_3889")return L.S("err.retail");
            if(m=="LEGACY_REPO_ONLY")return L.S("err.legacyrepo");
            if(m=="DATA_PATH_NOT_ANSI")return L.S("err.ansi");
            if(m=="DATA_PATH_TOO_LONG"||m=="DATA_PATH_NOT_LOCAL")return L.S("compat.local_path")+"\n"+m;
            if(m.StartsWith("RETAIL_RESOURCE_MISMATCH:",StringComparison.Ordinal)||
               m.StartsWith("RETAIL_SHADER_BASE_MISMATCH:",StringComparison.Ordinal))return L.S("dist.base")+"\n"+m;
            if(m=="XR_RUNTIME:XR_NO_ACTIVE_RUNTIME")return L.S("err.xr.none");
            if(m=="XR_RUNTIME:XR_MANIFEST_MISSING")return L.S("err.xr.missing");
            if(m.StartsWith("XR_RUNTIME:"))return L.S("err.xr.bad")+"\n"+m.Substring("XR_RUNTIME:".Length);
            if(m.StartsWith("File identity mismatch: "))return L.S("err.identity")+"\n"+m.Substring("File identity mismatch: ".Length);
            if(m=="ARCHIVE_PASSWORD")return L.S("err.password");
            if(m.StartsWith("Unexpected original; no overwrite authorized: "))return L.S("err.conflict")+"\n"+m.Substring("Unexpected original; no overwrite authorized: ".Length);
            // Every other engine message is technical: the player reads a plain sentence, the detail stays below it.
            return L.S("err.generic")+"\n\n"+m;
        }

        // ---------- dialogs ----------
        // New profile. test=true starts it as a test profile; modId: "Probar" from the library (that mod already on).
        public void NewProfile(){NewProfile(false,null);}
        public void NewProfile(bool test,string modId){
            var mod=modId==null?null:S.Mod(modId);
            var name=new TextBox{Text=mod!=null?L.F("try.name",ShortName(mod)):L.S(test?"new.name.test":"new.name"),Style=(Style)Application.Current.Resources["Field"]};
            var versions=S.Versions.Where(v=>(S.Dev||v.channel!="test")&&(mod==null||mod.WorksWith(v))).ToList();
            string vsel=S.Selected!=null&&versions.Any(v=>v.id==S.Selected.version)?S.Selected.version:(versions.Count>0?versions[0].id:null);
            string mode=S.Selected!=null&&test?"dup":"copy";int color=S.Profiles.Count%Theme.Bands.Length;
            bool testOn=test;
            var vrow=new WrapPanel();var srow=new WrapPanel();
            var testSwitch=new CheckBox{Style=(Style)Application.Current.Resources["Switch"],IsChecked=testOn,VerticalAlignment=VerticalAlignment.Center};
            var tape=U.Tape(null,8);tape.Visibility=testOn?Visibility.Visible:Visibility.Collapsed;
            testSwitch.Click+=(o,e)=>{testOn=testSwitch.IsChecked==true;tape.Visibility=testOn?Visibility.Visible:Visibility.Collapsed;};
            Action paint=null;
            paint=()=>{
                vrow.Children.Clear();srow.Children.Clear();
                foreach(var v in versions){string id=v.id;var b=U.Btn(v.build+" "+v.Title(L.Lang),vsel==id?"Primary":"Btn",()=>{vsel=id;paint();});b.Margin=new Thickness(0,0,8,8);vrow.Children.Add(b);}
                var opts=new System.Collections.Generic.List<Tuple<string,string>>{Tuple.Create("copy",L.S("new.saves.copy")),Tuple.Create("fresh",L.S("new.saves.fresh"))};
                if(S.Selected!=null)opts.Add(Tuple.Create("dup",L.F("new.saves.dup",S.Selected.name)));
                foreach(var o in opts){string m=o.Item1;var b=U.Btn(o.Item2,mode==m?"Primary":"Btn",()=>{mode=m;paint();});b.Margin=new Thickness(0,0,8,8);srow.Children.Add(b);}
            };
            paint();
            var create=U.Btn(L.S("new.create"),"Primary",()=>{
                string n=(name.Text??"").Trim();if(n.Length==0||vsel==null)return;
                CloseOverlay();
                Guard(()=>{
                    Profile p;
                    string dupOf=mode=="dup"&&S.Selected!=null?S.Selected.id:null;
                    if(testOn)p=S.Imp.CreateTestProfile(n,vsel,dupOf,mode=="copy");
                    else if(dupOf!=null){p=S.Imp.Duplicate(dupOf,n);p.version=vsel;}
                    else p=S.Imp.CreateProfile(n,vsel,mode=="copy",null);   // VR settings: the "Historia VR" list (S2)
                    p=S.Lib.Profile(p.id);p.color=color;p.version=vsel;
                    var v=S.Version(vsel);
                    if(mod!=null){if(!p.mods.Contains(mod.id))p.mods.Add(mod.id);}
                    else{var mv=S.Mods.FirstOrDefault(m=>m.kind=="mvga-native");if(mode!="dup"&&mv!=null&&v!=null&&v.mvga!="none"&&!p.mods.Contains(mv.id))p.mods.Add(mv.id);}
                    S.Lib.SaveProfile(p);S.Reload();S.Select(p.id);
                    if(Section!="play"){Section="play";Refresh();}
                    if(mod!=null)Toast(L.F("try.ready",p.name));
                });
            });
            var buttons=U.Row(10,create,U.Btn(L.S("cancel"),"Ghost",CloseOverlay));buttons.HorizontalAlignment=HorizontalAlignment.Right;
            var testRow=new DockPanel{LastChildFill=true};
            DockPanel.SetDock(testSwitch,Dock.Right);testRow.Children.Add(testSwitch);
            testRow.Children.Add(U.Col(2,U.Text(L.S("new.test"),17,Theme.Text,true,FontWeights.SemiBold),U.Text(L.S("new.test.hint"),13,Theme.Mist)));
            var title=mod!=null?L.F("try.title",mod.name):L.S("new.title");
            var body=U.Col(14,new StickerText(title.ToUpperInvariant(),28),
                U.Col(6,U.Label(L.S("prof.name"),Theme.Mist),name),U.Col(6,U.Label(L.S("f.version"),Theme.Mist),vrow),U.Col(6,U.Label(L.S("new.saves"),Theme.Mist),srow),
                U.Col(8,tape,testRow),buttons);
            ShowOverlay(Card(body,640));
            name.Focus();name.SelectAll();
        }
        static string ShortName(ModManifest m){return m.kind=="mvga-native"?"MVGA":m.name;}

        // Mod library "Probar": choose a test profile for this mod (or make one). Nothing changes in normal profiles.
        public void TryMod(ModManifest m){
            if(!S.Versions.Any(v=>m.WorksWith(v)&&(S.Dev||v.channel!="test"))){Info(L.F("try.title",m.name),L.S("try.none"));return;}
            var list=new StackPanel();
            foreach(var p in S.Profiles.Where(x=>x.test)){
                var v=S.Version(p.version);bool works=m.WorksWith(v);
                string pid=p.id;
                var row=U.Btn(p.name+"  ·  "+(v==null?"—":v.build+" "+v.Title(L.Lang))+(works?"":"  ·  "+L.S("mods.no.version")),"Btn",()=>{
                    CloseOverlay();
                    Guard(()=>{var q=S.Lib.Profile(pid);if(!q.mods.Contains(m.id))q.mods.Add(m.id);S.Lib.SaveProfile(q);S.Reload();S.Select(pid);
                        PlayView.EditorOpen=true;PlayView.EditorTab="mods";Section="play";Refresh();Toast(L.F("try.ready",q.name));});
                });
                row.HorizontalContentAlignment=HorizontalAlignment.Left;row.IsEnabled=works;row.Margin=new Thickness(0,0,0,8);list.Children.Add(row);
            }
            if(list.Children.Count==0)list.Children.Add(U.Text(L.S("try.notest"),14,Theme.Mist));
            var fresh=U.Btn(L.S("try.new"),"Primary",()=>{CloseOverlay();NewProfile(true,m.id);});
            var buttons=U.Row(10,fresh,U.Btn(L.S("cancel"),"Ghost",CloseOverlay));buttons.HorizontalAlignment=HorizontalAlignment.Right;
            ShowOverlay(Card(U.Col(14,U.Tape(L.S("tape.flight"),22),new StickerText(L.F("try.title",m.name).ToUpperInvariant(),28),U.Text(L.S("try.body"),15,Theme.Mist),
                U.Col(6,U.Label(L.S("try.pick"),Theme.Mist),list),buttons),620));
        }

        // "Pasar a mi perfil": a normal profile takes the test profile's version + mods.
        public void Promote(Profile tp){
            var targets=S.Profiles.Where(x=>!x.test&&x.id!=tp.id).ToList();
            var v=S.Version(tp.version);var mods=S.ActiveMods(tp);
            string what=(v==null?"—":v.build+" "+v.Title(L.Lang))+(mods.Count>0?" + "+String.Join(", ",mods.Select(ShortName)):"");
            if(targets.Count==0){Info(L.S("pr.title"),L.S("pr.none"));return;}
            var list=new StackPanel();
            foreach(var t in targets){
                var tv=S.Version(t.version);string tid=t.id,tname=t.name;
                var b=U.Btn(t.name+"  ·  "+(tv==null?"—":tv.build+" "+tv.Title(L.Lang)),"Btn",()=>Confirm(L.S("pr.title"),L.F("pr.q",tname,what),L.S("pr.go"),()=>{
                    S.Imp.Promote(tp.id,tid);S.Reload();S.Select(tid);Toast(L.F("pr.done",tname));},false));
                b.HorizontalContentAlignment=HorizontalAlignment.Left;b.Margin=new Thickness(0,0,0,8);list.Children.Add(b);
            }
            var cancel=U.Btn(L.S("cancel"),"Ghost",CloseOverlay);cancel.HorizontalAlignment=HorizontalAlignment.Right;
            ShowOverlay(Card(U.Col(14,new StickerText(L.S("pr.title").ToUpperInvariant(),28),U.Text(L.F("pr.body",what),15,Theme.Mist),list,cancel),600));
        }

        // What the way back found after a test flight.
        public void FlightResult(SessionRecord r){
            var p=S.Lib.Profile(r.profile);if(p==null)return;
            var pv=r.prev_version==null?null:S.Version(r.prev_version);
            var pm=(r.prev_mods??new List<string>()).Select(id=>S.Mod(id)).Where(m=>m!=null).Select(ShortName).ToList();
            string before=pv==null?L.S("fl.nothing"):pv.build+" "+pv.Title(L.Lang)+(pm.Count>0?" + "+String.Join(", ",pm):"");
            var lines=new StackPanel();
            Action<int,string,string> line=(kind,text,sub)=>{
                Color c=kind==0?Theme.Go:kind==1?Theme.Hazard:Theme.Stop;
                var mark=new Border{Width=22,Height=22,CornerRadius=new CornerRadius(11),Background=Theme.B(c,0.18),VerticalAlignment=VerticalAlignment.Top,Margin=new Thickness(0,1,0,0),
                    Child=new TextBlock{Text=kind==0?"✓":"!",Foreground=Theme.B(c),FontFamily=Theme.Sign_,FontWeight=FontWeights.Bold,HorizontalAlignment=HorizontalAlignment.Center,VerticalAlignment=VerticalAlignment.Center}};
                var g=new Grid{Margin=new Thickness(0,0,0,10)};g.ColumnDefinitions.Add(new ColumnDefinition{Width=new GridLength(34)});g.ColumnDefinitions.Add(new ColumnDefinition());
                g.Children.Add(mark);var c2=U.Col(2,U.Text(text,15,Theme.Text));if(!String.IsNullOrEmpty(sub)){var st=U.Text(sub,12,Theme.Mist);c2.Children.Add(st);}
                Grid.SetColumn(c2,1);g.Children.Add(c2);lines.Children.Add(g);
            };
            line(0,L.F("fl.game",before),null);
            line(0,L.S("fl.normal"),null);
            var save=S.LatestSave(p);
            line(0,save==null?L.F("fl.save.none",p.name):L.F("fl.save",p.name,save.Short(L.Lang)),null);
            int nq=r.quarantined==null?0:r.quarantined.Count,nc=r.changed_game_files==null?0:r.changed_game_files.Count;
            if(nq>0)line(1,L.F("fl.quar",nq),System.IO.Path.Combine(S.Env.DataRoot,"quarantine",r.id));
            if(nc>0)line(1,L.F("fl.changed",nc),String.Join(", ",r.changed_game_files.Take(4))+(nc>4?" …":""));
            int nm=r.missing_game_files==null?0:r.missing_game_files.Count;
            if(nm>0)line(2,L.F("fl.missing",nm),String.Join(", ",r.missing_game_files.Take(4))+(nm>4?" …":""));
            if(r.retail_check=="changed")line(2,L.S("fl.retail"),null);
            int na=(r.aside==null?0:r.aside.Count)-(r.aside_kept==null?0:r.aside_kept.Count);
            if(na>0)line(0,L.F("run.aside",na),null);
            if(r.exit_hang=="closed-after-exit-file")line(1,L.S("fl.exithang"),r.exit_dump);
            string word=r.aborted?L.S("fl.aborted"):r.result=="cancelled"?L.S("fl.cancelled"):L.S("fl.done");
            var undo=U.Btn(L.S("fl.undo"),"Ghost",()=>Confirm(L.S("fl.undo"),L.F("fl.undo.q",p.name),L.S("fl.undo"),()=>{S.Imp.UndoSession(p.id,r.id);S.Reload();Toast(L.S("fl.undone"));},false));
            undo.IsEnabled=S.Lib.ReadCopy(p.id,"history/"+r.id)!=null;
            var promote=U.Btn(L.S("fl.promote"),"Btn",()=>Promote(p));
            var done=U.Btn(L.S("fl.ok"),"Primary",CloseOverlay);
            var buttons=U.Row(10,undo,promote,done);buttons.HorizontalAlignment=HorizontalAlignment.Right;
            ShowOverlay(Card(U.Col(14,U.Tape(L.S("tape.flight"),22),new StickerText(word,30),U.Text(p.name,15,Theme.Mist),lines,buttons),660));
        }

        public void InstallEmbedded(string profileId){
            InstallEmbedded(profileId,new PlayOptions());
        }
        void InstallEmbedded(string profileId,PlayOptions options){
            RunTask(L.S("dist.install"),()=>{
                string installed=S.Session.InstallEmbedded(profileId,null,System.Threading.CancellationToken.None,options);
                var settings=S.Env.LoadSettings();settings.selected_profile=installed;S.Env.SaveSettings(settings);
                return L.S("dist.ready");
            },error=>{
                var optional=error as OptionalComponentException;
                if(optional==null) return false;
                Confirm(L.S("compat.title"),optional.Message+"\n\n"+L.S("compat.once"),L.S("compat.continue"),
                        ()=>InstallEmbedded(profileId,optional.Retry),false);
                return true;
            });
        }

        // ---------- play ----------
        public void Play(string profileId){
            Play(profileId,new PlayOptions());
        }
        void Play(string profileId,PlayOptions options){
            if(S.Busy)return;
            S.Busy=true;
            var p=S.Lib.Profile(profileId);var v=S.Version(p.version);
            var panel=new PlayingPanel(p,v);
            panel.OnAbort=()=>Task.Factory.StartNew(()=>S.Session.Abort());
            panel.OnClose=()=>S.Session.CloseGame();
            ShowOverlay(panel.Root,false);
            var started=DateTime.UtcNow;DispatcherTimer clock=null;string lastStep=null;
            Action<PlayProgress> progress=pp=>Root.Dispatcher.BeginInvoke((Action)(()=>{
                if(pp.step=="playing"){
                    panel.Playing();started=DateTime.UtcNow;panel.Clock(TimeSpan.Zero);
                    clock=new DispatcherTimer{Interval=TimeSpan.FromSeconds(1)};clock.Tick+=(o,e)=>panel.Clock(DateTime.UtcNow-started);clock.Start();
                }else{
                    if(pp.step!=lastStep){panel.Landing();panel.SetState(L.S("run."+pp.step),PlayingPanel.Word(pp.step),true);lastStep=pp.step;}
                    panel.Progress(pp.done,pp.total);
                }
            }));
            Task.Factory.StartNew(()=>S.Session.Play(profileId,progress,options)).ContinueWith(t=>Root.Dispatcher.BeginInvoke((Action)(()=>{
                if(clock!=null)clock.Stop();
                S.Busy=false;CloseOverlay();S.Reload();
                if(t.IsFaulted){
                    var optional=t.Exception.GetBaseException() as OptionalComponentException;
                    if(optional!=null){
                        string mode=optional.Retry.GraphicsFallback;
                        string detail=mode==null?"":"\n"+L.S(mode=="native"?"compat.native":"compat.sr");
                        Confirm(L.S("compat.title"),optional.Message+detail+"\n\n"+L.S("compat.once"),L.S("compat.continue"),
                                ()=>Play(profileId,optional.Retry),false);
                        return;
                    }
                    // A test flight that could not start still put the game back: show that too.
                    var last=S.Session.Last();
                    if(last!=null&&last.test&&last.profile==profileId&&last.phase=="CLOSED"){Info(L.S("err.title"),Explain(t.Exception.GetBaseException())+"\n\n"+L.S("fl.back"));return;}
                    Info(L.S("err.title"),Explain(t.Exception.GetBaseException()));return;
                }
                var res=t.Result;
                if(res.aside_kept!=null&&res.aside_kept.Count>0)Info(L.S("err.title"),L.S("run.kept")+"\n"+String.Join(", ",res.aside_kept)+"\n"+System.IO.Path.Combine(S.Env.DataRoot,"sessions",res.id,"aside"));
                if(res.test){FlightResult(res);return;}
                var save=S.LatestSave(S.Lib.Profile(profileId));
                int back=(res.aside==null?0:res.aside.Count)-(res.aside_kept==null?0:res.aside_kept.Count);
                Toast((save==null?L.S("run.done.none"):L.S("run.done")+": "+save.Short(L.Lang))+(back>0?" · "+L.F("run.aside",back):""));
            })));
        }        public void Recover(){
            if(S.Busy)return;S.Busy=true;
            Task.Factory.StartNew(()=>S.Session.Recover()).ContinueWith(t=>Root.Dispatcher.BeginInvoke((Action)(()=>{
                S.Busy=false;S.Reload();
                if(t.IsFaulted)Info(L.S("err.title"),Explain(t.Exception.GetBaseException()));
                else if(t.Result=="GAME_RUNNING")Info(L.S("rec.title"),L.S("rec.wait"));
                else Toast(L.S("rec.ok"));
            })));
        }
        // Developer mode: bring new project builds (with a Suite contract) into the library on start.
        public void CheckNewVersions(){
            if(!S.Dev||S.Settings==null||!S.Settings.first_run_done||S.Busy)return;
            List<string> fresh,stale;
            try{var have=new HashSet<string>(S.Versions.Select(v=>v.source_name));var importable=S.Imp.ImportableReleases();fresh=importable.Where(r=>!have.Contains(r)).ToList();
                // 2.4.1: versions an older Suite imported without fields of their contract are imported again (same id: profiles keep them).
                stale=S.Versions.Where(v=>importable.Contains(v.source_name)&&S.Imp.ContractStale(v)).Select(v=>v.source_name).Distinct().ToList();}catch(Exception){return;}
            if(fresh.Count+stale.Count==0)return;
            S.Busy=true;
            Func<List<string>,List<string>> import=list=>{var names=new List<string>();foreach(var r in list){try{var v=S.Imp.ImportRelease(r,null);names.Add(v.build+" "+v.Title(L.Lang));}catch(Exception){}}return names;};
            Task.Factory.StartNew(()=>Tuple.Create(import(fresh),import(stale)))
                .ContinueWith(t=>Root.Dispatcher.BeginInvoke((Action)(()=>{S.Busy=false;S.Reload();if(t.IsFaulted)return;
                    if(t.Result.Item1.Count>0)Toast(L.F("dev.newversion",String.Join(", ",t.Result.Item1)));
                    if(t.Result.Item2.Count>0)Toast(L.F("dev.refreshed",String.Join(", ",t.Result.Item2)));})));
        }        public void RunTask(string title,Func<string> work,Func<Exception,bool> handled=null){
            if(S.Busy)return;S.Busy=true;
            var panel=U.Col(12,U.Text(title,26,Theme.Text,true,FontWeights.SemiBold),new ProgressBar{Height=4,IsIndeterminate=true,Foreground=Theme.B(Theme.Sign),Background=Theme.B(Theme.Flap),BorderThickness=new Thickness(0),Width=420});
            panel.HorizontalAlignment=HorizontalAlignment.Center;panel.VerticalAlignment=VerticalAlignment.Center;
            ShowOverlay(panel,false);
            Task.Factory.StartNew(work).ContinueWith(t=>Root.Dispatcher.BeginInvoke((Action)(()=>{
                S.Busy=false;CloseOverlay();S.Reload();
                if(t.IsFaulted){
                    var error=t.Exception.GetBaseException();
                    if(handled==null||!handled(error))Info(L.S("err.title"),Explain(error));
                }else if(!String.IsNullOrEmpty(t.Result))Toast(t.Result);
            })));
        }
    }

    public static class SuiteApp {
        public const string Version="1.0";
        static void EnsureApp(){if(Application.Current==null){var app=new Application{ShutdownMode=ShutdownMode.OnMainWindowClose};app.Resources.MergedDictionaries.Add(Theme.Resources());}}
        public static Window CreateWindow(AppState state,Shell shell){
            var w=new Window{Title="GTAVXR",Width=1280,Height=860,MinWidth=1040,MinHeight=700,Background=Theme.B(Theme.Terminal),Content=shell.Root,
                WindowStartupLocation=WindowStartupLocation.CenterScreen,FontFamily=Theme.Body,UseLayoutRounding=true};
            TextOptions.SetTextFormattingMode(w,TextFormattingMode.Display);
            WindowChrome.SetWindowChrome(w,new WindowChrome{CaptionHeight=Shell.TitleHeight,ResizeBorderThickness=new Thickness(6),GlassFrameThickness=new Thickness(0),CornerRadius=new CornerRadius(0),UseAeroCaptionButtons=false});
            try{var ico=System.Drawing.Icon.ExtractAssociatedIcon(Program.SelfPath);w.Icon=System.Windows.Interop.Imaging.CreateBitmapSourceFromHIcon(ico.Handle,Int32Rect.Empty,System.Windows.Media.Imaging.BitmapSizeOptions.FromEmptyOptions());}catch(Exception){}
            w.Closing+=(o,e)=>{if(state.Busy){e.Cancel=true;shell.Toast(L.Lang=="en"?"Wait until the game closes and your progress is saved.":"Espera a que se cierre el juego y se guarde tu partida.");}};
            shell.Window=w;
            return w;
        }
        // Builds the real window object (never shown) to prove startup wiring headlessly.
        public static object Smoke(){
            EnsureApp();
            string root=System.IO.Path.Combine(System.IO.Path.GetTempPath(),"gtavxr-smoke-"+Ids.New());
            var env=Env.ForFixture(root);var state=new AppState(env){Demo=true};var shell=new Shell(state);
            var w=CreateWindow(state,shell);state.Reload();
            w.Measure(new Size(1280,860));
            var r=new{status="PASS_WINDOW_CONSTRUCTED_NOT_SHOWN",title=w.Title,chrome=WindowChrome.GetWindowChrome(w)!=null,icon=w.Icon!=null,first_run=!state.Settings.first_run_done,shown=false};
            try{System.IO.Directory.Delete(root,true);}catch(Exception){}
            return r;
        }
        public static int Start(string library=null){
            EnsureApp();var app=Application.Current;
            Env env;
            try{
                string path=library==null?Env.DefaultDataRoot():SafeFs.Root(library);
                string problem=library==null?null:PlatformCompatibility.SessionPathProblem(path);
                if(problem!=null)throw new InvalidDataException(problem);
                env=Env.Product(path);
            }
            catch(Exception e){MessageBox.Show(e.Message,"GTAVXR");return 1;}
            bool created;var single=new System.Threading.Mutex(true,"Local\\GTAVXR_Suite2",out created);
            if(!created){MessageBox.Show(L.Lang=="en"?"GTAVXR is already open.":"GTAVXR ya está abierto.","GTAVXR");return 0;}
            var state=new AppState(env);
            var shell=new Shell(state);
            var w=CreateWindow(state,shell);
            w.Loaded+=(o,e)=>{state.Reload();shell.CheckNewVersions();};
            var refresh=new DispatcherTimer{Interval=TimeSpan.FromSeconds(5)};
            refresh.Tick+=(o,e)=>{if(!state.Busy&&w.IsActive){bool vd=state.RuntimeName!=null&&XrRuntimes.Running(state.RuntimeName)==true;var lease=state.Env.DevHost?PlayLease.Holder(state.Env.DevRepo):null;
                if(vd!=state.StreamerRunning||((lease==null)!=(state.Lease==null)))state.Reload();}};
            refresh.Start();
            app.Run(w);GC.KeepAlive(single);return 0;
        }
    }
}
