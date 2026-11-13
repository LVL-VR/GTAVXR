using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Threading;

namespace GtavxrSuite {
    public static class ScrollProbe {
        public static object CheckDlss(string models,string fresh) {
            fresh=SafeFs.Root(fresh);
            if(Directory.Exists(fresh))throw new InvalidDataException("Fresh DLSS probe required");
            var env=Env.ForFixture(fresh);
            var settings=env.LoadSettings();settings.dlss_dir=SafeFs.Root(models);env.SaveSettings(settings);
            var state=new AppState(env);state.Reload();
            var frame=new DispatcherFrame();
            var timer=new DispatcherTimer{Interval=TimeSpan.FromSeconds(10)};
            timer.Tick+=(sender,args)=>frame.Continue=false;
            bool loading=false,finished=false;
            DlssFolder result=null;
            var watch=Stopwatch.StartNew();
            state.ShowDlss((value,pending)=>{
                if(pending){loading=true;return;}
                finished=true;result=value;frame.Continue=false;
            });
            double dispatcherMs=watch.Elapsed.TotalMilliseconds;
            timer.Start();Dispatcher.PushFrame(frame);timer.Stop();
            double completed=watch.Elapsed.TotalMilliseconds;
            if(!loading||!finished||!Dlss.Usable(result))throw new InvalidDataException("DLSS asynchronous probe did not complete");
            state.Reload();
            bool cached=false;
            watch.Restart();state.ShowDlss((value,pending)=>cached=!pending&&Object.ReferenceEquals(value,result));
            if(!cached)throw new InvalidDataException("DLSS cache did not survive UI refresh");
            return new{status="PASS_DLSS_OFF_DISPATCHER",dispatcher_ms=dispatcherMs,complete_ms=completed,
                       cached_ms=watch.Elapsed.TotalMilliseconds,sr=result.sr.version,window_shown=false,gta_launched=false};
        }

        static IEnumerable<DependencyObject> Descendants(DependencyObject root) {
            for (int index=0; index<VisualTreeHelper.GetChildrenCount(root); ++index) {
                var child=VisualTreeHelper.GetChild(root,index);
                yield return child;
                foreach (var next in Descendants(child)) yield return next;
            }
        }

        public static object Run(string fresh) {
            fresh=SafeFs.Root(fresh);
            if (Directory.Exists(fresh)) throw new InvalidDataException("Fresh scroll probe required");
            Preview.EnsureApp();
            var results=new List<object>();
            foreach (int count in new[] { 12, 240 }) {
                var state=new AppState(Demo.Build(SafeFs.Under(fresh,"mods-"+count),"en","mods")) { Demo=true };
                state.Reload();
                state.Mods=Enumerable.Range(0,count).Select(index=>new ModManifest {
                    id="test-"+index, name="Synthetic mod "+index, kind="mvga-native", category="graphics",
                    loadable=true, version="1.0", files=new List<FileRef>()
                }).ToList();
                var shell=new Shell(state) { Section="mods" };
                var root=shell.Root;
                const int width=1200,height=740;
                Action layout=()=>{root.Measure(new Size(width,height));root.Arrange(new Rect(0,0,width,height));root.UpdateLayout();};
                var watch=Stopwatch.StartNew();
                shell.Refresh();layout();
                double initial=watch.Elapsed.TotalMilliseconds;
                var scroll=Descendants(root).OfType<ScrollViewer>().First(x=>x.ScrollableHeight>0);
                var render=new RenderTargetBitmap(width,height,96,96,PixelFormats.Pbgra32);
                render.Render(root);
                int visuals=Descendants(root).Count();
                var frames=new List<double>();
                for (int index=0;index<50;++index) {
                    watch.Restart();
                    scroll.ScrollToVerticalOffset(scroll.ScrollableHeight*(index+1)/50);
                    layout();
                    render.Clear();render.Render(root);
                    frames.Add(watch.Elapsed.TotalMilliseconds);
                }
                var refresh=new List<double>();
                for (int index=0;index<7;++index) {
                    watch.Restart();shell.Refresh();layout();
                    refresh.Add(watch.Elapsed.TotalMilliseconds);
                }
                frames.Sort();refresh.Sort();
                results.Add(new { screen="Mods", cards=count, visuals=visuals, initial_ms=initial,
                                  scroll_p50_ms=frames[25], scroll_p95_ms=frames[47], scroll_worst_ms=frames[49],
                                  refresh_p50_ms=refresh[3], refresh_worst_ms=refresh[6] });
            }
            return new { status="PASS_OFFSCREEN_SCROLL_PROBE", samples=results,
                         limit="Offscreen layout and rendering. No physical wheel, touchpad or timer reproduction.",
                         gta_launched=false, window_shown=false };
        }
    }
}
