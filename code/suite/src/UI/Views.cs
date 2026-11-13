using System;
using System.IO;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Data;
using System.Windows.Shapes;
using System.Collections.Generic;

namespace GtavxrSuite {
    // Mods: the library, as luggage tags. Each tag says what it is, whether the game can load it, and which profiles use it.
    public sealed class ModsView {
        readonly AppState s;readonly Shell shell;
        public ModsView(AppState st,Shell sh){s=st;shell=sh;}
        public UIElement Build(){
            if(s.Mods.Count<=30){
                var root=(StackPanel)Header();
                root.Margin=new Thickness(28,22,28,28);
                var tiles=new WrapPanel();
                foreach(var mod in s.Mods)tiles.Children.Add(Tag(mod));
                root.Children.Add(tiles);
                return new ScrollViewer{Content=root,VerticalScrollBarVisibility=ScrollBarVisibility.Auto};
            }
            var list=new ListBox{Margin=new Thickness(28,22,28,28),Padding=new Thickness(0),BorderThickness=new Thickness(0),
                                 Background=Brushes.Transparent,Focusable=false};
            ScrollViewer.SetCanContentScroll(list,true);
            ScrollViewer.SetHorizontalScrollBarVisibility(list,ScrollBarVisibility.Disabled);
            ScrollViewer.SetVerticalScrollBarVisibility(list,ScrollBarVisibility.Auto);
            VirtualizingPanel.SetIsVirtualizing(list,true);
            VirtualizingPanel.SetVirtualizationMode(list,VirtualizationMode.Recycling);
            VirtualizingPanel.SetScrollUnit(list,ScrollUnit.Pixel);
            list.ItemsPanel=new ItemsPanelTemplate(new FrameworkElementFactory(typeof(VirtualizingStackPanel)));
            var row=new FrameworkElementFactory(typeof(ModLibraryRow));
            row.SetBinding(ModLibraryRow.BuildProperty,new Binding());
            list.ItemTemplate=new DataTemplate{VisualTree=row};
            var container=new Style(typeof(ListBoxItem));
            container.Setters.Add(new Setter(Control.TemplateProperty,new ControlTemplate(typeof(ListBoxItem)){
                VisualTree=new FrameworkElementFactory(typeof(ContentPresenter))}));
            container.Setters.Add(new Setter(UIElement.FocusableProperty,false));
            container.Setters.Add(new Setter(Control.HorizontalContentAlignmentProperty,HorizontalAlignment.Stretch));
            list.ItemContainerStyle=container;
            int columns=0;
            Action<double> setRows=width=>{
                int count=Math.Max(1,(int)((width-SystemParameters.VerticalScrollBarWidth)/316));
                if(count==columns)return;
                columns=count;
                var factories=new List<Func<UIElement>>{Header};
                for(int start=0;start<s.Mods.Count;start+=columns){
                    var mods=s.Mods.Skip(start).Take(columns).ToArray();
                    factories.Add(()=>{
                        var tiles=new StackPanel{Orientation=Orientation.Horizontal};
                        foreach(var mod in mods)tiles.Children.Add(Tag(mod));
                        return tiles;
                    });
                }
                list.ItemsSource=factories;
            };
            setRows(960);
            list.SizeChanged+=(sender,args)=>setRows(args.NewSize.Width);
            return list;
        }
        StackPanel Header(){
            var root=new StackPanel();
            var head=new DockPanel{LastChildFill=false,Margin=new Thickness(0,0,0,16)};
            head.Children.Add(U.Col(6,new StickerText(L.S("modsv.title").ToUpperInvariant(),30),U.Text(L.S("modsv.hint"),14,Theme.Mist)));
            root.Children.Add(head);
            var drop=new Border{BorderBrush=Theme.B(Theme.C("#2E3A48")),BorderThickness=new Thickness(1.5),CornerRadius=new CornerRadius(14),Padding=new Thickness(20),Margin=new Thickness(0,0,0,18),
                Background=Theme.PanelBrush(),AllowDrop=true};
            var add=U.Btn(L.S("modsv.add"),"Primary",()=>Add(null));
            var dropText=U.Text(L.S("modsv.drop"),14,Theme.Mist);dropText.VerticalAlignment=VerticalAlignment.Center;
            drop.Child=U.Row(14,add,dropText);
            drop.Drop+=(o,e)=>{var files=e.Data.GetData(DataFormats.FileDrop) as string[];if(files!=null)foreach(var f in files)Add(f);};
            drop.DragOver+=(o,e)=>{e.Effects=e.Data.GetDataPresent(DataFormats.FileDrop)?DragDropEffects.Copy:DragDropEffects.None;e.Handled=true;};
            root.Children.Add(drop);
            return root;
        }
        UIElement Tag(ModManifest m){
            var users=s.Profiles.Where(p=>p.mods.Contains(m.id)||(m.kind=="mvga-native"&&s.Version(p.version)!=null&&s.Version(p.version).mvga=="required")).Select(p=>p.name).ToList();
            var works=s.Versions.Where(v=>m.WorksWith(v)).Select(v=>v.build).Distinct().ToList();
            var g=new Grid{Width=300,Margin=new Thickness(0,0,16,16)};
            g.Children.Add(new TagShape{Fill=Theme.B(Theme.Ticket)});
            g.Children.Add(new Ellipse{Width=12,Height=12,Fill=Theme.B(Theme.Terminal),Stroke=Theme.B(Theme.PaperLine),StrokeThickness=3,HorizontalAlignment=HorizontalAlignment.Left,Margin=new Thickness(10,0,0,0),VerticalAlignment=VerticalAlignment.Center});
            var c=new StackPanel{Margin=new Thickness(40,16,18,14)};
            string size=m.Bytes()>=1024*1024?Math.Round(m.Bytes()/1048576.0)+" MB":Math.Max(1,m.Bytes()/1024)+" KB";
            c.Children.Add(U.Label(L.Category(m.category)+" · "+size,Theme.InkSoft));
            var title=U.Text(m.name,20,Theme.Ink,true,FontWeights.SemiBold);title.Margin=new Thickness(0,2,0,2);c.Children.Add(title);
            if(!String.IsNullOrEmpty(m.version))c.Children.Add(U.Text(m.version,12,Theme.InkSoft));
            bool usable=m.loadable&&works.Count>0;
            var pills=new WrapPanel{Margin=new Thickness(0,8,0,6)};
            var pill=usable?U.Pill(L.F("modsv.ok",String.Join(", ",works)),Theme.C("#126B39"),Theme.C("#CDEFD9")):U.Pill(L.S("modsv.pending"),Theme.C("#9A3412"),Theme.C("#F7DCCF"));
            pills.Children.Add(pill);
            if(usable){var tp=m.tested_utc!=null?U.Pill(L.S("modsv.tested"),Theme.C("#0E4F86"),Theme.C("#D3EBFD")):U.Pill(L.S("modsv.untested"),Theme.C("#6B4E00"),Theme.C("#FFEDB0"));tp.Margin=new Thickness(6,0,0,0);pills.Children.Add(tp);}
            c.Children.Add(pills);
            c.Children.Add(U.Text(!m.loadable?L.S("modsv.pending.why"):users.Count>0?L.F("modsv.inuse",String.Join(", ",users)):L.S("modsv.unused"),13,Theme.Ink));
            var del=U.Btn(L.S("modsv.delete"),"Link",()=>{
                string from=users.Count>0?L.F("modsv.delete.from",String.Join(", ",users)):"";
                shell.Confirm(L.S("modsv.delete"),L.F("modsv.delete.q",m.name,from),L.S("modsv.delete"),()=>{s.Lib.DeleteMod(m.id);s.Reload();},true);
            });
            del.Foreground=Theme.B(Theme.C("#B4232A"));del.VerticalAlignment=VerticalAlignment.Center;
            var foot=new DockPanel{LastChildFill=false,Margin=new Thickness(0,12,0,0)};
            foot.Children.Add(del);
            if(usable){
                // Try it without risk: a test profile, the game put back afterwards.
                var tryIt=U.Btn(L.S("mods.try"),"Primary",()=>shell.TryMod(m));tryIt.Padding=new Thickness(14,5,14,5);
                DockPanel.SetDock(tryIt,Dock.Right);foot.Children.Add(tryIt);
            }
            c.Children.Add(foot);
            g.Children.Add(c);
            return g;
        }
        void Add(string path){
            var paths=new List<string>();
            if(path==null){
                var d=new Microsoft.Win32.OpenFileDialog{Filter="Mods (*.zip;*.oiv;*.rar;*.7z;*.asi;dlc.rpf)|*.zip;*.oiv;*.rar;*.7z;*.asi;*.rpf|*.*|*.*",Multiselect=true};
                if(d.ShowDialog()!=true)return;paths.AddRange(d.FileNames);
            }else paths.Add(path);
            shell.RunTask(L.S("modsv.add"),()=>{var names=paths.Select(x=>s.Imp.ImportArchive(x).name).ToList();return L.S("modsv.added")+": "+String.Join(", ",names);});
        }
    }

