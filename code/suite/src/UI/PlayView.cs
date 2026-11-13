using System;
using System.IO;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Media.Effects;
using System.Windows.Shapes;
using System.Collections.Generic;
using Path=System.Windows.Shapes.Path;

namespace GtavxrSuite {
    // Jugar: the departures board (profiles) and the boarding pass of the chosen profile, with Play on its stub.
    // The profile is edited right below the pass: version, mods, save, VR settings, name.
    // Test profiles wear hazard tape: every Play is a test flight and the game is put back when it lands.
    public sealed class PlayView {
        readonly AppState s;readonly Shell shell;
        public static bool EditorOpen=true;public static string EditorTab="mods";
        readonly Dictionary<string,List<FlapText>> rowFlaps=new Dictionary<string,List<FlapText>>();
        public PlayView(AppState state,Shell sh){s=state;shell=sh;}

        public UIElement Build(){
            var root=new StackPanel{Margin=new Thickness(28,22,28,28)};
            if(s.Recovery)root.Children.Add(RecoveryCard());
            root.Children.Add(Board());
            var p=s.Selected;
            if(EmbeddedRelease.Available&&!s.Demo){
                var install=U.Btn(L.S("dist.install"),"Primary",()=>shell.InstallEmbedded(p==null?null:p.id));
                install.IsEnabled=s.GameFound&&!s.Busy&&!s.Recovery;
                install.HorizontalAlignment=HorizontalAlignment.Left;
                root.Children.Add(install);
            }
            if(p==null)root.Children.Add(Empty());
            else{
                var pass=(FrameworkElement)Pass(p);pass.Margin=new Thickness(0,20,0,0);root.Children.Add(pass);
                if(EditorOpen){var ed=(FrameworkElement)Editor(p);ed.Margin=new Thickness(0,16,0,0);root.Children.Add(ed);}
            }
            return new ScrollViewer{Content=root,VerticalScrollBarVisibility=ScrollBarVisibility.Auto};
        }
        static Border Panel(UIElement child,double pad){
            return new Border{Child=child,Background=Theme.PanelBrush(),BorderBrush=Theme.RimBrush(),BorderThickness=new Thickness(1.5),CornerRadius=new CornerRadius(16),Padding=new Thickness(pad)};
        }

        // ---------- recovery ----------
        UIElement RecoveryCard(){
            var body=U.Col(6,U.Text(L.S("rec.title"),20,Theme.Text,true,FontWeights.SemiBold),U.Text(L.S("rec.body"),14,Theme.Mist));
            var fix=U.Btn(L.S("rec.fix"),"Primary",()=>shell.Recover());fix.VerticalAlignment=VerticalAlignment.Center;
            var g=new Grid();g.ColumnDefinitions.Add(new ColumnDefinition());g.ColumnDefinitions.Add(new ColumnDefinition{Width=GridLength.Auto});
            g.Children.Add(body);Grid.SetColumn(fix,1);g.Children.Add(fix);
            var b=U.Box(g,Theme.C("#221B0C"),Theme.C("#6B5A1A"),12,16);b.Margin=new Thickness(0,0,0,16);return b;
        }

        // ---------- board ----------
        static readonly int[] Cells={14,13,8,16,10};
        UIElement Board(){
            var head=new DockPanel{LastChildFill=false,Margin=new Thickness(2,0,2,12)};
            var plane=new Path{Data=Geometry.Parse(U.PlanePath),Fill=Theme.B(Theme.Sign),Stretch=Stretch.Uniform,Width=24,Height=24,VerticalAlignment=VerticalAlignment.Center,Margin=new Thickness(0,0,10,0)};
            head.Children.Add(plane);
            var title=new StickerText(L.S("board.title"),26){VerticalAlignment=VerticalAlignment.Center,Margin=new Thickness(0,0,16,0)};
            head.Children.Add(title);var hint=U.Text(L.S("board.hint"),13,Theme.Mist);hint.VerticalAlignment=VerticalAlignment.Center;head.Children.Add(hint);
            var chips=new StackPanel{Orientation=Orientation.Horizontal,VerticalAlignment=VerticalAlignment.Center};
            // 2.4.0: the chip of the runtime a Play would use (green = its process is up; grey = a runtime without an indicator)
            if(s.Demo)chips.Children.Add(U.Chip(s.RuntimeName??"Virtual Desktop",Theme.Go));
            else if(s.RuntimeName==null)chips.Children.Add(U.Chip(L.S("chip.xr.none"),Theme.C("#E0A030")));
            else if(s.RuntimeUp==true)chips.Children.Add(U.Chip(s.RuntimeName,Theme.Go));
            else if(s.RuntimeUp==null)chips.Children.Add(U.Chip(s.RuntimeName,Theme.Mist));
            else chips.Children.Add(U.Chip(L.F("chip.xr.off",s.RuntimeName),Theme.C("#E0A030")));
            chips.Children.Add(s.GameFound||s.Demo?U.Chip(L.S("chip.game"),Theme.Go):U.Chip(L.S("chip.game.missing"),Theme.Stop));
            DockPanel.SetDock(chips,Dock.Right);head.Children.Add(chips);

            var grid=new Grid();
            double[] stars={1.45,1.3,0.85,1.6,1.0};
            foreach(double st in stars)grid.ColumnDefinitions.Add(new ColumnDefinition{Width=new GridLength(st,GridUnitType.Star)});
            string[] heads={"col.profile","col.version","col.mods","col.save","col.state"};
            grid.RowDefinitions.Add(new RowDefinition{Height=GridLength.Auto});
            for(int c=0;c<5;c++){var h=U.Label(L.S(heads[c]),Theme.Mist);h.Margin=new Thickness(c==0?16:8,0,0,6);Grid.SetColumn(h,c);grid.Children.Add(h);}
            int r=1;
            foreach(var p in s.Visible()){
                grid.RowDefinitions.Add(new RowDefinition{Height=GridLength.Auto});
                var v=s.Version(p.version);var save=s.LatestSave(p);
                string state=v==null?L.S("state.missing"):(p.test||v.channel=="test")?L.S("state.test"):L.S("state.ready");
                Color stateInk=v==null?Theme.Stop:(p.test||v.channel=="test")?Theme.Hazard:Theme.Go;
                string[] vals={p.name,v==null?"—":v.build+" "+v.Title(L.Lang),s.ModsShort(p),save==null?L.S("save.new"):save.mission+" "+Math.Floor(save.percent)+"%",state};
                bool on=s.Selected!=null&&s.Selected.id==p.id;
                var bg=new Border{Background=Theme.B(on?Theme.Selected:Theme.Board),BorderBrush=Theme.B(on?Theme.Sign:Theme.FlapCut),BorderThickness=new Thickness(on?3:0,1,0,0)};
                Grid.SetRow(bg,r);Grid.SetColumnSpan(bg,5);grid.Children.Add(bg);
                // Test profiles: a strip of tape at the start of the row.
                if(p.test){var tape=new Border{Width=7,Background=Theme.HazardBrush(9),HorizontalAlignment=HorizontalAlignment.Left,Margin=new Thickness(on?3:0,1,0,0)};Grid.SetRow(tape,r);grid.Children.Add(tape);}
                var flaps=new List<FlapText>();
                for(int c=0;c<5;c++){
                    var f=new FlapText(vals[c],Cells[c],c==4?stateInk:Theme.FlapInk,16){Margin=new Thickness(c==0?16:8,6,4,6),IsHitTestVisible=false};
                    Grid.SetRow(f,r);Grid.SetColumn(f,c);grid.Children.Add(f);flaps.Add(f);
                }
                rowFlaps[p.id]=flaps;
                var hit=new Button{Style=(Style)Application.Current.Resources["Plain"],Background=Brushes.Transparent,ToolTip=p.name};
                string id=p.id;hit.Click+=(o,e)=>{if(s.Busy)return;s.Select(id);};
                hit.MouseEnter+=(o,e)=>{if(!(s.Selected!=null&&s.Selected.id==id))bg.Background=Theme.B(Theme.Hover);};
                hit.MouseLeave+=(o,e)=>{if(!(s.Selected!=null&&s.Selected.id==id))bg.Background=Theme.B(Theme.Board);};
                Grid.SetRow(hit,r);Grid.SetColumnSpan(hit,5);grid.Children.Add(hit);
                r++;
            }
            var box=new StackPanel();box.Children.Add(head);box.Children.Add(grid);
            return Panel(box,16);
        }
        public void FlipSelected(){List<FlapText> f;if(s.Selected!=null&&rowFlaps.TryGetValue(s.Selected.id,out f))foreach(var x in f)x.Flip();}

