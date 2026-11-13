using System;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.Versioning;

[assembly: AssemblyVersion("1.0.0.0")]
[assembly: AssemblyFileVersion("1.0.0.0")]
[assembly: AssemblyProduct("GTAVXR by LVL")]
[assembly: TargetFramework(".NETFramework,Version=v4.6.2")]

namespace GtavxrSuite {
    static class Program {
        [STAThread] public static int Main(string[] args) {
#if SUITE_FIXTURES
            if(args.Length==0 || (args[0]!="--bundle-fixtures" && args[0]!="--version" && args[0]!="--render-all" && args[0]!="--runtime-report"))return 64;
#endif
            // Process-local long path support. No registry or OS policy change.
            AppContext.SetSwitch("Switch.System.IO.UseLegacyPathHandling",false);
            AppContext.SetSwitch("Switch.System.IO.BlockLongPaths",false);
            if(args.Length==2&&args[0]=="--library")return SuiteApp.Start(args[1]);
            if(args.Length>0&&args[0]=="--fixture-game")return FixtureGame.Run(args);
            if(args.Length>0){
                try{return Headless(args);}
                catch(Exception e){WriteResult(args,new{status="FAIL",error=e.GetType().Name,message=e.Message,stack=e.StackTrace});return 1;}
            }
            return SuiteApp.Start();
        }
        // Headless commands never open a window, never start GTA and never touch the real game folder.
        private static int Headless(string[] a) {
            if(a[0]=="--self-test"&&a.Length==3){var r=EngineTests.Run(a[1]);Json.New(a[2],r);return r.failed==0?0:1;}
            if(a[0]=="--verify-embedded-release"&&a.Length==4){Json.New(a[3],EmbeddedRelease.Reconstruct(a[1],a[2],System.Threading.CancellationToken.None));return 0;}
            if(a[0]=="--embedded-install-fixture"&&a.Length==4){Json.New(a[3],EmbeddedInstallTests.Run(a[1],a[2]));return 0;}
            if(a[0]=="--shader-delta-install-fixture"&&a.Length==4){Json.New(a[3],EmbeddedInstallTests.Run(a[1],a[2],true));return 0;}
            if(a[0]=="--release-install-fixture"&&a.Length==5){Json.New(a[4],EmbeddedInstallTests.Run(a[1],a[2],true,a[3]));return 0;}
            if(a[0]=="--compatibility-mod-fixtures"&&a.Length==4){Json.New(a[3],CompatibilityTests.ModContracts(a[1],a[2]));return 0;}
            if(a[0]=="--compatibility-install-fixture"&&a.Length==6){Json.New(a[5],EmbeddedInstallTests.Run(a[1],a[2],true,a[3],a[4]));return 0;}
            if(a[0]=="--scroll-probe"&&a.Length==3){Json.New(a[2],ScrollProbe.Run(a[1]));return 0;}
            if(a[0]=="--bundle-fixtures"&&a.Length==4){Json.New(a[3],BundleFixtures.Run(a[1],a[2]));return 0;}
            if(a[0]=="--dlss-ui-probe"&&a.Length==4){Json.New(a[3],ScrollProbe.CheckDlss(a[1],a[2]));return 0;}
            if(a[0]=="--render"&&a.Length==5){Json.New(a[4],Preview.Render(a[1],a[2],a[3]));return 0;}
            if(a[0]=="--render-all"&&a.Length==3){Json.New(a[2],Preview.RenderAll(a[1]));return 0;}
            if(a[0]=="--dry-run"&&a.Length==3){Json.New(a[2],DryRun.Run(a[1],null));return 0;}
            if(a[0]=="--finish-update"&&a.Length==4)return SelfInstall.FinishUpdate(a[1],Int32.Parse(a[2]),a[3]=="restart");
            if(a[0]=="--publish-channel"&&a.Length==6){Json.New(a[5],SelfInstall.Publish(a[1],a[2],a[3],a[4]));return 0;}
            if(a[0]=="--smoke-window"&&a.Length==2){Json.New(a[1],SuiteApp.Smoke());return 0;}
            if(a[0]=="--self-install-test"&&a.Length==3){Json.New(a[2],SelfInstallTests.Run(a[1]));return 0;}
            if(a[0]=="--content-build"&&a.Length==4){Json.New(a[3],Content.ContentCli.Run(a[1],a[2]));return 0;}
            if(a[0]=="--content-options"&&a.Length==4){Json.New(a[3],Content.ContentCli.RunOptions(a[1],a[2]));return 0;}
            if(a[0]=="--render-library"&&a.Length==6){Json.New(a[5],Preview.RenderLibrary(a[1],a[2],a[3],a[4]));return 0;}
            if(a[0]=="--import-mods"&&a.Length>=3){Json.New(a[a.Length-1],ImportMods(a.Skip(1).Take(a.Length-2).ToList()));return 0;}
            // Read-only: is this game folder a Legacy 3889 game GTAVXR accepts, and what would wait aside at Play.
            if(a[0]=="--inspect-game"&&a.Length==3){
                object r;try{var g=LegacyGamePolicy.Inspect(a[1]);r=new{accepted=true,modified=g.modified,verified=g.verified,aside_at_play=RetailPlugins.Incompatible(a[1],null)};}
                catch(Exception e){r=new{accepted=false,error=e.Message};}
                Json.New(a[2],r);return 0;}
            // 2.4.4: the product rename of ScriptHookV.dll imports (ShvImports), for the script host workspace tests
            if(a[0]=="--rename-shv"&&a.Length==4){ShvRename rn;byte[] o=ShvImports.Rename(System.IO.File.ReadAllBytes(a[1]),out rn);System.IO.File.WriteAllBytes(a[2],o);Json.New(a[3],rn);return rn.error==null?0:1;}
            if(a[0]=="--runtime-report"&&(a.Length==2||a.Length==3)){
                var active=XrRuntimes.RegistryActive();
                var candidates=XrRuntimes.RegistryAvailable().AsEnumerable();
                if(a.Length==3)candidates=candidates.Concat(new[]{a[1]});
                var rows=XrRuntimes.List(active,candidates).Select(r=>new{
                    r.name,r.manifest,r.library,r.active,r.negotiation_export,
                    problem=XrRuntimes.NegotiationProblem(r)
                }).ToArray();
                Json.New(a[a.Length-1],new{status="STATIC_OPENXR_INSPECTION",suite=SuiteApp.Version,
                    runtimes=rows,runtime_negotiated=false,connection="not-measured",gta_launched=false});
                return 0;
            }
            if(a[0]=="--version"&&a.Length==2){Json.New(a[1],new{product="GTAVXR Suite",version=SuiteApp.Version,gta_launched=false});return 0;}
            throw new ArgumentException("Headless: --self-test FRESH RESULT | --render VIEW LOCALE PNG RESULT | --render-all FOLDER RESULT | --dry-run FRESH RESULT | --version RESULT");
        }
        // Headless: --import-mods PATH... RESULT. Imports content mods into the player's own Suite library (same code as Mods >
        // Add) and replaces the build 166 fixed packs in every profile. Refused while the Suite window or the game is open.
        private static object ImportMods(System.Collections.Generic.List<string> paths) {
            int me=System.Diagnostics.Process.GetCurrentProcess().Id;
            foreach(var p in System.Diagnostics.Process.GetProcessesByName("GTAVXR"))using(p)if(p.Id!=me)throw new InvalidOperationException("SUITE_RUNNING");
            var env=Env.Product(Env.DefaultDataRoot());
            if(new PlaySession(env).GameRunning())throw new InvalidOperationException("GAME_RUNNING");
            var imp=new Importers(env);var lib=new Library(env);
            var cards=new System.Collections.Generic.List<string>();
            foreach(string p in paths){var m=imp.ImportArchive(p);cards.Add(m.id+" "+m.name);}
            return new{status="IMPORTED",cards=cards,library=System.Linq.Enumerable.ToList(System.Linq.Enumerable.Select(lib.Mods(),m=>new{m.id,m.name,m.kind,card=m.IsCard()})),
                profiles=System.Linq.Enumerable.ToList(System.Linq.Enumerable.Select(lib.Profiles(),p=>new{p.name,p.mods})),gta_launched=false};
        }
        private static void WriteResult(string[] a,object result) {
            if(a.Length<2)return;string p=a[a.Length-1];
            try{if(Path.IsPathRooted(p)&&!File.Exists(p)&&Directory.Exists(Path.GetDirectoryName(p)))Json.New(p,result);}catch(Exception){}
        }
        public static string SelfPath {get{return Assembly.GetExecutingAssembly().Location;}}
    }
}