    public sealed class ModLibraryRow : ContentControl {
        public static readonly DependencyProperty BuildProperty=DependencyProperty.Register("Build",typeof(Func<UIElement>),
            typeof(ModLibraryRow),new PropertyMetadata(null,(owner,args)=>{
                var build=args.NewValue as Func<UIElement>;
                ((ModLibraryRow)owner).Content=build==null?null:build();
            }));
    }

    // Ajustes: game folder, headset, GTAVXR in the game (uninstall), language, developer mode.
    public sealed class SettingsView {
        readonly AppState s;readonly Shell shell;
        public SettingsView(AppState st,Shell sh){s=st;shell=sh;}
        Border Row(string label,UIElement value,UIElement action){
            var g=new Grid();g.ColumnDefinitions.Add(new ColumnDefinition{Width=new GridLength(230)});g.ColumnDefinitions.Add(new ColumnDefinition());g.ColumnDefinitions.Add(new ColumnDefinition{Width=GridLength.Auto});
            var l=U.Text(label,15,Theme.Mist);l.VerticalAlignment=VerticalAlignment.Center;g.Children.Add(l);
            var fe=value as FrameworkElement;if(fe!=null)fe.VerticalAlignment=VerticalAlignment.Center;Grid.SetColumn(value,1);g.Children.Add(value);
            if(action!=null){Grid.SetColumn(action,2);g.Children.Add(action);((FrameworkElement)action).VerticalAlignment=VerticalAlignment.Center;}
            return new Border{Child=g,Padding=new Thickness(18,14,18,14),BorderBrush=Theme.B(Theme.Line),BorderThickness=new Thickness(0,0,0,1)};
        }
        public UIElement Build(){
            var root=new StackPanel{Margin=new Thickness(28,22,28,28)};
            root.Children.Add(new StickerText(L.S("set.title").ToUpperInvariant(),30));
            var rows=new StackPanel();
            var lang=U.Row(8,U.Btn("Español",L.Lang=="es"?"Primary":"Btn",()=>SetLang("es")),U.Btn("English",L.Lang=="en"?"Primary":"Btn",()=>SetLang("en")));
            rows.Children.Add(Row(L.S("set.lang"),lang,null));
            var game=U.Text(s.Env.GameDir??L.S("fr.game.no"),14,Theme.Text);game.FontFamily=Theme.Mono;game.TextWrapping=TextWrapping.NoWrap;game.TextTrimming=TextTrimming.CharacterEllipsis;game.Margin=new Thickness(0,0,12,0);game.ToolTip=s.Env.GameDir;
            rows.Children.Add(Row(L.S("set.game"),game,U.Btn(L.S("set.change"),"Btn",ChooseGame)));
            rows.Children.Add(Row(L.S("set.headset"),RuntimeText(),s.Demo?null:U.Btn(L.S("set.change"),"Btn",ChooseRuntime)));
            {
                rows.Children.Add(Row(L.S("set.dlss"),DlssText(),U.Row(8,U.Btn(L.S("set.choose.folder"),"Btn",ChooseDlss),
                    String.IsNullOrEmpty(s.Settings.dlss_dir)?U.Btn(L.S("set.dlss.open"),"Ghost",()=>OpenFolder(s.Env.Dir("dlss"))):U.Btn(L.S("set.auto"),"Ghost",()=>{s.Settings.dlss_dir=null;s.Save();s.Reload();}))));
                var of=s.OfxrNow();
                var ofText=U.Text(!of.layer?L.S("set.ofxr.none"):of.tray!=null?L.F("set.ofxr.tray",of.tray):L.S("set.ofxr.notray"),14,of.layer?Theme.Text:Theme.Mist);ofText.TextWrapping=TextWrapping.Wrap;ofText.Margin=new Thickness(0,0,12,0);
                rows.Children.Add(Row(L.S("set.ofxr"),ofText,of.layer?U.Btn(L.S("set.choose.file"),"Btn",ChooseTray):null));
            }
            var inst=s.Installed;var iv=inst==null?null:s.Version(inst.version);
            string installed=inst==null?L.S("set.installed.none"):L.F("set.installed.v",iv==null?inst.version:iv.build+" "+iv.Title(L.Lang))+" · "+Math.Round(inst.files.Sum(f=>f.bytes)/1048576.0)+" MB";
            var instActions=inst==null?null:U.Row(8,U.Btn(L.S("set.repair"),"Btn",Repair),U.Btn(L.S("set.uninstall"),"Danger",Uninstall));
            rows.Children.Add(Row(L.S("set.installed"),U.Text(installed,15,Theme.Text),instActions));
            var dev=new CheckBox{Style=(Style)Application.Current.Resources["Switch"],IsChecked=s.Dev};
            dev.Click+=(o,e)=>{s.Settings.dev_mode=dev.IsChecked==true;s.Save();s.Reload();};
            rows.Children.Add(Row(L.S("set.dev"),U.Text(L.S("set.dev.hint"),15,Theme.Mist),dev));
            if(!s.Demo){
                bool appInst=SelfInstall.RunningInstalled();
                rows.Children.Add(Row(L.S("set.app"),U.Text(appInst?L.S("set.app.installed"):L.S("set.app.install.hint"),15,appInst?Theme.Mist:Theme.Text),
                    appInst?null:U.Btn(L.S("set.app.install"),"Btn",()=>shell.RunTask(L.S("set.app.install"),()=>{SelfInstall.Install();return L.S("set.app.done");}))));
                SelfInstall.ChannelBuild up=null;try{up=SelfInstall.Available(s.Env);}catch(Exception){}
                rows.Children.Add(Row(L.S("set.update"),U.Text(up==null?L.S("set.update.none"):L.F("set.update.avail",up.version),15,up==null?Theme.Mist:Theme.Text),
                    up==null?null:U.Btn(L.S("set.update.go"),"Primary",()=>{SelfInstall.ApplyUpdate(s.Env,up,true);if(shell.Window!=null)shell.Window.Close();})));
            }            rows.Children.Add(Row(L.S("set.about"),U.Text("GTAVXR Suite "+SuiteApp.Version,15,Theme.Mist),null));
            var box=new Border{Child=rows,BorderBrush=Theme.RimBrush(),BorderThickness=new Thickness(1.5),CornerRadius=new CornerRadius(14),Margin=new Thickness(0,16,0,0),Background=Theme.PanelBrush()};
            root.Children.Add(box);
            if(s.Dev)root.Children.Add(DevBox());
            return new ScrollViewer{Content=root,VerticalScrollBarVisibility=ScrollBarVisibility.Auto};
        }
        UIElement DevBox(){
            var c=new StackPanel();
            c.Children.Add(U.Label(L.S("dev.title"),Theme.Hazard));
            bool busy=s.Lease!=null;
            c.Children.Add(U.Row(8,U.Dot(busy?Theme.Sign:Theme.Go),U.Text(busy?L.F("dev.busy",s.Lease.agent=="claude"?"Claude":s.Lease.agent,s.Lease.purpose):L.S("dev.free"),15,Theme.Text)));
            var head=U.Col(4,U.Text(L.S("dev.import"),20,Theme.Text,true,FontWeights.SemiBold),U.Text(L.S("dev.import.hint"),13,Theme.Mist));head.Margin=new Thickness(0,16,0,8);c.Children.Add(head);
            List<string> rel;try{rel=s.Imp.ImportableReleases();}catch(Exception){rel=new List<string>();}
            var have=new HashSet<string>(s.Versions.Select(v=>v.source_name));
            var fresh=rel.Where(r=>!have.Contains(r)).ToList();
            if(fresh.Count==0)c.Children.Add(U.Text(L.S("dev.none"),14,Theme.Mist));
            foreach(var r in fresh){
                string name=r;
                var row=new DockPanel{LastChildFill=false,Margin=new Thickness(0,0,0,8)};
                var t=U.Text(name,14,Theme.Text);t.FontFamily=Theme.Mono;t.VerticalAlignment=VerticalAlignment.Center;row.Children.Add(t);
                var b=U.Btn(L.S("dev.add"),"Primary",()=>shell.RunTask(L.S("dev.add")+" "+name,()=>{var v=s.Imp.ImportRelease(name,null);return L.F("dev.added",v.build+" "+v.Title(L.Lang));}));
                DockPanel.SetDock(b,Dock.Right);row.Children.Add(b);c.Children.Add(row);
            }
            var box=new Border{Child=c,Padding=new Thickness(18),Margin=new Thickness(0,16,0,0),CornerRadius=new CornerRadius(14),BorderBrush=Theme.B(Theme.Hazard,0.45),BorderThickness=new Thickness(1.5),Background=Theme.B(Theme.C("#15130A"))};
            return box;
        }
        void SetLang(string l){s.Settings.language=l;s.Save();s.Reload();}
        // 2.4.0: which OpenXR runtime a Play uses. Automatico = Windows' active runtime at every Play.
        UIElement RuntimeText(){
            string text;Color c=Theme.Text;
            if(s.Demo)text=L.F("fr.headset.ok",s.RuntimeName??"Virtual Desktop");
            else{
                bool auto=String.IsNullOrEmpty(s.Settings.openxr_runtime)||s.Settings.openxr_runtime==XrRuntimes.Auto;
                var rt=s.Runtime;
                string name=rt!=null&&rt.error==null?rt.name:null;
                if(auto)text=name==null?L.S("set.xr.auto.none"):L.F("set.xr.auto",name);
                else text=name==null?L.F("set.xr.chosen.bad",s.Settings.openxr_runtime):L.F("set.xr.chosen",name);
                if(name==null)c=Theme.Hazard;
            }
            var tb=U.Text(text,15,c);tb.TextWrapping=TextWrapping.Wrap;tb.Margin=new Thickness(0,0,12,0);
            if(s.Runtime!=null&&s.Runtime.manifest!=null)tb.ToolTip=s.Runtime.manifest;
            return tb;
        }
        void ChooseRuntime(){
            string active=s.Env.XrActive!=null?s.Env.XrActive():s.Env.OpenXrManifest;
            var list=XrRuntimes.List(active,s.Env.XrAvailable!=null?s.Env.XrAvailable():new List<string>());
            var hint=U.Text(L.S("set.xr.hint"),14,Theme.Mist);hint.TextWrapping=TextWrapping.Wrap;
            var col=U.Col(8,U.Text(L.S("set.xr.title"),24,Theme.Text,true,FontWeights.SemiBold),hint);
            var act=list.FirstOrDefault(r=>r.active);
            var autoB=U.Btn(act!=null&&act.error==null?L.F("set.xr.auto",act.name):L.S("set.xr.auto.none"),s.Settings.openxr_runtime==XrRuntimes.Auto?"Primary":"Btn",()=>SetRuntime(XrRuntimes.Auto));
            autoB.HorizontalAlignment=HorizontalAlignment.Left;col.Children.Add(autoB);
            foreach(var r in list){
                string m=r.manifest;bool ok=r.error==null;
                var b=U.Btn((r.name??System.IO.Path.GetFileName(m))+(ok?"":"  ("+L.S("set.xr.broken")+")"),String.Equals(s.Settings.openxr_runtime,m,StringComparison.OrdinalIgnoreCase)?"Primary":"Btn",()=>SetRuntime(m));
                b.IsEnabled=ok;b.ToolTip=m;b.HorizontalAlignment=HorizontalAlignment.Left;col.Children.Add(b);
            }
            var file=U.Btn(L.S("set.xr.file"),"Ghost",()=>{
                var d=new Microsoft.Win32.OpenFileDialog{Filter="OpenXR runtime (*.json)|*.json",CheckFileExists=true};
                if(d.ShowDialog()!=true)return;
                var r=XrRuntimes.Inspect(d.FileName,active);
                if(r.error!=null){shell.Info(L.S("set.xr.title"),Shell.Explain(new InvalidDataException("XR_RUNTIME:"+r.error)));return;}
                SetRuntime(d.FileName);
            });
            var cancel=U.Btn(L.S("cancel"),"Ghost",shell.CloseOverlay);
            var bottom=U.Row(8,file,cancel);bottom.Margin=new Thickness(0,8,0,0);col.Children.Add(bottom);
            shell.ShowOverlay(shell.Card(col));
        }
        void SetRuntime(string choice){shell.CloseOverlay();s.Settings.openxr_runtime=choice;s.Save();s.Reload();}