        UIElement Empty(){
            var c=U.Col(10,new StickerText(L.S("empty.title").ToUpperInvariant(),30),U.Text(L.S("empty.body"),14,Theme.Mist),U.Btn(L.S("pass.new"),"Primary",()=>shell.NewProfile()));
            ((FrameworkElement)c.Children[2]).HorizontalAlignment=HorizontalAlignment.Left;
            var b=Panel(c,28);b.Margin=new Thickness(0,20,0,0);return b;
        }

        // ---------- boarding pass ----------
        UIElement Pass(Profile p){
            var v=s.Version(p.version);var mods=s.ActiveMods(p);var save=s.LatestSave(p);
            Color band=Theme.Band(p.color);
            var outer=new Grid();
            outer.ColumnDefinitions.Add(new ColumnDefinition());outer.ColumnDefinitions.Add(new ColumnDefinition{Width=new GridLength(256)});
            // main: white stock, profile color band, the logo as a faint watermark
            var main=new Grid{Background=Theme.B(Theme.Ticket),ClipToBounds=true};
            if(Assets.Logo!=null){var wm=U.Img(Assets.Logo,190);wm.Opacity=0.07;wm.HorizontalAlignment=HorizontalAlignment.Right;wm.VerticalAlignment=VerticalAlignment.Bottom;wm.Margin=new Thickness(0,0,-24,-34);main.Children.Add(wm);}
            main.Children.Add(new Border{Width=10,Background=Theme.B(band),HorizontalAlignment=HorizontalAlignment.Left});
            var mc=new StackPanel{Margin=new Thickness(32,0,24,16)};
            if(p.test){var tape=U.Tape(L.S("tape.flight"),24);tape.Margin=new Thickness(-22,0,-24,4);mc.Children.Add(tape);}
            else mc.Children.Add(new Border{Height=18});
            var top=new DockPanel{LastChildFill=false};
            var routeIcon=new Path{Data=Geometry.Parse(U.PlanePath),Fill=Theme.B(Theme.InkSoft),Stretch=Stretch.Uniform,Width=15,Height=15,VerticalAlignment=VerticalAlignment.Center,Margin=new Thickness(0,0,8,0)};
            var route=U.Text(L.S("pass.route"),14,Theme.InkSoft,true,FontWeights.Bold);route.VerticalAlignment=VerticalAlignment.Center;
            top.Children.Add(routeIcon);top.Children.Add(route);
            var nb=U.Btn(L.S("pass.new"),"OnTicket",()=>shell.NewProfile());DockPanel.SetDock(nb,Dock.Right);top.Children.Add(nb);
            mc.Children.Add(top);
            var name=new StickerText(p.name,44,null,0,false){Margin=new Thickness(-3,6,0,8),HorizontalAlignment=HorizontalAlignment.Left};
            mc.Children.Add(name);
            mc.Children.Add(new Rectangle{Height=1.5,Stroke=Theme.B(Theme.PaperLine),StrokeThickness=1.5,StrokeDashArray=new DoubleCollection{4,3},Margin=new Thickness(0,0,0,12)});
            var fields=new Grid();for(int i=0;i<4;i++)fields.ColumnDefinitions.Add(new ColumnDefinition());
            string modsMain=mods.Count==0?L.S("f.nomods"):mods.Count==1?(mods[0].kind=="mvga-native"?"MVGA 2.65":mods[0].name):L.F("f.modsn",mods.Count);
            string modsSub=mods.Count==0?(v!=null&&v.mvga=="switch"?"MVGA: off":""):String.Join(" · ",mods.Select(m=>m.kind=="mvga-native"?"Make Visuals Great Again":m.name));
            Field(fields,0,L.S("f.version"),v==null?"—":v.build+" "+v.Title(L.Lang),v==null?L.S("state.missing"):v.channel=="test"?L.S("f.testv"):L.S("f.stable"));
            Field(fields,1,L.S("f.mods"),modsMain,modsSub);
            Field(fields,2,L.S("f.save"),save==null?L.S("save.none"):save.Short(L.Lang),save==null?"":L.Ago(save.written_utc.ToString("o")));
            Field(fields,3,L.S("f.vr"),p.has_vr_settings?L.S("f.vr.title"):L.S("f.vr.default"),L.S("f.vr.own"));
            mc.Children.Add(fields);
            var foot=new DockPanel{LastChildFill=false,Margin=new Thickness(0,16,0,0)};
            foot.Children.Add(new Barcode(p.id,Theme.Ink));
            var edit=U.Btn(EditorOpen?L.S("pass.close"):L.S("pass.edit"),"Link",()=>{EditorOpen=!EditorOpen;shell.Refresh();});
            edit.VerticalAlignment=VerticalAlignment.Bottom;DockPanel.SetDock(edit,Dock.Right);foot.Children.Add(edit);
            mc.Children.Add(foot);
            main.Children.Add(mc);
            outer.Children.Add(main);
            // stub: the window seat. Los Santos through an airplane window, the gate, and Play.
            var stub=new Grid{Background=new LinearGradientBrush(Theme.C("#141C27"),Theme.C("#090D13"),90)};
            var sc=new DockPanel{Margin=new Thickness(24,16,22,18),LastChildFill=false};
            var gateLabel=U.Label(L.S("stub.gate"),Theme.C("#9AA6B3"));DockPanel.SetDock(gateLabel,Dock.Top);sc.Children.Add(gateLabel);
            var view=new Grid{Margin=new Thickness(0,6,0,0),HorizontalAlignment=HorizontalAlignment.Left};
            var gatePic=Assets.Gate(p.image);if(gatePic!=null)view.Children.Add(U.Porthole(gatePic,206,128));
            var gateNo=new StickerText(v==null?"—":v.build,50){HorizontalAlignment=HorizontalAlignment.Left,VerticalAlignment=VerticalAlignment.Bottom,Margin=new Thickness(10,0,0,2)};
            view.Children.Add(gateNo);
            DockPanel.SetDock(view,Dock.Top);sc.Children.Add(view);
            var vt=U.Text((v==null?"":(v.Title(L.Lang)+" · "+(v.channel=="test"?L.S("f.testv"):L.S("f.stable")))).ToUpperInvariant(),12,Theme.C("#9AA6B3"),true);
            vt.Margin=new Thickness(0,8,0,0);DockPanel.SetDock(vt,Dock.Top);sc.Children.Add(vt);
            string readyText=p.test?L.S("stub.flight"):s.RuntimeName==null?L.S("chip.xr.none"):
                s.RuntimeUp==true||s.Demo?L.F("stub.xr.signal",s.RuntimeName):
                s.RuntimeUp==false?L.F("chip.xr.off",s.RuntimeName):L.F("stub.xr.selected",s.RuntimeName);
            Color readyDot=p.test?Theme.Hazard:s.Demo||s.RuntimeUp==true?Theme.Go:
                s.RuntimeName!=null&&s.RuntimeUp==null?Theme.Mist:Theme.C("#E0A030");
            var readyTxt=U.Text(readyText,12,Theme.C("#B9C3CE"));readyTxt.MaxWidth=180;
            var ready=U.Row(8,U.Dot(readyDot),readyTxt);ready.Margin=new Thickness(0,10,0,0);
            DockPanel.SetDock(ready,Dock.Bottom);sc.Children.Add(ready);
            var go=new Button{Style=(Style)Application.Current.Resources["Go"],IsEnabled=v!=null&&!s.Busy,Margin=new Thickness(0,14,0,0)};
            var goContent=U.Row(12,new Path{Data=Geometry.Parse("M0,0 L13,8 L0,16 Z"),Fill=Brushes.White,Stroke=Brushes.Black,StrokeThickness=1.5,VerticalAlignment=VerticalAlignment.Center},
                new StickerText(L.S(p.test?"stub.test":"stub.play"),26){VerticalAlignment=VerticalAlignment.Center});
            go.Content=goContent;go.Click+=(o,e)=>shell.Play(p.id);
            DockPanel.SetDock(go,Dock.Bottom);sc.Children.Add(go);
            stub.Children.Add(sc);
            // perforation
            stub.Children.Add(new Line{X1=0,Y1=0,X2=0,Y2=1,Stretch=Stretch.Fill,Stroke=Theme.B(Theme.Ticket),StrokeThickness=2,StrokeDashArray=new DoubleCollection{3,3},HorizontalAlignment=HorizontalAlignment.Left,Margin=new Thickness(0,14,0,14)});
            Grid.SetColumn(stub,1);outer.Children.Add(stub);
            // The whole pass: heavy black outline and a hard drop, like the logo's letters.
            var frame=new Border{Child=outer,BorderBrush=Brushes.Black,BorderThickness=new Thickness(3),CornerRadius=new CornerRadius(15),Background=Brushes.Black};
            frame.Effect=new DropShadowEffect{Color=Colors.Black,ShadowDepth=6,Direction=270,BlurRadius=0,Opacity=0.85};
            var clip=new Grid();clip.Children.Add(frame);
            // round the inner corners
            outer.SizeChanged+=(o,e)=>{outer.Clip=new RectangleGeometry(new Rect(0,0,outer.ActualWidth,outer.ActualHeight),12,12);};
            // notches where the stub tears off
            var holes=new Grid();holes.ColumnDefinitions.Add(new ColumnDefinition());holes.ColumnDefinitions.Add(new ColumnDefinition{Width=new GridLength(256)});
            foreach(var va in new[]{VerticalAlignment.Top,VerticalAlignment.Bottom}){
                var n=new Ellipse{Width=20,Height=20,Fill=Brushes.Black,VerticalAlignment=va,HorizontalAlignment=HorizontalAlignment.Left,Margin=new Thickness(-7,va==VerticalAlignment.Top?-7:0,0,va==VerticalAlignment.Bottom?-7:0),IsHitTestVisible=false};
                Grid.SetColumn(n,1);holes.Children.Add(n);
            }
            holes.Margin=new Thickness(3);clip.Children.Add(holes);
            return clip;
        }
        static void Field(Grid g,int col,string label,string value,string sub){
            var c=U.Col(2,U.Label(label,Theme.InkSoft),U.Text(value,19,Theme.Ink,true,FontWeights.SemiBold),U.Text(sub,12,Theme.InkSoft));
            c.Margin=new Thickness(0,0,14,0);Grid.SetColumn(c,col);g.Children.Add(c);
        }

