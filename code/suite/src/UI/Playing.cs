using System;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;

namespace GtavxrSuite {
    // The screen while a profile is "boarding" and in flight: one big departures row that flips through the steps.
    // A test flight wears the hazard tape. While the game runs, "Abortar" ends it (asked twice) and everything lands.
    public sealed class PlayingPanel {
        public readonly Grid Root=new Grid();
        public Action OnAbort;
        public Func<bool> OnClose;   // "Cerrar GTA" (clean close request); runs off the UI thread
        readonly ContentControl stateRow=new ContentControl(),clockRow=new ContentControl(),abortRow=new ContentControl();
        readonly TextBlock hint;readonly ProgressBar bar;readonly TextBlock count;readonly bool test;
        public PlayingPanel(Profile p,VersionManifest v){
            test=p.test;
            var col=new StackPanel{HorizontalAlignment=HorizontalAlignment.Center,VerticalAlignment=VerticalAlignment.Center,Width=780};
            if(Assets.Logo!=null){var logo=U.Img(Assets.Logo,118);logo.HorizontalAlignment=HorizontalAlignment.Center;logo.Margin=new Thickness(0,0,0,18);col.Children.Add(logo);}
            if(test){var tape=U.Tape(L.S("tape.flight"),22);tape.Margin=new Thickness(0,0,0,16);col.Children.Add(tape);}
            var head=new DockPanel{LastChildFill=false};
            head.Children.Add(U.Label(L.S("board.title"),Theme.Sign));
            var gate=U.Label(L.S("stub.gate")+" "+(v==null?"":v.build),Theme.Mist);DockPanel.SetDock(gate,Dock.Right);head.Children.Add(gate);
            col.Children.Add(head);
            var name=new FlapText(p.name,22,Theme.FlapInk,30){Margin=new Thickness(0,12,0,0)};col.Children.Add(name);
            var sub=U.Text(v==null?"":(v.build+" "+v.Title(L.Lang)).ToUpperInvariant(),15,Theme.Mist,true,FontWeights.SemiBold);sub.Margin=new Thickness(2,8,0,22);col.Children.Add(sub);
            col.Children.Add(stateRow);
            bar=new ProgressBar{Height=4,Minimum=0,Maximum=1,Value=0,IsIndeterminate=true,Foreground=Theme.B(Theme.Sign),Background=Theme.B(Theme.Flap),BorderThickness=new Thickness(0),Margin=new Thickness(0,18,0,12)};
            col.Children.Add(bar);
            var line=new DockPanel{LastChildFill=false};
            hint=U.Text("",16,Theme.Text);line.Children.Add(hint);
            count=U.Text("",14,Theme.Mist);count.FontFamily=Theme.Mono;DockPanel.SetDock(count,Dock.Right);line.Children.Add(count);
            col.Children.Add(line);
            var bottom=new DockPanel{LastChildFill=false,Margin=new Thickness(0,26,0,0)};
            bottom.Children.Add(clockRow);
            DockPanel.SetDock(abortRow,Dock.Right);abortRow.VerticalAlignment=VerticalAlignment.Center;bottom.Children.Add(abortRow);
            col.Children.Add(bottom);
            var frame=new Border{Child=col,Padding=new Thickness(40,30,40,34),Background=Theme.PanelBrush(),BorderBrush=Theme.RimBrush(),BorderThickness=new Thickness(1.5),CornerRadius=new CornerRadius(16),
                HorizontalAlignment=HorizontalAlignment.Center,VerticalAlignment=VerticalAlignment.Center};
            Root.Children.Add(frame);
            SetState(L.S("run.game"),Word("game"),false);
        }
        // big flap word + plain sentence
        public void SetState(string sentence,string word,bool flip){
            var f=new FlapText(word,22,Theme.FlapInk,30);stateRow.Content=f;if(flip)f.Flip();hint.Text=sentence;
        }
        public void Progress(int done,int total){
            if(total>0){bar.IsIndeterminate=false;bar.Value=(double)done/total;count.Text=done+" / "+total;}else{bar.IsIndeterminate=true;count.Text="";}
        }
        public void Playing(){
            bar.IsIndeterminate=false;bar.Value=1;count.Text="";
            SetState(L.S(test?"run.playing.test":"run.playing.hint"),L.S("run.playing"),true);
            AbortButton(false);
        }
        public void Landing(){abortRow.Content=null;}
        void AbortButton(bool confirm){
            if(OnAbort==null)return;
            if(!confirm){
                var force=U.Btn(L.S(test?"abort.test":"abort.normal"),"Danger",()=>AbortButton(true));
                abortRow.Content=OnClose==null?(UIElement)force:U.Row(10,U.Btn(L.S("close.game"),"Primary",RequestClose),force);
                return;
            }
            var q=U.Text(L.S(test?"abort.q.test":"abort.q"),13,Theme.Mist);q.MaxWidth=300;q.VerticalAlignment=VerticalAlignment.Center;
            abortRow.Content=U.Row(10,q,U.Btn(L.S("abort.yes"),"Danger",()=>{abortRow.Content=U.Text(L.S("abort.doing"),13,Theme.Mist);OnAbort();}),
                U.Btn(L.S("cancel"),"Ghost",()=>AbortButton(false)));
        }
        // Clean close first; the force button comes back if the game did not accept the request.
        void RequestClose(){
            abortRow.Content=U.Text(L.S("close.doing"),13,Theme.Mist);
            var close=OnClose;var ui=Root.Dispatcher;
            System.Threading.Tasks.Task.Factory.StartNew(()=>{
                bool ok=false;try{ok=close();}catch(Exception){ok=false;}
                if(!ok)ui.BeginInvoke((Action)(()=>{AbortButton(false);hint.Text=L.S("close.failed");}));
            });
        }
        public void Clock(TimeSpan t){clockRow.Content=new FlapText(t.ToString(@"hh\:mm\:ss"),8,Theme.Sign,22);}
        public static string Word(string step){
            bool en=L.Lang=="en";
            switch(step){
                case "content":return en?"LOADING MODS":"CARGANDO MODS";
                case "game":return en?"PREPARING":"PREPARANDO";
                case "saves":return en?"BOARDING":"EMBARCANDO";
                case "launch":return en?"TAKE OFF":"DESPEGANDO";
                case "capture":return en?"LANDING":"ATERRIZANDO";
                case "revert":return en?"RESTORING":"RESTAURANDO";
                default:return step.ToUpperInvariant();
            }
        }
    }
}