        // 2.4.0: the DLSS folder (the player's own NVIDIA files; GTAVXR never ships them).
        UIElement DlssText(){
            var tb=U.Text("",14,Theme.Text);tb.TextWrapping=TextWrapping.Wrap;tb.Margin=new Thickness(0,0,12,0);
            s.ShowDlss((f,checking)=>{
                string text;Color c=Theme.Text;
                if(checking){text=L.S("gfx.dlss.check");c=Theme.Mist;}
                else if(f==null){text=L.S("set.dlss.none");c=Theme.Mist;}
                else if(!Dlss.Usable(f)){text=L.F("set.dlss.bad",f.dir);c=Theme.Hazard;}
                else text=L.F(f.source=="chosen"?"set.dlss.chosen":f.source=="suite"?"set.dlss.suite":"set.dlss.found",f.dir)+"\n"+
                    (Dlss.NeuralUsable(f)?L.F("set.dlss.models.nr",f.sr.version??"?",f.nr.version??"?"):L.F("set.dlss.models.sr",f.sr.version??"?"));
                tb.Text=text;tb.Foreground=Theme.B(c);
            });
            return tb;
        }
        void ChooseDlss(){
            using(var d=new System.Windows.Forms.FolderBrowserDialog{Description="DLSS (nvngx_dlss.dll)",ShowNewFolderButton=false}){
                if(d.ShowDialog()!=System.Windows.Forms.DialogResult.OK)return;
                var f=Dlss.InspectFolder(d.SelectedPath,"chosen");
                if(!Dlss.Usable(f)){shell.Info(L.S("set.dlss"),L.S("set.dlss.invalid")+(f!=null&&f.sr!=null&&f.sr.error!=null?"\n"+f.sr.error:""));return;}
                s.Settings.dlss_dir=d.SelectedPath;s.Save();s.Reload();
            }
        }
        void ChooseTray(){
            var d=new Microsoft.Win32.OpenFileDialog{Filter="OFXRBridgeTray.exe|OFXRBridgeTray.exe",CheckFileExists=true};
            if(d.ShowDialog()!=true)return;
            s.Settings.ofxr_tray=d.FileName;s.Save();s.Reload();
        }
        public static void OpenFolder(string dir){try{System.Diagnostics.Process.Start("explorer.exe","\""+dir+"\"");}catch(Exception){}}
        void ChooseGame(){
            using(var d=new System.Windows.Forms.FolderBrowserDialog{Description="GTA V",ShowNewFolderButton=false}){
                if(d.ShowDialog()!=System.Windows.Forms.DialogResult.OK)return;
                if(!File.Exists(System.IO.Path.Combine(d.SelectedPath,"GTA5.exe"))){shell.Info(L.S("set.game"),L.S("fr.game.no"));return;}
                if(s.Installed!=null){shell.Info(L.S("set.game"),L.S("set.uninstall.q"));return;}
                s.Settings.game_dir=d.SelectedPath;s.Save();s.Env.GameDir=d.SelectedPath;s.Reload();
            }
        }
        void Repair(){shell.RunTask(L.S("set.repair"),()=>{if(s.Session.GameRunning())throw new InvalidOperationException("GAME_RUNNING");var bad=new GameLayer(s.Env).Repair();if(bad.Count==0)return L.S("set.repair.ok");
            return L.F("set.repair.bad",bad.Count);});}
        void Uninstall(){shell.Confirm(L.S("set.uninstall"),L.S("set.uninstall.q"),L.S("set.uninstall"),()=>shell.RunTask(L.S("set.uninstall"),()=>{
            if(s.Session.GameRunning())throw new InvalidOperationException("GAME_RUNNING");new GameLayer(s.Env).Uninstall(null);return L.S("set.uninstalled");}),true);}
    }