        // ---------- editor ----------
        UIElement Editor(Profile p){
            var tabs=new StackPanel{Orientation=Orientation.Horizontal};
            string[] keys={"version","mods","save","vr","profile"};
            foreach(string k in keys){
                var rb=new RadioButton{Content=L.S("tab."+k),Style=(Style)Application.Current.Resources["Tab"],IsChecked=EditorTab==k,GroupName="edtab"};
                string kk=k;rb.Checked+=(o,e)=>{EditorTab=kk;shell.Refresh();};tabs.Children.Add(rb);
            }
            var tabBar=new Border{Child=tabs,BorderBrush=Theme.B(Theme.Line),BorderThickness=new Thickness(0,0,0,1),Padding=new Thickness(6,0,6,0)};
            UIElement body;
            switch(EditorTab){
                case "version":body=VersionTab(p);break;
                case "save":body=SaveTab(p);break;
                case "vr":body=VrTab(p);break;
                case "profile":body=ProfileTab(p);break;
                default:body=ModsTab(p);break;
            }
            var panel=new StackPanel();panel.Children.Add(tabBar);
            var inner=new Border{Child=body,Padding=new Thickness(18)};panel.Children.Add(inner);
            return Panel(panel,0);
        }
        Border Option(UIElement left,UIElement right,bool selected,Action click,bool enabled=true){
            var g=new Grid();g.ColumnDefinitions.Add(new ColumnDefinition());g.ColumnDefinitions.Add(new ColumnDefinition{Width=GridLength.Auto});
            g.Children.Add(left);if(right!=null){Grid.SetColumn(right,1);g.Children.Add(right);((FrameworkElement)right).VerticalAlignment=VerticalAlignment.Center;}
            var b=new Border{Child=g,Background=Theme.B(selected?Theme.Selected:Theme.Terminal),BorderBrush=Theme.B(selected?Theme.Sign:Theme.Line),BorderThickness=new Thickness(1.5),CornerRadius=new CornerRadius(9),
                Padding=new Thickness(14,10,14,10),Margin=new Thickness(0,0,0,8),Opacity=enabled?1:0.55};
            if(click!=null&&enabled){b.Cursor=System.Windows.Input.Cursors.Hand;b.MouseLeftButtonUp+=(o,e)=>click();}
            return b;
        }
        static UIElement Radio(bool on){
            var g=new Grid{Width=18,Height=18};g.Children.Add(new Ellipse{Stroke=Theme.B(on?Theme.Sign:Theme.Mist),StrokeThickness=2});
            if(on)g.Children.Add(new Ellipse{Width=8,Height=8,Fill=Theme.B(Theme.Sign)});return g;
        }
        UIElement VersionTab(Profile p){
            var list=new StackPanel();
            foreach(var v in s.Versions.Where(x=>s.Dev||x.channel!="test"||x.id==p.version)){
                var title=U.Row(8,U.Text(v.build+" "+v.Title(L.Lang),17,Theme.Text,true,FontWeights.SemiBold));
                var pill=v.channel=="test"?U.Pill(L.S("state.test"),Theme.Ink,Theme.Hazard):U.Pill(L.S("f.stable"),Theme.Go,Theme.C("#16301A"));pill.Margin=new Thickness(10,2,0,0);pill.VerticalAlignment=VerticalAlignment.Center;title.Children.Add(pill);
                var notes=v.Notes(L.Lang);
                var left=U.Col(3,title,U.Text(notes!=null&&notes.Count>0?notes[0]:"",13,Theme.Mist));
                string id=v.id;
                list.Children.Add(Option(left,Radio(p.version==v.id),p.version==v.id,()=>{var q=s.Lib.Profile(p.id);q.version=id;s.Lib.SaveProfile(q);s.Reload();}));
            }
            list.Children.Add(U.Text(L.S(p.test?"ver.hint.test":"ver.hint")+(s.Dev?" "+L.S("ver.hint.dev"):""),13,Theme.Mist));
            return list;
        }
        // 2.4.4: the profile's mod conflicts (ModConflicts), grouped by the mods involved: what blocks Play first, then whole files
        // one mod's copy replaces, then text / XML files the Suite merges. null = no conflicts.
        UIElement Conflicts(Profile p,VersionManifest v){
            List<ModConflict> all;
            // a failure of the check is shown, never hidden (an empty panel would read as "no conflicts")
            try{all=ModConflicts.For(s.Env,p,v);}catch(Exception e){var er=U.Row(8,U.Dot(Theme.Hazard,7),U.Text(L.F("conf.error",e.Message),13,Theme.Hazard));((TextBlock)er.Children[1]).TextWrapping=TextWrapping.Wrap;er.Margin=new Thickness(0,0,24,14);return er;}
            if(all.Count==0)return null;
            var box=new StackPanel();
            bool blocks=all.Any(c=>c.verdict=="blocks");
            box.Children.Add(U.Label(L.S(blocks?"conf.title.blocks":"conf.title"),blocks?Theme.Hazard:Theme.Mist));
            foreach(var g in all.GroupBy(c=>c.verdict+"\u0001"+c.kind+"\u0001"+String.Join("\u0001",c.mods)+"\u0001"+(c.winner??""))){
                var first=g.First();string names=String.Join(L.S("conf.and"),first.mods);int n=g.Count();string what=Shown(first.what);
                string text;
                if(first.kind=="asi-gtavxr")text=L.F("conf.asi.gtavxr",names,what);
                else if(first.verdict=="blocks")text=L.F(n==1?"conf.blocks1":"conf.blocksn",names,n==1?what:n.ToString());
                else if(first.verdict=="wins")text=L.F(n==1?"conf.wins1":"conf.winsn",names,n==1?what:n.ToString(),first.winner);
                else text=L.F(n==1?"conf.merged1":"conf.mergedn",names,n==1?what:n.ToString());
                var line=U.Row(8,U.Dot(first.verdict=="blocks"?Theme.Hazard:first.verdict=="wins"?Theme.Sign:Theme.Go,7),U.Text(text,13,first.verdict=="blocks"?Theme.Hazard:Theme.Text));
                ((TextBlock)line.Children[1]).TextWrapping=TextWrapping.Wrap;line.Margin=new Thickness(0,6,24,0);box.Children.Add(line);
                if(n>1){
                    var files=new StackPanel();foreach(var c in g.Take(500)){var t=U.Text(Shown(c.what),12,Theme.Mist);t.FontFamily=Theme.Mono;files.Children.Add(t);}
                    if(n>500)files.Children.Add(U.Text(L.F("conf.more",n-500),12,Theme.Mist));
                    var ex=new Expander{Header=U.Text(L.S("conf.files"),12,Theme.Mist),Content=new ScrollViewer{Content=files,MaxHeight=220,VerticalScrollBarVisibility=ScrollBarVisibility.Auto},Margin=new Thickness(15,2,24,0)};
                    box.Children.Add(ex);
                }
            }
            return new Border{Child=box,BorderBrush=Theme.B(blocks?Theme.Hazard:Theme.Line),BorderThickness=new Thickness(1.5),CornerRadius=new CornerRadius(10),Padding=new Thickness(16,12,16,14),Margin=new Thickness(0,0,0,14)};
        }
        // a content file is "archive|path inside it": shown as archive › path
        static string Shown(string what){return (what??"").Replace("|"," › ");}
        public static string PersonalizeMod;   // the mod whose Personalizar page is open in the Mods tab (null = the list)
        UIElement ModsTab(Profile p){
            var v=s.Version(p.version);
            var pm=PersonalizeMod==null?null:s.Mod(PersonalizeMod);
            if(pm!=null&&(pm.kind=="content-source"||pm.kind=="script")&&p.mods.Contains(pm.id))return Personalize(p,pm);
            PersonalizeMod=null;
            var list=new StackPanel();
            // 2.4.4: what this profile's mods share and what the Suite does about it, before Play
            var conflicts=Conflicts(p,v);if(conflicts!=null)list.Children.Add(conflicts);
            var cards=s.Mods.Where(m=>m.IsCard()).ToList();
            if(cards.Count==0)list.Children.Add(U.Text(L.S("mods.empty"),14,Theme.Mist));
            var onCats=new HashSet<string>(p.mods.Select(x=>s.Mod(x)).Where(x=>x!=null&&x.source!=null&&x.source.catalog!=null).Select(x=>x.source.catalog));
            foreach(var m in cards){
                bool works=m.WorksWith(v);bool required=v!=null&&v.mvga=="required"&&m.kind=="mvga-native";
                bool on=required||(works&&p.mods.Contains(m.id));
                bool untested=!p.test&&works&&!required&&m.tested_utc==null;
                var cat=m.source==null?null:Content.ContentCatalog.Mod(m.source.catalog);
                // a mod that needs another one (WoV Plus needs World of Variety)
                string needs=cat==null||cat.requires_mods==null?null:String.Join(", ",cat.requires_mods.Where(r=>!onCats.Contains(r)).Select(r=>{var c=Content.ContentCatalog.Mod(r);return c==null?r:c.name;}));
                if(needs=="")needs=null;
                string state=!m.loadable?L.S("modsv.pending"):required?L.S("mods.required"):!works?L.S("mods.no.version"):(m.version??"");
                string sub=L.Category(m.category)+(state.Length>0?" · "+state:"");
                var left=U.Col(2,U.Text(m.name,17,Theme.Text,true,FontWeights.SemiBold),U.Text(sub,13,Theme.Mist));
                if(cat!=null&&cat.about!=null){var ab=U.Text(Content.ContentCatalog.T(cat.about,L.Lang),13,Theme.Mist);ab.TextWrapping=TextWrapping.Wrap;ab.Margin=new Thickness(0,4,24,0);left.Children.Add(ab);}
                if(cat!=null&&cat.known_issue!=null){var ki=U.Row(8,U.Dot(Theme.Hazard,7),U.Text(Content.ContentCatalog.T(cat.known_issue,L.Lang),13,Theme.Hazard));ki.Margin=new Thickness(0,4,0,0);left.Children.Add(ki);}
                if(needs!=null){var nd=U.Row(8,U.Dot(Theme.Hazard,7),U.Text(L.F(on?"mods.notused":"mods.requires",needs),13,Theme.Hazard));nd.Margin=new Thickness(0,4,0,0);left.Children.Add(nd);}
                // why a script mod cannot be loaded yet (ScriptHookVDotNet 3, Enhanced-only files...)
                string why=m.loadable||m.not_loadable_reason==null?null:L.S("mods.why."+m.not_loadable_reason);
                // 2.4.3: a ScriptHookVDotNet 3 mod on a script host version without the v3 API: say which version to pick
                if(why==null&&m.loadable&&m.kind=="script"&&m.script_api=="v3"&&v!=null&&v.script_host=="switch"&&!v.RunsShvdnV3())why=L.S("mods.why.SHVDN_V3_VERSION");
                // 2.4.4: an .asi mod on a script host version that does not load .asi mods yet
                if(why==null&&m.loadable&&m.kind=="script"&&m.HasAsi()&&v!=null&&v.script_host=="switch"&&!v.RunsAsiMods())why=L.S("mods.why.ASI_VERSION");
                // 2.4.4: a mod that still names ScriptHookV.dll inside after the rename (it may look it up while running)
                if(m.loadable&&m.shv_by_name!=null&&m.shv_by_name.Count>0){var sb=U.Row(8,U.Dot(Theme.Hazard,7),U.Text(L.S("mods.shv.byname"),13,Theme.Hazard));
                    ((TextBlock)sb.Children[1]).TextWrapping=TextWrapping.Wrap;sb.Margin=new Thickness(0,4,24,0);sb.ToolTip=String.Join("\n",m.shv_by_name);left.Children.Add(sb);}
                if(why!=null&&why!="mods.why."+m.not_loadable_reason){var wy=U.Text(why,13,Theme.Hazard);wy.TextWrapping=TextWrapping.Wrap;wy.Margin=new Thickness(0,4,24,0);left.Children.Add(wy);}
                if(untested){
                    var warn=U.Row(8,U.Dot(Theme.Hazard,7),U.Text(L.S("mods.untested"),13,Theme.Hazard));
                    var tryIt=U.Btn(L.S("mods.try"),"Link",()=>shell.TryMod(m));tryIt.Foreground=Theme.B(Theme.Sign);tryIt.Margin=new Thickness(10,0,0,0);tryIt.VerticalAlignment=VerticalAlignment.Center;
                    warn.Children.Add(tryIt);warn.Margin=new Thickness(0,4,0,0);left.Children.Add(warn);
                }
                if(on&&cat!=null&&cat.options!=null&&cat.options.Count>0){
                    int changed=p.options==null?0:p.options.Keys.Count(k=>k.StartsWith(cat.id+".",StringComparison.Ordinal));   // the player's own changes
                    string mid=m.id;
                    var pers=U.Btn(L.S("mods.personalize")+(changed>0?"  ·  "+L.F(changed==1?"mods.changed1":"mods.changedn",changed):""),"Link",()=>{PersonalizeMod=mid;shell.Refresh();});
                    pers.Foreground=Theme.B(Theme.Sign);pers.HorizontalAlignment=HorizontalAlignment.Left;pers.Margin=new Thickness(0,6,0,0);left.Children.Add(pers);
                }
                var sw=new CheckBox{Style=(Style)Application.Current.Resources["Switch"],IsChecked=on,IsEnabled=works&&!required&&!s.Busy&&(on||needs==null),ToolTip=m.name};
                string id=m.id;string kind=m.kind;
                sw.Click+=(o,e)=>{var q=s.Lib.Profile(p.id);if(sw.IsChecked==true){
                        // one content profile in the game at a time: a project-built profile and the Suite's own mods exclude each other
                        if(kind=="content-native")q.mods.RemoveAll(x=>{var o2=s.Lib.Mod(x);return o2!=null&&(o2.kind=="content-native"||o2.kind=="content-source")&&x!=id;});
                        if(kind=="content-source")q.mods.RemoveAll(x=>{var o2=s.Lib.Mod(x);return o2!=null&&o2.kind=="content-native";});
                        if(!q.mods.Contains(id))q.mods.Add(id);}else q.mods.Remove(id);
                    s.Lib.SaveProfile(q);s.Reload();};
                list.Children.Add(Option(left,sw,false,null,works||required));
            }
            if(v!=null&&v.content=="profile")list.Children.Add(WorldBox(p,v));
            list.Children.Add(U.Text(L.S(p.test?"mods.hint.test":"mods.hint"),13,Theme.Mist));
            return list;
        }