    // First start: the check-in desk. Finds the game, headset and space, then brings in versions and profiles.
    public sealed class FirstRunView {
        readonly AppState s;readonly Shell shell;
        public FirstRunView(AppState st,Shell sh){s=st;shell=sh;}
        public UIElement Build(){
            string game=s.Env.GameDir;
            if(game==null&&!s.Demo){var c=Detect.GameCandidates();if(c.Count>0)game=c[0];}
            string xr=s.Env.OpenXrManifest??(s.Demo?null:Detect.OpenXrRuntime());
            string repo=s.Env.DevRepo??(s.Demo?null:Detect.DevRepo());
            // Free space on C: as information; only a nearly full disk (under 2 GiB) is flagged.
            long free=-1;try{free=new DriveInfo("C").AvailableFreeSpace;}catch(Exception){}
            bool space=free<0||free>2L*1024*1024*1024;
            if(s.Demo){game=game??@"C:\Program Files (x86)\Steam\steamapps\common\Grand Theft Auto V";xr=xr??@"C:\Program Files\Virtual Desktop Streamer\OpenXR\virtualdesktop-openxr.json";}
            var left=U.Col(12,U.Label("Check-in",Theme.Sign),new StickerText(L.S("fr.title").ToUpperInvariant(),38),U.Text(L.S("fr.body"),16,Theme.Mist));
            if(Assets.Logo!=null){var logo=U.Img(Assets.Logo,300);logo.HorizontalAlignment=HorizontalAlignment.Left;logo.Margin=new Thickness(-8,0,0,10);left.Children.Insert(0,logo);}
            var steps=new StackPanel();
            steps.Children.Add(Step(1,L.S("fr.game"),game!=null?L.F("fr.game.ok",game):L.S("fr.game.no"),game!=null));
            steps.Children.Add(Step(2,L.S("fr.headset"),xr!=null?L.F("fr.headset.ok",Detect.RuntimeName(xr)):L.S("fr.headset.no"),xr!=null));
            steps.Children.Add(Step(3,L.S("fr.space"),!space?L.S("fr.space.no"):free<0?L.S("fr.space.unknown"):L.F("fr.space.ok",free/(1024*1024*1024)),space));
            steps.Children.Add(Step(4,L.S("fr.profiles"),repo!=null?L.S("fr.profiles.dev"):L.S("fr.profiles.plain"),null));
            var go=U.Btn(L.S("fr.go"),"Primary",()=>Run(game,xr,repo));go.IsEnabled=game!=null&&(xr!=null||EmbeddedRelease.Available)&&space;go.HorizontalAlignment=HorizontalAlignment.Left;go.Margin=new Thickness(0,18,0,0);go.Padding=new Thickness(28,10,28,10);go.FontSize=18;
            var choose=U.Btn(L.S("fr.choose"),"Btn",()=>{
                using(var d=new System.Windows.Forms.FolderBrowserDialog{Description="GTA V",ShowNewFolderButton=false}){
                    if(d.ShowDialog()==System.Windows.Forms.DialogResult.OK&&File.Exists(System.IO.Path.Combine(d.SelectedPath,"GTA5.exe"))){s.Env.GameDir=d.SelectedPath;shell.Refresh();}
                }});
            left.Children.Add(game==null?U.Row(10,choose,go):(UIElement)go);
            var g=new Grid{Margin=new Thickness(48,34,48,40)};
            g.ColumnDefinitions.Add(new ColumnDefinition{Width=new GridLength(1,GridUnitType.Star)});g.ColumnDefinitions.Add(new ColumnDefinition{Width=new GridLength(40)});g.ColumnDefinitions.Add(new ColumnDefinition{Width=new GridLength(1.2,GridUnitType.Star)});
            g.Children.Add(left);var box=new Border{Child=steps,BorderBrush=Theme.RimBrush(),BorderThickness=new Thickness(1.5),CornerRadius=new CornerRadius(14),Background=Theme.PanelBrush(),VerticalAlignment=VerticalAlignment.Center};
            Grid.SetColumn(box,2);g.Children.Add(box);
            return g;
        }
        UIElement Step(int n,string title,string body,bool? ok){
            var mark=new Border{Width=26,Height=26,CornerRadius=new CornerRadius(13),Background=Theme.B(ok==true?Theme.C("#16301A"):ok==false?Theme.C("#3A1C1E"):Theme.Sign),VerticalAlignment=VerticalAlignment.Top};
            mark.Child=new TextBlock{Text=ok==true?"✓":ok==false?"!":n.ToString(),Foreground=Theme.B(ok==true?Theme.Go:ok==false?Theme.Stop:Theme.Ink),FontFamily=Theme.Sign_,FontWeight=FontWeights.Bold,HorizontalAlignment=HorizontalAlignment.Center,VerticalAlignment=VerticalAlignment.Center};
            var g=new Grid();g.ColumnDefinitions.Add(new ColumnDefinition{Width=new GridLength(42)});g.ColumnDefinitions.Add(new ColumnDefinition());
            g.Children.Add(mark);var c=U.Col(3,U.Text(title,17,Theme.Text,true,FontWeights.SemiBold),U.Text(body,13,Theme.Mist));Grid.SetColumn(c,1);g.Children.Add(c);
            return new Border{Child=g,Padding=new Thickness(16,14,16,14),BorderBrush=Theme.B(Theme.Line),BorderThickness=new Thickness(0,0,0,n<4?1:0)};
        }
        void Run(string game,string xr,string repo){
            shell.RunTask(L.S("fr.working"),()=>{
                var st=s.Env.LoadSettings();st.game_dir=game;st.openxr_manifest=xr;st.user_data_dir=s.Env.UserDataDir??Detect.UserData();st.dev_repo=repo;
                s.Env.GameDir=game;s.Env.OpenXrManifest=xr;s.Env.DevRepo=repo;s.Env.UserDataDir=st.user_data_dir;
                FirstRun.Bring(s.Env,repo!=null);
                st.dev_mode=repo!=null;st.first_run_done=true;s.Env.SaveSettings(st);
                return L.S("fr.done");
            });
        }
    }