        // "Mundo": the profile's starting density and Online maps (the VR menu Mods page changes them live in game).
        UIElement WorldBox(Profile p,VersionManifest v){
            // the profile's own settings, else the version's seed it starts from
            string ini=ProfileIni(p,v);
            Func<string,int,int> num=(k,d)=>{int n;return Int32.TryParse(VrIni.Get(ini,k),out n)&&n>=0&&n<=100&&n%10==0?n:d;};
            var col=new StackPanel();
            col.Children.Add(U.Text(L.S("world.title"),17,Theme.Text,true,FontWeights.SemiBold));
            var hint=U.Text(L.S("world.hint"),13,Theme.Mist);hint.TextWrapping=TextWrapping.Wrap;hint.Margin=new Thickness(0,2,0,10);col.Children.Add(hint);
            col.Children.Add(SliderRow(L.S("world.peds"),0,100,10,num("mods_peds166",100),"%",n=>SetVr(p,v,"mods_peds166",n.ToString())));
            col.Children.Add(SliderRow(L.S("world.traffic"),0,100,10,num("mods_traffic166",100),"%",n=>SetVr(p,v,"mods_traffic166",n.ToString())));
            // 2.4.2: a script mod of this profile that needs the Online map data turns them on at Play (PlaySession.StageOnlineMaps)
            var needMaps=Content.ContentCatalog.NeedsOnlineMaps(p.mods.Select(x=>s.Mod(x)).Where(x=>x!=null&&x.kind=="script"&&x.WorksWith(v)));
            bool forced=needMaps.Count>0;
            var maps=new CheckBox{Style=(Style)Application.Current.Resources["Switch"],IsChecked=forced||VrIni.Get(ini,"mods_mp_maps166")=="1",IsEnabled=!s.Busy&&!forced};
            maps.Click+=(o,e)=>SetVr(p,v,"mods_mp_maps166",maps.IsChecked==true?"1":"0");
            var ml=U.Col(2,U.Text(L.S("world.mpmaps"),15,Theme.Text),U.Text(forced?L.F("world.mpmaps.needed",String.Join(", ",needMaps)):L.S("world.mpmaps.hint"),12,forced?Theme.Hazard:Theme.Mist));((TextBlock)ml.Children[1]).TextWrapping=TextWrapping.Wrap;
            col.Children.Add(Row2(ml,maps));
            var b=new Border{Child=col,Background=Theme.B(Theme.Terminal),BorderBrush=Theme.B(Theme.Line),BorderThickness=new Thickness(1.5),CornerRadius=new CornerRadius(9),Padding=new Thickness(14,12,14,12),Margin=new Thickness(0,4,0,10)};
            return b;
        }
        // the profile's own settings, else the version's seed it starts from
        string ProfileIni(Profile p,VersionManifest v){
            string own=s.Lib.VrSettingsFile(p.id),seed=v.vr_settings_seed==null?null:new Blobs(s.Env).PathOf(v.vr_settings_seed.sha256);
            return p.has_vr_settings&&File.Exists(own)?File.ReadAllText(own):seed!=null&&File.Exists(seed)?File.ReadAllText(seed):"";
        }
        string OwnVrFile(Profile p,VersionManifest v){
            var q=s.Lib.Profile(p.id);string f=s.Lib.VrSettingsFile(p.id);
            // a profile on the version's seed settings gets its own file first (the seed stays what it started from)
            if(!q.has_vr_settings||!File.Exists(f)){if(v.vr_settings_seed!=null)new Blobs(s.Env).Materialize(v.vr_settings_seed,f);else File.WriteAllText(f,Importers.StartingVrSettings,new System.Text.UTF8Encoding(false));q.has_vr_settings=true;s.Lib.SaveProfile(q);}
            return f;
        }
        void SetVr(Profile p,VersionManifest v,string key,string value){
            string f=OwnVrFile(p,v);
            File.WriteAllText(f,VrIni.Set(File.ReadAllText(f),key,value),new System.Text.UTF8Encoding(false));
            s.Reload();
        }
        // 167-neural: the [Graphics] keys are written only here, when the player changes them (the VR menu's live changes stay).
        void SetGraphics(Profile p,VersionManifest v,GraphicsChoice c){
            string f=OwnVrFile(p,v);
            File.WriteAllText(f,GtavxrSuite.Graphics.Write(File.ReadAllText(f),c),new System.Text.UTF8Encoding(false));
            s.Reload();
        }
        static GraphicsChoice With(GraphicsChoice g,string preset,string upscale,bool neural,int intensity){
            return new GraphicsChoice{preset=preset,upscale=upscale,neural=neural,intensity=intensity};
        }