    // What the check-in imports on the development PC: the stable version, every version with a Suite contract,
    // the current VR profile and the 155 test save. All copies; the launcher's own state is never changed.
    public static class FirstRun {
        public static void Bring(Env env,bool dev){
            var imp=new Importers(env);var lib=new Library(env);
            if(!dev)return;
            string stable=imp.CurrentStableName();
            foreach(string r in imp.ImportableReleases())if(!lib.Versions().Any(v=>v.source_name==r))imp.ImportRelease(r,null);
            var vs=lib.Versions();
            var stableV=vs.FirstOrDefault(v=>v.source_name==stable);
            var test=vs.Where(v=>v.channel=="test").OrderByDescending(v=>Library.BuildNumber(v.build)).FirstOrDefault();
            if(lib.Profiles().Count==0){
                if(stableV!=null){var p=imp.ImportAcceptedProfile("Historia VR",stableV.id);}
                if(test!=null){
                    string seedRelease=Directory.Exists(System.IO.Path.Combine(imp.ReleasesRoot,"native-xr-build155-mvga265-v1"))?"native-xr-build155-mvga265-v1":test.source_name;
                    var p=imp.ImportCandidateProfile(seedRelease,"Prueba "+test.build,test.id);
                    var mv=lib.Mods().FirstOrDefault(m=>m.kind=="mvga-native");
                    if(mv!=null){p=lib.Profile(p.id);p.mods.Add(mv.id);p.color=1;lib.SaveProfile(p);}
                    // Test versions are tried in a test profile: every Play puts the game back afterwards.
                    lib.SetTest(p.id,true);
                }
            }
        }
    }
}