        // "Graficos" (167-neural B5): DLSS Super Resolution and the neural pass of this profile, the player's DLSS files and OFXR.
        UIElement GraphicsBox(Profile p,VersionManifest v){
            var g=GtavxrSuite.Graphics.Read(ProfileIni(p,v));
            var col=new StackPanel();
            col.Children.Add(U.Text(L.S("gfx.title"),17,Theme.Text,true,FontWeights.SemiBold));
            var hint=U.Text(L.S("gfx.hint"),13,Theme.Mist);hint.TextWrapping=TextWrapping.Wrap;hint.Margin=new Thickness(0,2,0,10);col.Children.Add(hint);
            var pr=new WrapPanel();
            foreach(string id in GtavxrSuite.Graphics.Presets){
                string pid=id;var b=U.Btn(L.S("gfx.preset."+pid),g.preset==pid?"Primary":"Btn",()=>SetGraphics(p,v,GtavxrSuite.Graphics.For(pid,g)));
                b.Margin=new Thickness(0,0,8,8);b.IsEnabled=!s.Busy;pr.Children.Add(b);
            }
            col.Children.Add(pr);
            var ph=U.Text(L.S("gfx.preset."+g.preset+".hint"),12,Theme.Mist);ph.TextWrapping=TextWrapping.Wrap;ph.Margin=new Thickness(0,0,0,6);col.Children.Add(ph);
            bool custom=g.preset=="custom";
            if(g.upscale!="off"||custom){
                var lv=new WrapPanel();
                var levels=custom?new[]{"off"}.Concat(GtavxrSuite.Graphics.Levels).ToArray():GtavxrSuite.Graphics.Levels;
                foreach(string l in levels){
                    string lid=l;var b=U.Btn(L.S("gfx.level."+lid),g.upscale==lid?"Primary":"Btn",()=>SetGraphics(p,v,With(g,g.preset,lid,g.neural,g.intensity)));
                    b.Margin=new Thickness(6,0,0,6);b.IsEnabled=!s.Busy;lv.Children.Add(b);
                }
                col.Children.Add(Row2(U.Text(L.S("gfx.level"),15,Theme.Text),lv));
            }
            if(custom){
                var sw=new CheckBox{Style=(Style)Application.Current.Resources["Switch"],IsChecked=g.neural,IsEnabled=!s.Busy};
                sw.Click+=(o,e)=>SetGraphics(p,v,With(g,"custom",g.upscale,sw.IsChecked==true,g.intensity));
                var nl=U.Col(2,U.Text(L.S("gfx.neural"),15,Theme.Text),U.Text(L.S("gfx.neural.hint"),12,Theme.Mist));((TextBlock)nl.Children[1]).TextWrapping=TextWrapping.Wrap;
                col.Children.Add(Row2(nl,sw));
            }
            if(g.neural)col.Children.Add(SliderRow(L.S("gfx.intensity"),0,100,5,g.intensity,"%",n=>SetGraphics(p,v,With(g,g.preset,g.upscale,g.neural,n))));
            // the player's DLSS files, as a Play would find them now
            if(g.NeedsDlss()){
                var dot=U.Dot(Theme.Mist,6);
                var label=U.Text("",12,Theme.Mist);label.TextWrapping=TextWrapping.Wrap;
                var dl=U.Row(6,dot,label);dl.Margin=new Thickness(0,6,0,0);col.Children.Add(dl);
                s.ShowDlss((f,checking)=>{
                    string text;Color c;
                    if(checking){text=L.S("gfx.dlss.check");c=Theme.Mist;}
                    else if(f==null){text=L.S("gfx.dlss.none");c=Theme.Hazard;}
                    else if(!Dlss.Usable(f)){text=L.F("gfx.dlss.bad",f.dir);c=Theme.Hazard;}
                    else if(g.neural&&!Dlss.NeuralUsable(f)){text=L.S("gfx.dlss.nonr");c=Theme.Hazard;}
                    else{text=g.neural?L.F("set.dlss.models.nr",f.sr.version??"?",f.nr.version??"?"):L.F("set.dlss.models.sr",f.sr.version??"?");c=Theme.Go;}
                    label.Text=text;label.Foreground=dot.Fill=Theme.B(c);
                });
            }
            // OFXR: the player's own tool, switched on from its tray before putting the headset on
            var of=s.OfxrNow();
            var oh=U.Text(L.S("gfx.ofxr"),15,Theme.Text,true,FontWeights.SemiBold);oh.Margin=new Thickness(0,12,0,2);col.Children.Add(oh);
            if(!of.layer)col.Children.Add(U.Text(L.S("gfx.ofxr.none"),12,Theme.Mist));
            else{
                col.Children.Add(U.Text(L.F("gfx.ofxr.status",L.S("ofxr."+(of.status??"no-data"))),12,Theme.Text));
                var acts=new WrapPanel{Margin=new Thickness(0,6,0,0)};
                string tray=of.tray,profile=of.profile;
                var open=U.Btn(L.S("gfx.ofxr.open"),"Btn",()=>{try{System.Diagnostics.Process.Start(tray);}catch(Exception){shell.Info(L.S("gfx.ofxr"),L.S("gfx.ofxr.notray"));}});
                open.IsEnabled=tray!=null;open.Margin=new Thickness(0,0,8,8);acts.Children.Add(open);
                var rec=U.Btn(L.S("gfx.ofxr.recommend"),"Ghost",()=>{string err=Ofxr.Recommend(profile);if(err==null)shell.Toast(L.S("gfx.ofxr.done"));else shell.Info(L.S("gfx.ofxr"),L.S("ofxr.err."+err));});
                rec.IsEnabled=profile!=null&&!s.Busy;rec.Margin=new Thickness(0,0,8,8);acts.Children.Add(rec);
                col.Children.Add(acts);
                if(tray==null){var th=U.Text(L.S("gfx.ofxr.notray"),12,Theme.Mist);th.TextWrapping=TextWrapping.Wrap;col.Children.Add(th);}
                if(profile==null){var pf=U.Text(L.S("gfx.ofxr.noprofile"),12,Theme.Mist);pf.TextWrapping=TextWrapping.Wrap;col.Children.Add(pf);}
                var ssw=U.Row(6,U.Dot(Theme.Hazard,6),U.Text(L.S("gfx.ofxr.ssw"),12,Theme.Hazard));((TextBlock)ssw.Children[1]).TextWrapping=TextWrapping.Wrap;ssw.Margin=new Thickness(0,2,0,0);col.Children.Add(ssw);
            }
            return new Border{Child=col,Background=Theme.B(Theme.Terminal),BorderBrush=Theme.B(Theme.Line),BorderThickness=new Thickness(1.5),CornerRadius=new CornerRadius(9),Padding=new Thickness(14,12,14,12),Margin=new Thickness(0,4,0,10)};
        }
        static Grid Row2(UIElement left,UIElement right){
            var g=new Grid{Margin=new Thickness(0,4,0,4)};g.ColumnDefinitions.Add(new ColumnDefinition());g.ColumnDefinitions.Add(new ColumnDefinition{Width=GridLength.Auto});
            g.Children.Add(left);if(right!=null){Grid.SetColumn(right,1);g.Children.Add(right);((FrameworkElement)right).VerticalAlignment=VerticalAlignment.Center;}
            return g;
        }
        UIElement SliderRow(string title,int min,int max,int step,int value,string unit,Action<int> set){
            var val=U.Text(value+unit,15,Theme.Text,true,FontWeights.SemiBold);val.Width=64;val.TextAlignment=TextAlignment.Right;
            var sl=new Slider{Minimum=min,Maximum=max,Value=value,TickFrequency=step,IsSnapToTickEnabled=true,Width=260,VerticalAlignment=VerticalAlignment.Center,IsEnabled=!s.Busy};
            sl.ValueChanged+=(o,e)=>{val.Text=((int)sl.Value)+unit;};
            sl.PreviewMouseUp+=(o,e)=>{if((int)sl.Value!=value)set((int)sl.Value);};
            sl.KeyUp+=(o,e)=>{if((int)sl.Value!=value)set((int)sl.Value);};
            return Row2(U.Text(title,15,Theme.Text),U.Row(10,sl,val));
        }

        // Personalizar: the mod's own parts and options (resources/embedded/content-catalog.json), for this profile.
        UIElement Personalize(Profile p,ModManifest m){
            var cat=Content.ContentCatalog.Mod(m.source.catalog);
            var col=new StackPanel();
            var back=U.Btn("‹  "+L.S("mods.back"),"Link",()=>{PersonalizeMod=null;shell.Refresh();});back.Foreground=Theme.B(Theme.Sign);back.HorizontalAlignment=HorizontalAlignment.Left;
            col.Children.Add(back);
            var head=U.Col(2,U.Text(L.S("mods.personalize")+": "+m.name,22,Theme.Text,true,FontWeights.SemiBold));head.Margin=new Thickness(0,8,0,4);col.Children.Add(head);
            if(cat.about!=null){var ab=U.Text(Content.ContentCatalog.T(cat.about,L.Lang),13,Theme.Mist);ab.TextWrapping=TextWrapping.Wrap;ab.Margin=new Thickness(0,0,0,10);col.Children.Add(ab);}
            // the states of the mods this one depends on, for the reasons shown
            var known=new List<Content.OptionState>();
            foreach(string r in cat.requires_mods??new List<string>()){var rc=Content.ContentCatalog.Mod(r);if(rc!=null)known.AddRange(Content.ContentCatalog.States(rc,p.Option,L.Lang));}
            var states=Content.ContentCatalog.States(cat,p.Option,L.Lang,known);
            string group=null;
            foreach(var st in states){
                if(!st.Visible)continue;
                var o=st.Option;int depth=st.Key.Count(c=>c=='.')-1;
                string g=o.group==null?null:Content.ContentCatalog.T(o.group,L.Lang);
                if(g!=null&&g!=group){var gh=U.Text(g,16,Theme.Sign,true,FontWeights.SemiBold);gh.Margin=new Thickness(0,12,0,4);col.Children.Add(gh);}
                group=g;
                col.Children.Add(OptionRow(p,st,depth));
            }
            var reset=U.Btn(L.S("mods.reset"),"Ghost",()=>{var q=s.Lib.Profile(p.id);if(q.options!=null){foreach(var k in q.options.Keys.Where(k=>k.StartsWith(cat.id+".",StringComparison.Ordinal)).ToList())q.options.Remove(k);s.Lib.SaveProfile(q);}s.Reload();});
            reset.HorizontalAlignment=HorizontalAlignment.Left;reset.Margin=new Thickness(0,12,0,8);col.Children.Add(reset);
            col.Children.Add(U.Text(L.S("mods.personalize.hint"),13,Theme.Mist));
            return col;
        }
        UIElement OptionRow(Profile p,Content.OptionState st,int depth){
            var o=st.Option;bool blocked=st.Blocked!=null;
            var left=U.Col(2,U.Text(Content.ContentCatalog.T(o.title,L.Lang),depth==0?17:15,Theme.Text,true,depth==0?FontWeights.SemiBold:FontWeights.Normal));
            if(o.hint!=null){var h=U.Text(Content.ContentCatalog.T(o.hint,L.Lang),12,Theme.Mist);h.TextWrapping=TextWrapping.Wrap;h.Margin=new Thickness(0,0,20,0);left.Children.Add(h);}
            if(o.vr_cost){var c=U.Row(6,U.Dot(Theme.Hazard,6),U.Text(L.S("mods.vrcost"),12,Theme.Hazard));c.Margin=new Thickness(0,2,0,0);left.Children.Add(c);}
            if(blocked){var c=U.Row(6,U.Dot(Theme.Hazard,6),U.Text(st.Blocked,12,Theme.Hazard));c.Margin=new Thickness(0,2,0,0);left.Children.Add(c);}
            UIElement right=null;string key=st.Key;
            Action<string> set=val=>{var q=s.Lib.Profile(p.id);if(q.options==null)q.options=new Dictionary<string,string>();if(val==o.@default)q.options.Remove(key);else q.options[key]=val;s.Lib.SaveProfile(q);s.Reload();};
            if(o.type=="switch"){
                var sw=new CheckBox{Style=(Style)Application.Current.Resources["Switch"],IsChecked=st.Value=="on",IsEnabled=!blocked&&!s.Busy};
                sw.Click+=(x,e)=>set(sw.IsChecked==true?"on":"off");right=sw;
            }else if(o.type=="choice"){
                var row=new StackPanel{Orientation=Orientation.Horizontal};
                foreach(var val in o.values){string id=val.id;var b=U.Btn(Content.ContentCatalog.T(val.title,L.Lang),st.Value==id?"Primary":"Btn",()=>set(id));b.Margin=new Thickness(6,0,0,0);b.IsEnabled=!blocked&&!s.Busy;row.Children.Add(b);}
                right=row;
            }else if(o.type=="list"){
                // a long list (radio stations): a drop-down instead of a row of buttons
                var box=new ComboBox{Width=260,IsEnabled=!blocked&&!s.Busy,VerticalAlignment=VerticalAlignment.Center};
                foreach(var val in o.values??new List<Content.CatValue>()){var it=new ComboBoxItem{Content=Content.ContentCatalog.T(val.title,L.Lang),Tag=val.id};box.Items.Add(it);if(st.Value==val.id)box.SelectedItem=it;}
                box.SelectionChanged+=(x,e)=>{var it=box.SelectedItem as ComboBoxItem;if(it!=null&&(string)it.Tag!=st.Value)set((string)it.Tag);};
                right=box;
            }else if(o.type=="slider"){
                int n;if(!Int32.TryParse(st.Value,out n))Int32.TryParse(o.@default,out n);
                return Indent(SliderRow(Content.ContentCatalog.T(o.title,L.Lang),o.min,o.max,o.step,n,o.unit??"",x=>set(x.ToString())),depth);
            }
            var b2=new Border{Child=Row2(left,right),Background=Theme.B(Theme.Terminal),BorderBrush=Theme.B(Theme.Line),BorderThickness=new Thickness(1.5),CornerRadius=new CornerRadius(9),
                Padding=new Thickness(14,10,14,10),Margin=new Thickness(depth*28,0,0,8),Opacity=blocked?0.6:1};
            return b2;
        }
        static UIElement Indent(UIElement e,int depth){var b=new Border{Child=e,Margin=new Thickness(depth*28+14,0,14,8)};return b;}
        UIElement SaveTab(Profile p){
            var list=new StackPanel();
            string user=s.Lib.UserDir(p.id);
            var saves=Directory.GetFiles(user,"SGTA5*",SearchOption.AllDirectories).Select(SaveInfo.Read).Where(x=>x!=null).OrderByDescending(x=>x.written_utc).Take(5).ToList();
            if(saves.Count==0)list.Children.Add(Option(U.Text(L.S("save.none"),17,Theme.Text,true,FontWeights.SemiBold),null,true,null));
            bool first=true;
            foreach(var sv in saves){
                var left=U.Col(2,U.Text(sv.Short(L.Lang),17,Theme.Text,true,FontWeights.SemiBold),U.Text((first?L.S("save.latest")+" · ":"")+L.Ago(sv.written_utc.ToString("o")),13,Theme.Mist));
                list.Children.Add(Option(left,null,first,null));first=false;
            }
            var btns=U.Row(10,U.Btn(L.S("save.copy"),"Btn",()=>shell.Confirm(L.S("save.copy"),L.S("save.copy.q"),L.S("yes"),()=>{s.Imp.ResetSaves(p.id,true);s.Reload();},false)),
                U.Btn(L.S("save.fresh"),"Ghost",()=>shell.Confirm(L.S("save.fresh"),L.S("save.fresh.q"),L.S("yes"),()=>{s.Imp.ResetSaves(p.id,false);s.Reload();},false)));
            btns.Margin=new Thickness(0,4,0,10);list.Children.Add(btns);
            list.Children.Add(U.Text(L.S("save.hint"),13,Theme.Mist));
            return list;
        }
        UIElement VrTab(Profile p){
            var list=new StackPanel();
            list.Children.Add(Option(U.Col(2,U.Text(p.has_vr_settings?L.S("f.vr.title"):L.S("f.vr.default"),17,Theme.Text,true,FontWeights.SemiBold),U.Text(L.S("vr.hint"),13,Theme.Mist)),null,false,null));
            var others=s.Profiles.Where(x=>x.id!=p.id&&x.has_vr_settings).ToList();
            var row=new WrapPanel();
            foreach(var o in others){string from=o.id;var b=U.Btn(L.S("vr.copyfrom")+": "+o.name,"Btn",()=>{File.Copy(s.Lib.VrSettingsFile(from),s.Lib.VrSettingsFile(p.id),true);var q=s.Lib.Profile(p.id);q.has_vr_settings=true;s.Lib.SaveProfile(q);s.Reload();shell.Toast(L.S("ok"));});b.Margin=new Thickness(0,0,10,8);row.Children.Add(b);}
            if(p.has_vr_settings){var r=U.Btn(L.S("vr.reset"),"Ghost",()=>shell.Confirm(L.S("vr.reset"),L.S("vr.reset")+"?",L.S("yes"),()=>{var q=s.Lib.Profile(p.id);File.Delete(s.Lib.VrSettingsFile(p.id));q.has_vr_settings=false;s.Lib.SaveProfile(q);s.Reload();},false));r.Margin=new Thickness(0,0,10,8);row.Children.Add(r);}
            list.Children.Add(row);
            var ver=s.Version(p.version);
            if(ver!=null&&ver.neural!=null)list.Children.Add(GraphicsBox(p,ver));
            return list;
        }
        UIElement ProfileTab(Profile p){
            var name=new TextBox{Text=p.name,Style=(Style)Application.Current.Resources["Field"],Width=360,MaxLength=40};
            var save=U.Btn(L.S("ok"),"Btn",()=>{var n=(name.Text??"").Trim();if(n.Length==0)return;var q=s.Lib.Profile(p.id);q.name=n;s.Lib.SaveProfile(q);s.Reload();});
            var colors=new StackPanel{Orientation=Orientation.Horizontal};
            for(int i=0;i<Theme.Bands.Length;i++){
                int ci=i;var sw=new Border{Width=30,Height=30,CornerRadius=new CornerRadius(15),Background=Theme.B(Theme.Bands[i]),Margin=new Thickness(0,0,10,0),Cursor=System.Windows.Input.Cursors.Hand,
                    BorderBrush=Theme.B(p.color==i?Theme.Text:Theme.Terminal),BorderThickness=new Thickness(p.color==i?3:2)};
                sw.MouseLeftButtonUp+=(o,e)=>{var q=s.Lib.Profile(p.id);q.color=ci;s.Lib.SaveProfile(q);s.Reload();};colors.Children.Add(sw);
            }
            // Imagen de perfil: the gate picture of this profile, one small window per colour variant.
            var images=new WrapPanel();string current=Assets.GateId(p.image);
            foreach(string id in Assets.GateIds){
                string gid=id;var pic=Assets.Gate(gid);if(pic==null)continue;bool on=gid==current;
                var cap=U.Text(L.S("img."+gid),12,on?Theme.Text:Theme.Mist,true,on?FontWeights.Bold:FontWeights.Normal);cap.HorizontalAlignment=HorizontalAlignment.Center;
                var tile=new Border{Padding=new Thickness(6),Margin=new Thickness(0,0,10,8),CornerRadius=new CornerRadius(12),Cursor=System.Windows.Input.Cursors.Hand,Background=Brushes.Transparent,
                    BorderBrush=Theme.B(on?Theme.Sign:Theme.Line),BorderThickness=new Thickness(on?2.5:1.5),Child=U.Col(6,U.Porthole(pic,104,66),cap)};
                tile.MouseLeftButtonUp+=(o,e)=>{var q=s.Lib.Profile(p.id);q.image=gid==Assets.GateIds[0]?null:gid;s.Lib.SaveProfile(q);s.Reload();};
                images.Children.Add(tile);
            }
            // Test profile: switch, starting point, pass the version + mods to a normal profile.
            var testSw=new CheckBox{Style=(Style)Application.Current.Resources["Switch"],IsChecked=p.test};
            testSw.Click+=(o,e)=>{bool on=testSw.IsChecked==true;shell.Guard(()=>{s.Lib.SetTest(p.id,on);s.Reload();});};
            var testHead=new DockPanel{LastChildFill=true};DockPanel.SetDock(testSw,Dock.Right);testHead.Children.Add(testSw);
            testHead.Children.Add(U.Col(2,U.Text(L.S("new.test"),17,Theme.Text,true,FontWeights.SemiBold),U.Text(L.S("new.test.hint"),13,Theme.Mist)));
            var testBox=U.Col(10,testHead);
            if(p.test){
                var start=s.Lib.ReadCopy(p.id,"start");
                testBox.Children.Add(U.Text(start==null?L.S("start.none"):L.F("start.when",L.Ago(start.utc)),13,Theme.Mist));
                var acts=new WrapPanel();
                var back=U.Btn(L.S("start.back"),"Btn",()=>shell.Confirm(L.S("start.back"),L.F("start.back.q",p.name),L.S("start.back"),()=>{s.Imp.BackToStart(p.id);s.Reload();shell.Toast(L.S("start.back.done"));},false));
                back.IsEnabled=start!=null;back.Margin=new Thickness(0,0,10,8);acts.Children.Add(back);
                var set=U.Btn(L.S("start.set"),"Ghost",()=>shell.Confirm(L.S("start.set"),L.S("start.set.q"),L.S("start.set"),()=>{s.Lib.SetStartPoint(p.id);s.Reload();shell.Toast(L.S("ok"));},false));
                set.Margin=new Thickness(0,0,10,8);acts.Children.Add(set);
                var promote=U.Btn(L.S("fl.promote"),"Btn",()=>shell.Promote(p));promote.Margin=new Thickness(0,0,10,8);acts.Children.Add(promote);
                testBox.Children.Add(acts);
            }
            var testFrame=new Border{Child=testBox,Padding=new Thickness(14,12,14,6),CornerRadius=new CornerRadius(9),BorderThickness=new Thickness(1.5),
                BorderBrush=Theme.B(p.test?Theme.Hazard:Theme.Line,p.test?0.6:1),Background=Theme.B(Theme.Terminal)};
            var actions=U.Row(10,U.Btn(L.S("prof.dup"),"Btn",()=>{var c=s.Imp.Duplicate(p.id,p.name+" 2");s.Reload();s.Select(c.id);}),
                U.Btn(L.S("prof.delete"),"Danger",()=>shell.Confirm(L.S("prof.delete"),L.F("prof.delete.q",p.name),L.S("prof.delete"),()=>{s.Lib.DeleteProfile(p.id);s.Settings.selected_profile=null;s.Save();s.Reload();},true)));
            return U.Col(16,U.Col(6,U.Label(L.S("prof.name"),Theme.Mist),U.Row(10,name,save)),U.Col(8,U.Label(L.S("prof.color"),Theme.Mist),colors),
                U.Col(8,U.Label(L.S("prof.image"),Theme.Mist),images),testFrame,actions);
        }
    }
}
