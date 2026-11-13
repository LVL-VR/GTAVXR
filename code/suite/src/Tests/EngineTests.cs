using System;
using System.IO;
using System.Linq;
using System.Text;
using System.Diagnostics;
using System.Collections.Generic;

namespace GtavxrSuite {
    public class TestRow { public string name {get;set;} public bool pass {get;set;} public string detail {get;set;} public double seconds {get;set;} }
    public class TestReport { public string schema {get;set;} public int passed {get;set;} public int failed {get;set;} public List<TestRow> rows {get;set;} public bool gta_launched {get;set;} public bool real_game_touched {get;set;} }

    // Headless engine tests over an owned fixture world. The "game" is this same EXE in --fixture-game mode.
    public static class EngineTests {
        public static TestReport Run(string fresh){
            fresh=SafeFs.Root(fresh);if(Directory.Exists(fresh))throw new InvalidDataException("Fresh test folder required");
            Directory.CreateDirectory(fresh);
            var rows=new List<TestRow>();
            Action<string,Action> t=(name,body)=>{
                var sw=Stopwatch.StartNew();
                try{body();rows.Add(new TestRow{name=name,pass=true,seconds=sw.Elapsed.TotalSeconds});}
                catch(Exception e){rows.Add(new TestRow{name=name,pass=false,detail=e.ToString(),seconds=sw.Elapsed.TotalSeconds});}
            };
            t("save title parse",SaveTitle);
            t("display conversion",DisplayConvert);
            t("import ids",ImportIds);
            t("OpenXR capability: coexistence, renamed negotiation, wrong machine and path recovery",()=>RuntimeCapabilities(Path.Combine(fresh,"xr-caps")));
            t("OpenXR large library is admitted by its bounded export tables",()=>CompatibilityTests.LargeOpenXr(Path.Combine(fresh,"xr-large")));
            t("Platform requirements: native path buffer, ANSI, .NET and VC++",()=>CompatibilityTests.Platform(fresh));
            var w=new World(Path.Combine(fresh,"w1"));
            t("play stages profile and restores normal GTA",()=>w.PlayA());
            t("version switch installs only the difference + MVGA args",()=>w.PlayBWithMvga());
            t("MVGA off removes its files",()=>w.PlayBWithoutMvga());
            t("foreign game file is never overwritten",()=>w.ForeignConflict());
            t("an agent's play lease never stops the user's Play",()=>w.BusyLease());
            t("recover before launch restores normal GTA only",()=>w.RecoverBeforeLaunch());
            t("recover after a crash keeps the progress",()=>w.RecoverAfterLaunch());
            t("recover when start was requested before the process receipt keeps progress",()=>w.RecoverAfterLaunch(true));
            t("recover an unconfirmed start without new progress preserves the profile",()=>w.RecoverAfterLaunch(true,true));
            t("interrupted game change finishes on recover",()=>w.RecoverLayer());
            t("a version re-imported under its id installs its new files",()=>w.SameIdNewFiles());
            t("profile copy, new story and delete to recycle",()=>w.ProfileOps());
            t("older build swaps game files only while playing",()=>w.PlaySwapping());
            t("crash while swapped: recover puts the original back",()=>w.CrashSwapping());
            t("check files repairs a damaged GTAVXR file",()=>w.RepairFiles());
            t("uninstall leaves only the game's own files",()=>w.Uninstall());
            var w3=new World(Path.Combine(fresh,"w3"));
            t("test flight sees a same-size, same-date edit on real NTFS; v1 inventories keep size/date",()=>FlightIdentity(fresh));
            t("test flight: game back as before, new files aside, changed files reported",()=>w3.FlightStray());
            t("test flight abort ends only our game and restores everything",()=>w3.FlightAbort());
            t("game stuck after its exit file is closed by the Suite, stacks kept",()=>w3.FlightExitHang());
            t("Suite dies during a test flight: recover puts the game back",()=>w3.FlightCrash());
            t("test flight that cannot start puts the game back at once",()=>w3.FlightStartFails());
            t("undo a test session, back to the starting point",()=>w3.FlightUndo());
            t("pass to my profile copies version + mods and marks them tested",()=>w3.FlightPromote());
            var w2=new World(Path.Combine(fresh,"w2"));
            t("release import: recorded arguments + MVGA split",()=>w2.ImportRelease());
            t("archive mod import is kept but not loadable",()=>w2.ArchiveMod(fresh));
            var w4=new World(Path.Combine(fresh,"w4"));
            t("content mods: import cards, options plan, built profile reused at Play, store stays protected",()=>w4.ContentMods());
            var w5=new World(Path.Combine(fresh,"w5"));
            t("build 166 fixed packs become separate mod cards in every profile; loose pack / folder import",()=>w5.ProjectPacks());
            t("variety option in settings.xml and VR ini starting values",ContentSettings);
            var w6=new World(Path.Combine(fresh,"w6"));
            t("store-game mods never block Play; only what GTAVXR cannot run waits aside and goes back, also after a crash",()=>w6.RetailMods());
            var w7=new World(Path.Combine(fresh,"w7"));
            t("script mods: one card + OpenIV companion; a script host session swaps scripts/ per profile, captures, recovers",()=>w7.ScriptModPlay());
            t("ScriptHookV names in a mod's copy: native imports and .NET DllImport renamed in place, nothing else changes",()=>ShvRenameFixture(fresh));
            var w8=new World(Path.Combine(fresh,"w8"));
            t(".asi mods: import, version check, game-folder session beside the player's files, capture, crash recover, conflicts listed before Play",()=>w8.AsiModPlay());
            t("script mod ini: one key set in its section, line ends and other lines kept",IniSet);
            t("game start: an unreadable process image is retried, a different image fails at once",ImageRetry);
            t("gameconfig pools above the EXE's 16-bit limit are lowered and reported",PoolClamp);
            t("Legacy 3889 check: plugins accepted, archive rewritten by a mod tool accepted and noted, non-archive and other executable refused",()=>LegacyGamePolicy.Fixture(Path.Combine(fresh,"policy")));
            NeuralSuiteTests.Add(t,Path.Combine(fresh,"neural"));
            return new TestReport{schema="gtavxr-suite2-tests-v1",rows=rows,passed=rows.Count(r=>r.pass),failed=rows.Count(r=>!r.pass),gta_launched=false,real_game_touched=false};
        }

        static void RuntimeCapabilities(string root){
            Directory.CreateDirectory(root);
            string normal=Path.Combine(root,"normal.json"),alternate=Path.Combine(root,"révision.json");
            string first=Path.Combine(root,"old.dll"),second=Path.Combine(root,"renamed.dll");
            const string standard="xrNegotiateLoaderRuntimeInterface",changed="VendorNegotiation";
            NeuralSuiteTests.FakeDll(first,"Fixture","OpenXR","1.0.0.0",new[]{standard});
            NeuralSuiteTests.FakeDll(second,"Fixture","OpenXR","1.0.0.1",new[]{changed});
            Json.New(normal,new{runtime=new{name="First",library_path="old.dll"}});
            Json.New(alternate,new{runtime=new{name="Second",library_path=second,functions=new{xrNegotiateLoaderRuntimeInterface=changed}}});
            Check(XrRuntimes.List(normal,new[]{alternate,normal}).Count==2,"coexistence without duplicate runtime");
            Check(XrRuntimes.NegotiationProblem(XrRuntimes.Resolve(XrRuntimes.Auto,normal))==null,"default ABI");
            Check(XrRuntimes.NegotiationProblem(XrRuntimes.Resolve(alternate,normal))==null,"compatible external revision with remap");
            var wrong=File.ReadAllBytes(second);int header=BitConverter.ToInt32(wrong,0x3c);wrong[header+4]=0x64;wrong[header+5]=0xaa;
            File.WriteAllBytes(second,wrong);
            Check(XrRuntimes.NegotiationProblem(XrRuntimes.Resolve(alternate,normal))=="XR_LIBRARY_NOT_X64","ARM64 is not x64 PE");
            File.Delete(second);
            Check(XrRuntimes.Resolve(alternate,normal).error=="XR_LIBRARY_MISSING","missing explicit runtime does not silently change runtime");
            Check(XrRuntimes.NegotiationProblem(XrRuntimes.Resolve(XrRuntimes.Auto,normal))==null,"explicit recovery to Auto");
        }

        static void Check(bool c,string what){if(!c)throw new Exception("expected: "+what);}
        static void W16(byte[] b,int at,int v){BitConverter.GetBytes((ushort)v).CopyTo(b,at);}
        static void W32(byte[] b,int at,uint v){BitConverter.GetBytes(v).CopyTo(b,at);}
        static void Str(byte[] b,int at,string s){Encoding.ASCII.GetBytes(s).CopyTo(b,at);}
        // 2.4.4: a minimal 64-bit .asi (PE32+, one section at RVA 0x1000 = file 0x200): imports KERNEL32.dll + ScriptHookV.dll (or
        // USER32.dll), delay-imports ScriptHookV.dll (or WINMM.dll) + USER32.dll. "mention" adds texts in its data: 2.4.5 whole
        // by-name lookups "ScriptHookV.dll" (ASCII, RVA 0x1180) and L"ScriptHookV" (UTF-16, 0x1200), "ScriptHookVDotNet3.dll"
        // (0x11C0, not ours) and "Install ScriptHookV first" (0x1240, a longer text that stays).
        internal static byte[] FakeAsi(bool shv,bool mention){
            var b=new byte[0x600];
            b[0]=(byte)'M';b[1]=(byte)'Z';W32(b,0x3C,0x40);
            W32(b,0x40,0x00004550);W16(b,0x44,0x8664);W16(b,0x46,1);W16(b,0x54,0xF0);W16(b,0x56,0x2022);
            int opt=0x58;W16(b,opt,0x20B);BitConverter.GetBytes(0x180000000UL).CopyTo(b,opt+24);W32(b,opt+108,16);
            int dirs=opt+112;W32(b,dirs+8,0x1000);W32(b,dirs+12,60);W32(b,dirs+13*8,0x1080);W32(b,dirs+13*8+4,96);
            int sec=opt+0xF0;Str(b,sec,".rdata");W32(b,sec+8,0x400);W32(b,sec+12,0x1000);W32(b,sec+16,0x400);W32(b,sec+20,0x200);
            Func<int,int> off=rva=>rva-0x1000+0x200;
            W32(b,off(0x1000)+12,0x1100);W32(b,off(0x1014)+12,0x1110);
            W32(b,off(0x1080),1);W32(b,off(0x1080)+4,0x1120);W32(b,off(0x10A0),1);W32(b,off(0x10A0)+4,0x1130);
            Str(b,off(0x1100),"KERNEL32.dll");Str(b,off(0x1110),shv?"ScriptHookV.dll":"USER32.dll");
            Str(b,off(0x1120),shv?"ScriptHookV.dll":"WINMM.dll");Str(b,off(0x1130),"USER32.dll");
            if(mention){Str(b,off(0x1180),"ScriptHookV.dll");Str(b,off(0x11C0),"ScriptHookVDotNet3.dll");
                Encoding.Unicode.GetBytes("ScriptHookV").CopyTo(b,off(0x1200));Str(b,off(0x1240),"Install ScriptHookV first");}
            return b;
        }
        // 2.4.4: a real .NET assembly like a ScriptHookVDotNet script calling ScriptHookV.dll directly (DllImport), plus a kernel32 call.
        // 2.4.5 "texts": methods returning the whole texts "ScriptHookV.dll" / "ScriptHookV" (ldstr, a by-name lookup) and a longer
        // message "Install ScriptHookV first".
        internal static byte[] ManagedShv(string dir,bool texts){
            Directory.CreateDirectory(dir);
            var ab=AppDomain.CurrentDomain.DefineDynamicAssembly(new System.Reflection.AssemblyName("ShvCaller"),System.Reflection.Emit.AssemblyBuilderAccess.Save,dir);
            var mb=ab.DefineDynamicModule("ShvCaller","ShvCaller.dll");
            var tb=mb.DefineType("ShvCaller.Native",System.Reflection.TypeAttributes.Public|System.Reflection.TypeAttributes.Abstract|System.Reflection.TypeAttributes.Sealed);
            var ma=System.Reflection.MethodAttributes.Public|System.Reflection.MethodAttributes.Static|System.Reflection.MethodAttributes.PinvokeImpl;
            tb.DefinePInvokeMethod("getGameVersion","ScriptHookV.dll",ma,System.Reflection.CallingConventions.Standard,typeof(int),Type.EmptyTypes,System.Runtime.InteropServices.CallingConvention.Cdecl,System.Runtime.InteropServices.CharSet.Ansi);
            tb.DefinePInvokeMethod("GetTickCount","kernel32.dll",ma,System.Reflection.CallingConventions.Standard,typeof(uint),Type.EmptyTypes,System.Runtime.InteropServices.CallingConvention.Winapi,System.Runtime.InteropServices.CharSet.Ansi);
            if(texts)foreach(var kv in new[]{new[]{"Name","ScriptHookV.dll"},new[]{"Short","ScriptHookV"},new[]{"Message","Install ScriptHookV first"}}){
                var m=tb.DefineMethod(kv[0],System.Reflection.MethodAttributes.Public|System.Reflection.MethodAttributes.Static,typeof(string),Type.EmptyTypes);
                var il=m.GetILGenerator();il.Emit(System.Reflection.Emit.OpCodes.Ldstr,kv[1]);il.Emit(System.Reflection.Emit.OpCodes.Ret);
            }
            tb.CreateType();ab.Save("ShvCaller.dll");
            return File.ReadAllBytes(Path.Combine(dir,"ShvCaller.dll"));
        }
        static void ShvRenameFixture(string fresh){
            ShvRename rn;
            byte[] a=FakeAsi(true,true);byte[] o=ShvImports.Rename(a,out rn);
            Check(rn.pe&&!rn.managed&&rn.native_imports==2&&rn.name_strings==2&&rn.error==null&&o.Length==a.Length,"import + delay import + 2 whole texts renamed, got "+rn.native_imports+"/"+rn.name_strings+" "+rn.error);
            string s=Encoding.ASCII.GetString(o);
            Check(s.Substring(0x310,16)=="GTAVXRHost.dll\0\0"&&s.Substring(0x320,16)=="GTAVXRHost.dll\0\0"&&s.Substring(0x300,13)=="KERNEL32.dll\0"&&s.Substring(0x330,11)=="USER32.dll\0","names written in place");
            Check(s.Substring(0x380,16)=="GTAVXRHost.dll\0\0"&&Encoding.Unicode.GetString(o,0x400,24)=="GTAVXRHost\0\0"&&s.Substring(0x3C0,23)=="ScriptHookVDotNet3.dll\0"&&s.Substring(0x440,26)=="Install ScriptHookV first\0",
                "whole texts renamed (ASCII and UTF-16), ScriptHookVDotNet3 and the longer message kept");
            for(int i=0;i<a.Length;i++)if(a[i]!=o[i]&&!(i>=0x310&&i<0x330)&&!(i>=0x380&&i<0x38F)&&!(i>=0x400&&i<0x416))throw new Exception("expected: only the names change, byte "+i.ToString("X"));
            Check(rn.runtime_mentions==1,"the longer message is noted, ScriptHookVDotNet3 is not, got "+rn.runtime_mentions);
            Check(a[0x310]==(byte)'S',"the input array is never changed");
            byte[] none=FakeAsi(false,false);
            Check(Object.ReferenceEquals(ShvImports.Rename(none,out rn),none)&&!rn.Renamed&&rn.pe&&rn.error==null,"nothing to rename: the same bytes");
            // 2.4.5: a plugin that only finds ScriptHookV.dll by name (no import) is renamed too
            byte[] dyn=FakeAsi(false,true);ShvImports.Rename(dyn,out rn);
            Check(rn.Renamed&&rn.native_imports==0&&rn.name_strings==2&&rn.runtime_mentions==1,"a by-name-only plugin is renamed, got "+rn.name_strings);
            // a text that only contains the name (no zero before it) is not a whole text
            byte[] part=FakeAsi(false,false);Str(part,0x380,"xScriptHookV.dll");ShvImports.Rename(part,out rn);
            Check(!rn.Renamed&&rn.runtime_mentions==1,"part of a longer text stays");
            byte[] bad=FakeAsi(true,false);W32(bad,0x58+112+8,0x5000);
            Check(Object.ReferenceEquals(ShvImports.Rename(bad,out rn),bad)&&rn.error!=null&&!rn.Renamed,"a broken table: original bytes, error noted");
            byte[] text=Encoding.ASCII.GetBytes("not a PE file at all ScriptHookV.dll");
            Check(Object.ReferenceEquals(ShvImports.Rename(text,out rn),text)&&!rn.pe,"not a PE: untouched");
            // a real .NET script: only the ScriptHookV ModuleRef changes and the copy still loads
            byte[] m=ManagedShv(Path.Combine(fresh,"emit"),true);byte[] mo=ShvImports.Rename(m,out rn);
            Check(rn.managed&&rn.module_refs==1&&rn.name_strings==2&&rn.native_imports==0&&!rn.strong_named&&rn.error==null&&rn.runtime_mentions==1,
                "one ModuleRef and two whole user strings renamed, the message noted, got "+rn.module_refs+"/"+rn.name_strings+"/"+rn.runtime_mentions+" "+rn.error);
            var ty=System.Reflection.Assembly.Load(mo).GetType("ShvCaller.Native");
            Func<string,string> call=n=>(string)ty.GetMethod(n).Invoke(null,null);
            Check(call("Name")=="GTAVXRHost.dll"&&call("Short")=="GTAVXRHost"&&call("Message")=="Install ScriptHookV first",
                "the renamed copy's code returns exactly GTAVXRHost.dll / GTAVXRHost (no padding), the message unchanged, got "+call("Name")+"|"+call("Short")+"|"+call("Message"));
            Func<string,string> dll=n=>((System.Runtime.InteropServices.DllImportAttribute)ty.GetMethod(n).GetCustomAttributes(typeof(System.Runtime.InteropServices.DllImportAttribute),false).Single()).Value;
            Check(dll("getGameVersion")=="GTAVXRHost.dll"&&dll("GetTickCount")=="kernel32.dll","DllImport names GTAVXRHost.dll, kernel32 untouched, got "+dll("getGameVersion")+"/"+dll("GetTickCount"));
            // the Suite itself (a real assembly with Windows DllImports): its metadata parses, no ModuleRef renamed (its own code holds
            // the texts "ScriptHookV.dll" / "ScriptHookV" as user strings: those count, it is never run renamed)
            byte[] self=File.ReadAllBytes(Program.SelfPath);ShvImports.Rename(self,out rn);
            Check(rn.managed&&rn.error==null&&rn.module_refs==0&&rn.native_imports==0,"the Suite EXE parses, no import renamed, got "+rn.error);
        }
        static void FlightIdentity(string fresh){
            string dir=Path.Combine(fresh,"identity");Directory.CreateDirectory(dir);string f=Path.Combine(dir,"data.rpf");
            File.WriteAllText(f,"ORIGINAL DATA 01");var when=File.GetLastWriteTimeUtc(f);
            var before=TestFlight.Take(dir,null).files.Single();
            Check(before.ctime!=0&&before.fid!=null,"NTFS change time and file id read");
            Check(!TestFlight.Changed(before,TestFlight.Take(dir,null).files.Single()),"untouched file not reported");
            System.Threading.Thread.Sleep(50);File.WriteAllText(f,"ORIGINAL DATA 02");File.SetLastWriteTimeUtc(f,when);
            var after=TestFlight.Take(dir,null).files.Single();
            Check(after.bytes==before.bytes&&after.mtime==before.mtime,"same size and same visible date");
            Check(TestFlight.Changed(before,after),"same-size, same-date edit reported");
            var v1=new InventoryFile{path=before.path,bytes=before.bytes,mtime=before.mtime};
            Check(!TestFlight.Changed(v1,after),"v1 inventory (no change time) compares size/date only");
            string other=Path.Combine(dir,"other.rpf");File.WriteAllText(other,"ORIGINAL DATA 03");File.SetLastWriteTimeUtc(other,when);
            File.Delete(f);File.Move(other,f);
            var replaced=TestFlight.Take(dir,null).files.Single();
            Check(replaced.fid!=after.fid&&TestFlight.Changed(after,replaced),"replaced file reported");
        }
        public static byte[] FakeSave(string title,int size){
            var b=new byte[size];var t=Encoding.Unicode.GetBytes(title);Array.Copy(t,0,b,4,Math.Min(t.Length,0x200));return b;
        }
        static void SaveTitle(){
            string p=Path.GetTempFileName();
            try{
                File.WriteAllBytes(p,FakeSave("(Autoguardado) Cambios (22.5%) - 10/01/26 05:03:45",4096));
                var s=SaveInfo.Read(p);Check(s!=null&&s.mission=="Cambios"&&Math.Abs(s.percent-22.5)<0.001&&s.autosave,"Cambios 22.5 autosave");
                Check(s.Short("es")=="Cambios · 22,5 %","spanish short form, got "+s.Short("es"));
                File.WriteAllBytes(p,FakeSave("Lamar Down (57.3%) - 1/2/26",4096));s=SaveInfo.Read(p);Check(s!=null&&s.mission=="Lamar Down"&&!s.autosave,"manual save");
                File.WriteAllBytes(p,new byte[100]);Check(SaveInfo.Read(p)==null,"short file ignored");
            }finally{File.Delete(p);}
        }
        public const string SettingsXml="<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n<Settings>\r\n  <video>\r\n    <ScreenWidth value=\"3840\" />\r\n    <ScreenHeight value=\"2160\" />\r\n    <Windowed value=\"0\" />\r\n    <Stereo value=\"1\" />\r\n    <Other value=\"7\" />\r\n  </video>\r\n</Settings>";
        static void DisplayConvert(){
            string o=Display.Convert(SettingsXml);
            Check(o.Contains("<ScreenWidth value=\"1920\" />")&&o.Contains("<ScreenHeight value=\"1080\" />")&&o.Contains("<Windowed value=\"1\" />")&&o.Contains("<Stereo value=\"0\" />")&&o.Contains("<Other value=\"7\" />"),"four fields set, others kept: "+o);
            bool threw=false;try{Display.Convert("<Settings><video><ScreenWidth value=\"1\"/></video></Settings>");}catch(InvalidDataException){threw=true;}Check(threw,"missing field rejected");
        }
        static void ImportIds(){
            Check(Importers.IdFor("native-xr-build125-dual-v1")=="125-dual","125-dual");
            Check(Importers.IdFor("native-xr-build155-mvga265-v1")=="155","155");
            Check(Importers.TitleOf("native-xr-build125-dual-v1")=="Dual","Dual");
            var two=new VersionManifest{title="Vehículos",title_en="Vehicles",notes=new List<string>{"Puertas"},notes_en=new List<string>{"Doors"}};
            var one=new VersionManifest{title="Panels",notes=new List<string>{"Panels by hand"}};
            Check(two.Title("es")=="Vehículos"&&two.Title("en")=="Vehicles"&&two.Notes("en")[0]=="Doors"&&two.Notes("es")[0]=="Puertas","title and notes in the player's language");
            Check(one.Title("es")=="Panels"&&one.Notes("es")[0]=="Panels by hand","one language only: shown in both");
        }

        static void ContentSettings(){
            string xml="<graphics>\r\n    <PedVarietyMultiplier value=\"0.800000\" />\r\n    <VehicleVarietyMultiplier value=\"2.000000\" />\r\n</graphics>";
            string game=ContentSources.ApplySettings(xml,new Dictionary<string,string>());
            Check(game.Contains("<PedVarietyMultiplier value=\"0.800000\" />")&&game.Contains("<VehicleVarietyMultiplier value=\"1.000000\" />"),"Game keeps the player's value, a value above 1.0 goes back to 1.0");
            string set=ContentSources.ApplySettings(xml,new Dictionary<string,string>{{"PedVarietyMultiplier","1.500000"},{"VehicleVarietyMultiplier","1.500000"}});
            Check(set.Contains("<PedVarietyMultiplier value=\"1.500000\" />")&&set.Contains("<VehicleVarietyMultiplier value=\"1.500000\" />"),"1.5 set");
            string ini="[VR]\r\nhud=1\r\n\r\n[Logs]\r\nhitches=0\r\n";
            string a=VrIni.Set(ini,"ped_density","70");
            Check(a=="[VR]\r\nhud=1\r\nped_density=70\r\n\r\n[Logs]\r\nhitches=0\r\n","new key inside [VR], got "+a.Replace("\r\n","|"));
            Check(VrIni.Get(VrIni.Set(a,"ped_density","40"),"ped_density")=="40"&&VrIni.Get(a,"hitches")==null,"key replaced; other sections not read");
            Check(VrIni.Set("","x","1")=="[VR]\r\nx=1\r\n","empty file gets [VR]");
        }
        static void ImageRetry(){
            string exe=@"C:\Games\GTAV\GTAVXR_SourceBaseline.exe";int calls=0;
            ProcessOwnership.ReadImage(()=>{if(++calls<3)throw new System.ComponentModel.Win32Exception(299);return exe;},()=>false,exe,5000,10);
            Check(calls==3,"read again until Windows lets it, got "+calls);
            calls=0;bool threw=false;
            try{ProcessOwnership.ReadImage(()=>{calls++;return @"C:\Other\game.exe";},()=>false,exe,5000,10);}catch(InvalidDataException){threw=true;}
            Check(threw&&calls==1,"another image fails at once");
            threw=false;
            try{ProcessOwnership.ReadImage(()=>{throw new System.ComponentModel.Win32Exception(299);},()=>false,exe,100,10);}catch(IOException){threw=true;}
            Check(threw,"never readable: fails after the wait");
            calls=0;bool done=false;
            ProcessOwnership.ReadImage(()=>{calls++;throw new System.ComponentModel.Win32Exception(299);},()=>{done=calls>0;return done;},exe,5000,10);
            Check(calls==1,"a game that already closed ends the wait");
        }
        static void PoolClamp(){
            string xml="<Item>\r\n <PoolName>MapDataStore</PoolName>\r\n <PoolSize value=\"155020\" />\r\n</Item>\r\n<Item>\r\n <PoolName>StaticBounds</PoolName>\r\n <PoolSize value=\"46500\" />\r\n</Item>\r\n<Item>\r\n <PoolName>DrawableStore</PoolName>\r\n <PoolSize value=\"400000\" />\r\n</Item>";
            var changed=new List<string>();string o=Content.ContentBuilder.ClampPools(xml,changed);
            Check(o.Contains("<PoolName>MapDataStore</PoolName>\r\n <PoolSize value=\"65534\" />")&&o.Contains("<PoolSize value=\"46500\" />")&&o.Contains("<PoolSize value=\"400000\" />"),"only MapDataStore lowered, got "+o.Replace("\r\n","|"));
            Check(changed.Count==1&&changed[0].StartsWith("PoolSize MapDataStore 155020 -> 65534",StringComparison.Ordinal),"reported");
            Check(Content.ContentBuilder.ClampPools(o,new List<string>())==o,"a value within the limit is left alone");
        }
        static void IniSet(){
            string a="[Settings]\r\nGym Blips=1\r\nGym Radio=RADIO_02_POP";
            string b=Ini.Set(a,"Settings","gym blips","0");
            Check(b=="[Settings]\r\nGym Blips=0\r\nGym Radio=RADIO_02_POP"||b=="[Settings]\r\ngym blips=0\r\nGym Radio=RADIO_02_POP","existing key set, no line end added, got "+b.Replace("\r\n","|"));
            string c=Ini.Set("[Other]\nx=1\n\n[Settings]\nA=1\n","Settings","Nightclub Radio","RADIO_20_THELAB");
            Check(c=="[Other]\nx=1\n\n[Settings]\nA=1\nNightclub Radio=RADIO_20_THELAB\n","new key at the end of its section, LF kept, got "+c.Replace("\n","|"));
            Check(Ini.Get(c,"Other","Nightclub Radio")==null&&Ini.Get(c,"Settings","nightclub radio")=="RADIO_20_THELAB","read by section");
            Check(Ini.Set("","Settings","A","1")=="[Settings]\nA=1","empty file gets the section");
        }

        // ---------------- fixture world ----------------
        sealed class World {
            public readonly Env env;readonly Library lib;readonly Blobs blobs;readonly string root;
            VersionManifest v1,v2;ModManifest mvga;Profile a,b;
            public World(string fresh){
                root=fresh;env=Env.ForFixture(fresh);lib=new Library(env);blobs=new Blobs(env);
                env.GameProcessNames=new[]{"GTAVXR_FixtureGame"};
                // Game folder with the three retail files (fixture bytes) and one file of the game itself.
                Directory.CreateDirectory(Path.Combine(env.GameDir,"update"));
                var retail=new Dictionary<string,string>();
                foreach(string n in new[]{"GTA5.exe","update/update.rpf","update/update2.rpf"}){string p=SafeFs.Under(env.GameDir,n);File.WriteAllText(p,"FIXTURE "+n);retail[n]=SafeFs.Hash(p);}
                File.WriteAllText(Path.Combine(env.GameDir,"x64a.rpf"),"FIXTURE GAME DATA");
                env.RetailOverride=retail;
                // Normal GTA user folder.
                Write(env.UserDataDir,"settings.xml",Encoding.UTF8.GetBytes(SettingsXml));
                Write(env.UserDataDir,"Profiles/ABCD/SGTA50000",FakeSave("Franklin and Lamar (2.0%)",8192));
                Write(env.UserDataDir,"launcher.log",Encoding.UTF8.GetBytes("normal"));
                File.WriteAllText(env.LegacySettingsPath,"[VR]\nnormal=1\n");
                File.WriteAllText(Path.Combine(Path.GetDirectoryName(env.OpenXrManifest),"fixture_runtime.dll"),"fixture runtime");
                File.WriteAllText(env.OpenXrManifest,"{\"file_format_version\":\"1.0.0\",\"runtime\":{\"name\":\"Fixture XR\",\"library_path\":\"fixture_runtime.dll\"}}");
                // Versions built from fixture blobs. The game EXE is this Suite EXE (fixture-game mode).
                string src=Path.Combine(root,"src");Directory.CreateDirectory(src);
                var exe=blobs.PutFile(Program.SelfPath,"GTAVXR_FixtureGame.exe");
                var c1=Put(src,"common/data/one.txt","one-v1");var c2=Put(src,"common/data/one.txt","one-v2");var shared=Put(src,"common/shaders/s.fxc","shader");
                var only1=Put(src,"common/data/only125.txt","only in 1");
                var bridge=Put(src,"gtavxr_native_openxr.dll","bridge");var trainer=Put(src,"gtavxr_trainer.dll","trainer");
                Func<string,string,List<FileRef>,string,VersionManifest> mk=(id,ch,files,m)=>new VersionManifest{id=id,build=id.Substring(0,3),title="Fixture "+id,channel=ch,added_utc=DateTime.UtcNow.ToString("o"),notes=new List<string>(),
                    retail=retail,files=files,runtime=new List<FileRef>{bridge,trainer},mvga=m,mvga_arg=m=="switch"?"-gtavxrMvga":null,
                    launch=new LaunchContract{exe="GTAVXR_FixtureGame.exe",args=new List<string>{"--fixture-game","{LOGFILE}","-gtavxrNativeXR"},disable_layers=new List<string>{"DISABLE_XR_APILAYER_fixture"},session_mode="legacy-repo",settings_mode="legacy-repo",display="windowed1920x1080_stereo0",residual="exact-one-thread"}};
                v1=mk("125-dual","stable",new List<FileRef>{exe,c1,shared,only1},"none");
                v2=mk("155v2","test",new List<FileRef>{exe,c2,shared},"switch");
                v2.vr_settings_seed=Put(src,"vr-settings.ini","[VR]\nseed=155\n");
                lib.SaveVersion(v1);lib.SaveVersion(v2);
                mvga=new ModManifest{id="mvga-2-65-vr",name="Make Visuals Great Again",version="2.65",category="graphics",loadable=true,kind="mvga-native",family="mvga",
                    files=new List<FileRef>{Put(src,"gtavxr_mvga147/common/data/timecycle.xml","mvga tc"),Put(src,"gtavxr_mvga147/platform/textures/x.ytd","mvga tex")},args=new List<string>{"-gtavxrMvga"}};
                lib.SaveMod(mvga);
                // Two profiles, each with its own saves and VR settings.
                var imp=new Importers(env);
                string ua=Path.Combine(root,"ua");Write(ua,"settings.xml",Encoding.UTF8.GetBytes(SettingsXml));Write(ua,"Default/SGTA50015",FakeSave("(Autoguardado) Cambios (22.5%) - x",8192));
                string ia=Path.Combine(root,"a.ini");File.WriteAllText(ia,"[VR]\nprofile=A\n");
                a=imp.CreateFrom("Historia VR","125-dual",ua,UserTree.Snapshot(ua),ia,"test");
                string ub=Path.Combine(root,"ub");Write(ub,"settings.xml",Encoding.UTF8.GetBytes(SettingsXml));
                b=imp.CreateFrom("Prueba 155","155v2",ub,UserTree.Snapshot(ub),null,"test");
            }
            static void Write(string dir,string rel,byte[] bytes){string p=SafeFs.Under(dir,rel);Directory.CreateDirectory(Path.GetDirectoryName(p));File.WriteAllBytes(p,bytes);}
            FileRef Put(string src,string rel,string text){string p=Path.Combine(src,Guid.NewGuid().ToString("N"));File.WriteAllText(p,text);return blobs.PutFile(p,rel);}
            List<SaveFileRef> Normal(){return UserTree.Snapshot(env.UserDataDir);}
            string NormalVr(){return File.ReadAllText(env.LegacySettingsPath);}
            static string Sig(List<SaveFileRef> l){return String.Join("|",l.Select(f=>f.relative+":"+f.sha256));}
            Dictionary<string,object> Report(SessionRecord r){return Json.Read<Dictionary<string,object>>(Path.Combine(r.native_session,"fixture-report.json"));}

            public void PlayA(){
                string before=Sig(Normal()),vrBefore=NormalVr();
                var r=new PlaySession(env).Play(a.id,null);
                Check(r.phase=="CLOSED"&&r.result=="played"&&r.exit_code==0,"closed played exit 0, got "+r.phase+"/"+r.result+"/"+r.exit_code);
                var rep=Report(r);
                Check((string)rep["save_seen"]=="(Autoguardado) Cambios (22.5%) - x","game saw profile A's save, got "+rep["save_seen"]);
                Check((string)rep["vr_seen"]=="[VR]\nprofile=A\n","game saw profile A's VR settings");
                Check(((string)rep["settings_seen"]).Contains("<Windowed value=\"1\" />"),"windowed settings during play");
                Check((string)rep["user_play"]=="1"&&((string)rep["session_dir"]).StartsWith(env.LegacySessionsRoot,StringComparison.OrdinalIgnoreCase),"native env");
                Check((string)rep["layer"]=="1","implicit layer disabled");
                Check(((string)rep["bridge"]).EndsWith(@"runtime\125-dual\gtavxr_native_openxr.dll",StringComparison.OrdinalIgnoreCase),"bridge from Suite runtime folder");
                Check(!((string)rep["args"]).Contains("-gtavxrMvga"),"no MVGA arg on 125");
                Check(!((string)rep["args"]).Contains("-gtavxrHeapMB"),"no game heap argument without a content profile");
                Check(Sig(Normal())==before&&NormalVr()==vrBefore,"normal GTA user folder and VR settings restored exactly");
                var pa=lib.Profile(a.id);var save=SaveInfo.Latest(lib.UserDir(a.id));
                Check(save!=null&&save.mission=="Prueba"&&Math.Abs(save.percent-50)<0.01,"new progress kept in the profile");
                Check(File.ReadAllText(lib.VrSettingsFile(a.id))=="[VR]\nprofile=A\nchanged=1\n","VR settings changed in game kept in the profile");
                Check(pa.last_played_utc!=null,"last played set");
                var st=new GameLayer(env).State();Check(st.version=="125-dual"&&File.Exists(Path.Combine(env.GameDir,"common","data","only125.txt")),"125 installed");
                Check(File.Exists(Path.Combine(env.GameDir,"x64a.rpf")),"game file untouched");
            }
            public void PlayBWithMvga(){
                var pb=lib.Profile(b.id);pb.mods=new List<string>{mvga.id};lib.SaveProfile(pb);
                var plan=new GameLayer(env).Preview(v2,new List<ModManifest>{mvga});
                Check(plan.add==3&&plan.remove==1&&plan.keep==2,"diff plan add3 remove1 keep2, got "+plan.add+"/"+plan.remove+"/"+plan.keep);
                var r=new PlaySession(env).Play(b.id,null);
                Check(((string)Report(r)["args"]).Contains("-gtavxrMvga"),"MVGA arg present");
                Check(File.Exists(Path.Combine(env.GameDir,"gtavxr_mvga147","platform","textures","x.ytd"))&&!File.Exists(Path.Combine(env.GameDir,"common","data","only125.txt")),"155 + MVGA installed, 125-only removed");
                Check(File.ReadAllText(Path.Combine(env.GameDir,"common","data","one.txt"))=="one-v2","changed file replaced");
                Check(lib.Profile(b.id).has_vr_settings&&File.ReadAllText(lib.VrSettingsFile(b.id))=="[VR]\nseed=155\nchanged=1\n","profile B started from the version seed and kept its change");
            }
            public void PlayBWithoutMvga(){
                var pb=lib.Profile(b.id);pb.mods=new List<string>();lib.SaveProfile(pb);
                var r=new PlaySession(env).Play(b.id,null);
                Check(!((string)Report(r)["args"]).Contains("-gtavxrMvga"),"no MVGA arg");
                Check(!Directory.Exists(Path.Combine(env.GameDir,"gtavxr_mvga147")),"MVGA folder removed");
                Check(new PlaySession(env).Play(b.id,null).result=="played","second play without changes");
            }
            public void ForeignConflict(){
                // Someone else puts a different file where 125 needs one.
                string p=Path.Combine(env.GameDir,"common","data","only125.txt");File.WriteAllText(p,"FOREIGN");
                string before=Sig(Normal());
                bool threw=false;try{new PlaySession(env).Play(a.id,null);}catch(GameConflictException){threw=true;}
                Check(threw,"conflict reported");Check(File.ReadAllText(p)=="FOREIGN","foreign file kept");
                Check(new GameLayer(env).State().version=="155v2"&&!new GameLayer(env).Pending,"state unchanged");
                Check(Sig(Normal())==before,"user folder untouched");
                var last=new PlaySession(env).Last();Check(last.phase=="CLOSED"&&last.result=="cancelled","session closed as cancelled");
                File.Delete(p);
            }
            public void BusyLease(){
                Directory.CreateDirectory(Path.Combine(env.DevRepo,"work"));File.WriteAllText(Path.Combine(env.DevRepo,"work","AGENT_LEASE.json"),"{\"schema\":\"gtavxr-agent-lease-v1\"}");
                string locks=Path.Combine(env.DevRepo,"local-cache","agent-locks");Directory.CreateDirectory(locks);
                string lease=Path.Combine(locks,"play-runtime.lease");
                File.WriteAllText(lease,"{\"agent\":\"claude\",\"resource\":\"play-runtime\",\"purpose\":\"Build156\",\"acquired_utc\":\"x\",\"host_pid\":1,\"machine\":\"m\"}");
                string foreign=File.ReadAllText(lease);
                var busy=new PlaySession(env).Play(a.id,null);
                Check(busy.result=="played"&&busy.phase=="CLOSED","the user plays while an agent holds play-runtime");
                Check(busy.lease_held_by=="claude: Build156"&&!busy.lease,"the agent's lease is only noted");
                Check(File.ReadAllText(lease)==foreign,"foreign lease untouched");
                File.Delete(lease);
                var r=new PlaySession(env).Play(a.id,null);
                Check((string)Report(r)["lease_agent"]=="suite","suite held play-runtime while playing");
                Check(!File.Exists(lease),"suite released its lease");
            }
            public void RecoverBeforeLaunch(){
                string before=Sig(Normal()),vrBefore=NormalVr();
                var ps=new PlaySession(env);
                ps.Starter=psi=>{throw new IOException("simulated start failure");};
                bool threw=false;try{ps.Play(a.id,null);}catch(IOException){threw=true;}
                Check(threw,"start failure surfaces");
                Check(Sig(Normal())==before&&NormalVr()==vrBefore,"normal GTA restored at once");
                Check(new PlaySession(env).Unfinished().Count==0,"nothing left unfinished");
                Check(!File.Exists(Path.Combine(env.DevRepo,"local-cache","agent-locks","play-runtime.lease")),"lease released");
            }
            public void RecoverAfterLaunch(bool interruptedBeforeReceipt=false,bool noChanges=false){
                string before=Sig(Normal()),vrBefore=NormalVr();
                string profileBefore=Sig(UserTree.Snapshot(lib.UserDir(a.id)));
                string profileIni=SafeFs.Hash(lib.VrSettingsFile(a.id));
                // Suite dies while the game runs: the game writes progress and exits, the session stays LAUNCHED.
                var ps=new PlaySession(env);
                SessionRecord beforeStart=null;
                ps.Starter=psi=>{
                    if(interruptedBeforeReceipt){
                        beforeStart=ps.Unfinished().Single();
                        Check(beforeStart.phase=="USER_STAGED" && beforeStart.launch_attempted &&
                              SafeFs.IsHash(beforeStart.launch_staging_sha256) && beforeStart.game==null,
                              "the start checkpoint is already durable before process creation");
                    }
                    psi.Arguments+=noChanges?" --fixture-no-write":" --suite-dies";
                    return Process.Start(psi);
                };
                ps.AfterLaunched=child=>{child.WaitForExit();throw new SimulatedSuiteDeath();};
                bool threw=false;try{ps.Play(a.id,null);}catch(SimulatedSuiteDeath){threw=true;}
                Check(threw,"simulated death");
                var open=new PlaySession(env).Unfinished();Check(open.Count==1&&open[0].phase=="LAUNCHED","one LAUNCHED session left");
                if(interruptedBeforeReceipt){
                    Json.Atomic(SafeFs.Under(env.Dir("sessions"),beforeStart.id+"/session.json"),beforeStart);
                }
                Check(Sig(Normal())!=before,"profile still staged");
                Check(new PlaySession(env).Recover()=="OK","recover ok");
                Check(Sig(Normal())==before&&NormalVr()==vrBefore,"normal GTA restored by recover");
                if(noChanges){
                    var recovered=Json.Read<SessionRecord>(SafeFs.Under(env.Dir("sessions"),beforeStart.id+"/session.json"));
                    Check(recovered.result=="cancelled" && profileBefore==Sig(UserTree.Snapshot(lib.UserDir(a.id))) &&
                          profileIni==SafeFs.Hash(lib.VrSettingsFile(a.id)),"a start without changes does not commit staged profile settings");
                }else{
                    var save=SaveInfo.Latest(lib.UserDir(a.id));Check(save!=null&&save.mission=="Tras corte","crash-time progress kept");
                }
                Check(!File.Exists(Path.Combine(env.DevRepo,"local-cache","agent-locks","play-runtime.lease")),"own lease released by recover");
            }
            // Store-game mods in the game folder never block Play. What GTAVXR's EXE would load and cannot run waits aside while it plays
            // and goes back afterwards; the game's own files, harmless files and folders stay where they are.
            public void RetailMods(){
                string game=env.GameDir;
                string[] aside={"OpenIV.asi","dinput8.dll","ScriptHookV.dll","dxgi.dll"},stay={"ScriptHookVDotNet3.dll","bink2w64.dll","scripts/OpenWorldInteriors.dll"};
                var sha=new Dictionary<string,string>();
                foreach(string n in aside.Concat(stay)){Write(game,n,Encoding.UTF8.GetBytes("STORE MOD "+n));sha[n]=SafeFs.Hash(SafeFs.Under(game,n));}
                bool seen=false;var ps=new PlaySession(env);
                ps.Starter=psi=>{
                    Check(aside.All(n=>!File.Exists(SafeFs.Under(game,n))),"incompatible store mods are not in the game folder while GTAVXR runs");
                    Check(stay.All(n=>File.Exists(SafeFs.Under(game,n))),"shipped / harmless files and scripts/ stay in place");
                    seen=true;return Process.Start(psi);};
                var r=ps.Play(a.id,null);
                Check(seen&&r.result=="played","played");
                Check(r.aside.Count==4&&r.aside_kept.Count==0&&!r.aside_moved,"four set aside and all back");
                Check(aside.Concat(stay).All(n=>SafeFs.Hash(SafeFs.Under(game,n))==sha[n]),"every file back with the same bytes");
                // The Suite dies while the game runs: Recover puts them back.
                ps=new PlaySession(env);
                ps.Starter=psi=>{psi.Arguments+=" --suite-dies";return Process.Start(psi);};
                ps.AfterLaunched=child=>{child.WaitForExit();throw new SimulatedSuiteDeath();};
                bool died=false;try{ps.Play(a.id,null);}catch(SimulatedSuiteDeath){died=true;}
                Check(died&&aside.All(n=>!File.Exists(SafeFs.Under(game,n))),"still aside after the crash");
                Check(new PlaySession(env).Recover()=="OK","recover");
                Check(aside.All(n=>SafeFs.Hash(SafeFs.Under(game,n))==sha[n]),"recover put them back");
                // A new file took a name meanwhile: never overwritten, the player's file stays in the session folder.
                ps=new PlaySession(env);
                ps.Starter=psi=>{File.WriteAllText(SafeFs.Under(game,"dinput8.dll"),"NEW");return Process.Start(psi);};
                r=ps.Play(a.id,null);
                Check(r.aside_kept.Count==1&&r.aside_kept[0]=="dinput8.dll"&&File.ReadAllText(SafeFs.Under(game,"dinput8.dll"))=="NEW","new file kept, never overwritten");
                Check(SafeFs.Hash(Path.Combine(env.DataRoot,"sessions",r.id,"aside","dinput8.dll"))==sha["dinput8.dll"],"player's file kept in the session folder");
                Check(LegacyGamePolicy.VerifyFiles(game,env.RetailOverride)!=null,"retail check passes with store mods present");
                foreach(string n in aside.Concat(stay))File.Delete(SafeFs.Under(game,n));
            }
            // Script mods (work/SCRIPT_HOST_PLAN.md): an Open World Interiors-like archive is one card with its OpenIV game files as a
            // companion that is not a card; a script host session puts the profile's scripts/ in the game and the player's back after.
            public void ScriptModPlay(){
                var imp=new Importers(env);string game=env.GameDir;
                Func<string,byte[]> dll=api=>Encoding.ASCII.GetBytes("MZ fixture\0"+api+"\0mscorlib\0");
                string zip=Path.Combine(root,"49f411-OpenWorldInteriors0.2.zip");
                using(var z=System.IO.Compression.ZipFile.Open(zip,System.IO.Compression.ZipArchiveMode.Create)){
                    Action<string,byte[]> add=(n,b)=>{using(var s=z.CreateEntry(n).Open())s.Write(b,0,b.Length);};
                    add("scripts/OpenWorldInteriors.dll",dll("ScriptHookVDotNet2"));
                    add("scripts/OpenWorldInteriors.ini",Encoding.ASCII.GetBytes("[Settings]\r\nGym Blips=1\r\nHospital Blips=1\r\nGym Radio=RADIO_02_POP"));
                    add("scripts/ScriptHookVDotNet2.dll",Encoding.ASCII.GetBytes("store api"));
                    add("OpenIV/Legacy/x64h.rpf/levels/gta5/interiors/v_int_40.rpf/v_hospital.ybn",Encoding.ASCII.GetBytes("RSC7 legacy ybn"));
                    add("OpenIV/Legacy/x64h.rpf/levels/gta5/interiors/int_props/int_medical.rpf/v_med_hospseating2.ydr",Encoding.ASCII.GetBytes("RSC7 legacy ydr"));
                    add("OpenIV/Enhanced/x64h.rpf/levels/gta5/interiors/v_int_40.rpf/v_hospital.ybn",Encoding.ASCII.GetBytes("RSC7 enhanced ybn"));
                }
                var owi=imp.ImportArchive(zip);
                Check(owi.kind=="script"&&owi.loadable&&owi.script_api=="v2"&&owi.source.catalog=="owi"&&owi.IsCard()&&owi.name=="Open World Interiors"&&owi.version=="0.2","script card, got "+owi.kind+"/"+owi.name+"/"+owi.version+"/"+owi.not_loadable_reason);
                Check(owi.files.Count==0&&owi.script_files.Select(f=>f.path).SequenceEqual(new[]{"OpenWorldInteriors.dll","OpenWorldInteriors.ini"}),"its scripts/ files only, no ScriptHookVDotNet of its own");
                var comp=lib.Mod(owi.companion);
                Check(comp!=null&&comp.kind=="content-source"&&!comp.IsCard()&&comp.source.option_of=="owi","OpenIV files as a companion, not a card");
                var ops=new ContentSources(env).Source(comp).Ops;
                Check(ops.Count==2&&ops.Any(o=>o.Archive=="x64h.rpf|levels/gta5/interiors/v_int_40.rpf"&&o.Target=="v_hospital.ybn"&&File.ReadAllText(o.Source)=="RSC7 legacy ybn")&&
                    ops.Any(o=>o.Archive=="x64h.rpf|levels/gta5/interiors/int_props/int_medical.rpf"&&o.Target=="v_med_hospseating2.ydr"),"Legacy files in nested archives, Enhanced left out: "+String.Join(",",ops.Select(o=>o.Archive+"|"+o.Target)));
                int count=lib.Mods().Count;Check(imp.ImportArchive(zip).id==owi.id&&lib.Mods().Count==count,"same archive again: nothing new");
                // 2.4.3: a ScriptHookVDotNet 3 mod is loadable; it works only with a version whose script host brings the v3 API.
                string z3=Path.Combine(root,"Other v3 1.0.zip");
                using(var z=System.IO.Compression.ZipFile.Open(z3,System.IO.Compression.ZipArchiveMode.Create))using(var s=z.CreateEntry("Other/scripts/Other.dll").Open()){var b=dll("ScriptHookVDotNet3");s.Write(b,0,b.Length);}
                var v3m=imp.ImportArchive(z3);
                Check(v3m.kind=="script"&&v3m.loadable&&v3m.not_loadable_reason==null&&v3m.script_api=="v3"&&v3m.IsCard(),"v3 kept and loadable");
                // a v3 mod stored by 2.4.2 ("v3 pending") reads as loadable
                {var old=lib.Mod(v3m.id);old.loadable=false;old.not_loadable_reason="SHVDN_V3_PENDING";lib.SaveMod(old);
                 var back=lib.Mod(v3m.id);Check(back.loadable&&back.not_loadable_reason==null&&lib.Mods().First(x=>x.id==v3m.id).loadable,"a 2.4.2 v3-pending mod reads as loadable");}
                // A version with the script host.
                string src=Path.Combine(root,"src");
                var vs=lib.Version("155v2");vs.id="167-scripts";vs.build="167";vs.mvga="none";vs.mvga_arg=null;vs.script_host="switch";vs.script_host_arg="-gtavxrScriptHost";
                vs.runtime=new List<FileRef>(vs.runtime){Put(src,"GTAVXRHost.dll","host"),Put(src,"scripthost/ScriptHookVDotNet.asi","shvdn asi"),Put(src,"scripthost/ScriptHookVDotNet2.dll","shvdn api")};
                lib.SaveVersion(vs);
                Check(owi.WorksWith(vs)&&!owi.WorksWith(lib.Version("155v2")),"works only with a script host version");
                Check(!vs.RunsShvdnV3()&&!v3m.WorksWith(vs),"a v3 mod does not work with a script host version without ScriptHookVDotNet3.dll");
                {var v3v=lib.Version(vs.id);v3v.runtime=new List<FileRef>(v3v.runtime){Put(src,"scripthost/ScriptHookVDotNet3.dll","shvdn api 3")};
                 Check(v3v.RunsShvdnV3()&&v3m.WorksWith(v3v)&&owi.WorksWith(v3v),"v2 and v3 mods work with a version that brings ScriptHookVDotNet3.dll");}
                // The player's own store-game scripts and ScriptHookVDotNet files.
                Write(game,"scripts/StoreMod.dll",Encoding.ASCII.GetBytes("store script"));Write(game,"ScriptHookVDotNet3.dll",Encoding.ASCII.GetBytes("store api 3"));Write(game,"ScriptHookVDotNet.asi",Encoding.ASCII.GetBytes("store asi"));
                string storeSha=SafeFs.Hash(SafeFs.Under(game,"scripts/StoreMod.dll"));
                // A profile with Open World Interiors, options changed (one to a value the catalog does not list) and an outfit saved earlier.
                string us=Path.Combine(root,"us");Write(us,"settings.xml",Encoding.UTF8.GetBytes(SettingsXml));
                var c=imp.CreateFrom("Interiores","167-scripts",us,UserTree.Snapshot(us),null,"test");
                c.mods=new List<string>{owi.id};c.options=new Dictionary<string,string>{{"owi.blips_gym","off"},{"owi.radio_gym","RADIO_01_CLASS_ROCK"},{"owi.radio_bite","NOT_A_STATION"}};lib.SaveProfile(c);
                Write(lib.ScriptsDir(c.id),"Outfits/old.xml",Encoding.ASCII.GetBytes("old outfit"));
                var r=new PlaySession(env).Play(c.id,null);
                var rep=Report(r);
                Check(r.result=="played"&&r.script_host&&((string)rep["args"]).Contains("-gtavxrScriptHost"),"script host argument");
                // 2.4.2: Open World Interiors lists "Enable MP Maps": the game starts with Online maps in Story on
                Check(VrIni.Get((string)rep["vr_seen"],"mods_mp_maps166")=="1"&&r.online_maps_for!=null&&r.online_maps_for.SequenceEqual(new[]{"Open World Interiors"}),
                    "Online maps on for Open World Interiors, got "+((string)rep["vr_seen"]??"").Replace("\r\n","|")+" / "+(r.online_maps_for==null?"null":String.Join(",",r.online_maps_for)));
                Check(Content.ContentCatalog.NeedsOnlineMaps(new[]{comp,v3m}).Count==0,"only the catalog's Online-maps mods force them");
                Check(((string)rep["scripts_path"]).EndsWith(@"\sessions\"+r.id+@"\scripthost",StringComparison.OrdinalIgnoreCase)&&(string)rep["host_seen"]=="ScriptHookVDotNet.asi|ScriptHookVDotNet2.dll","host folder: GTAVXR's ScriptHookVDotNet build, got "+rep["scripts_path"]+" / "+rep["host_seen"]);
                Check((string)rep["scripts_seen"]=="OpenWorldInteriors.dll|OpenWorldInteriors.ini|Outfits/old.xml","the profile's scripts/ in the game, the player's aside, got "+rep["scripts_seen"]);
                string ini=(string)rep["scripts_ini"];
                Check(Ini.Get(ini,"Settings","Gym Blips")=="0"&&Ini.Get(ini,"Settings","Hospital Blips")=="1"&&Ini.Get(ini,"Settings","Gym Radio")=="RADIO_01_CLASS_ROCK"&&Ini.Get(ini,"Settings","Bite Radio")=="RADIO_02_POP"&&
                    Ini.Get(ini,"Settings","Nightclub Radio")==null&&ini.StartsWith("[Settings]\r\n",StringComparison.Ordinal),"options written in the mod's ini, got "+ini.Replace("\r\n","|"));
                Check((string)rep["shvdn_root"]=="0","the store ScriptHookVDotNet API waited aside");
                Check(SafeFs.Hash(SafeFs.Under(game,"scripts/StoreMod.dll"))==storeSha&&!File.Exists(SafeFs.Under(game,"scripts/OpenWorldInteriors.dll"))&&
                    File.Exists(SafeFs.Under(game,"ScriptHookVDotNet3.dll"))&&File.Exists(SafeFs.Under(game,"ScriptHookVDotNet.asi"))&&!Directory.Exists(SafeFs.Under(game,r.scripts_aside)),"the player's scripts/ and ScriptHookVDotNet files back");
                string mine=lib.ScriptsDir(c.id);
                Check(File.ReadAllText(Path.Combine(mine,"Outfits","old.xml"))=="old outfit"&&File.ReadAllText(Path.Combine(mine,"Outfits","new.xml"))=="saved in game"&&
                    !File.Exists(Path.Combine(mine,"OpenWorldInteriors.dll"))&&!File.Exists(Path.Combine(mine,"OpenWorldInteriors.ini")),"what the scripts wrote kept in the profile, the mod's files not");
                // Without a script mod the same version plays as before: no host, the player's scripts/ stays in place.
                c=lib.Profile(c.id);c.mods.Clear();lib.SaveProfile(c);
                bool stayed=false;var ps=new PlaySession(env);
                ps.Starter=psi=>{stayed=File.Exists(SafeFs.Under(game,"scripts/StoreMod.dll"))&&File.Exists(SafeFs.Under(game,"ScriptHookVDotNet3.dll"));return Process.Start(psi);};
                r=ps.Play(c.id,null);rep=Report(r);
                Check(stayed&&!r.script_host&&!((string)rep["args"]).Contains("-gtavxrScriptHost")&&(string)rep["scripts_path"]=="","no script host without a script mod");
                Check(r.online_maps_for==null,"no script mod: Online maps are not forced");
                // The Suite dies while the game runs: Recover keeps what the scripts wrote and puts the player's scripts/ back.
                c=lib.Profile(c.id);c.mods.Add(owi.id);lib.SaveProfile(c);
                ps=new PlaySession(env);
                ps.Starter=psi=>{psi.Arguments+=" --suite-dies";return Process.Start(psi);};
                ps.AfterLaunched=child=>{child.WaitForExit();throw new SimulatedSuiteDeath();};
                bool died=false;try{ps.Play(c.id,null);}catch(SimulatedSuiteDeath){died=true;}
                Check(died&&File.Exists(SafeFs.Under(game,"scripts/OpenWorldInteriors.dll"))&&!File.Exists(SafeFs.Under(game,"scripts/StoreMod.dll")),"still staged after the crash");
                Check(new PlaySession(env).Recover()=="OK","recover");
                Check(SafeFs.Hash(SafeFs.Under(game,"scripts/StoreMod.dll"))==storeSha&&!File.Exists(SafeFs.Under(game,"scripts/OpenWorldInteriors.dll"))&&
                    File.ReadAllText(Path.Combine(mine,"Outfits","crash.xml"))=="saved before the crash","recover: the player's scripts/ back, the crash session's files kept");
                // A start failure before the game runs puts everything back at once (a player without a scripts/ folder).
                Directory.Delete(SafeFs.Under(game,"scripts"),true);
                ps=new PlaySession(env);ps.Starter=psi=>{throw new IOException("simulated start failure");};
                bool threw=false;try{ps.Play(c.id,null);}catch(IOException){threw=true;}
                Check(threw&&!Directory.Exists(SafeFs.Under(game,"scripts"))&&File.Exists(SafeFs.Under(game,"ScriptHookVDotNet3.dll"))&&new PlaySession(env).Unfinished().Count==0,"no scripts/ left when the player had none");
                foreach(string n in new[]{"ScriptHookVDotNet3.dll","ScriptHookVDotNet.asi"})File.Delete(SafeFs.Under(game,n));
            }
            // 2.4.4: ScriptHookV .asi mods (user 2026-11-04 "adelante con los .asi y los mods shvdn que llaman a scripthookv.dll"): the
            // Suite's copy imports GTAVXRHost.dll; a script host session puts the mod in the game folder beside the player's files, keeps
            // what it wrote in the profile and takes it out after, also after a crash; conflicts are listed before Play.
            public void AsiModPlay(){
                var imp=new Importers(env);string game=env.GameDir;string src=Path.Combine(root,"src");
                Func<string,byte[]> text=s=>Encoding.ASCII.GetBytes(s);
                Func<string,Dictionary<string,byte[]>,string> zip=(name,entries)=>{string z=Path.Combine(root,name);
                    using(var a=System.IO.Compression.ZipFile.Open(z,System.IO.Compression.ZipArchiveMode.Create))foreach(var kv in entries)using(var s=a.CreateEntry(kv.Key).Open())s.Write(kv.Value,0,kv.Value.Length);
                    return z;};
                byte[] managed=EngineTests.ManagedShv(Path.Combine(root,"emit"),false);
                var trainer=imp.ImportArchive(zip("My Trainer 1.2.zip",new Dictionary<string,byte[]>{
                    {"My Trainer/MyTrainer.asi",EngineTests.FakeAsi(true,false)},{"My Trainer/MyTrainer.ini",text("[Keys]\r\nMenu=F4\r\n")},
                    {"My Trainer/MyTrainer/config.ini",text("speed=1\r\n")},{"My Trainer/ScriptHookV.dll",text("store ScriptHookV")},
                    {"My Trainer/dinput8.dll",text("asi loader")},{"My Trainer/Read me.txt",text("read me")},{"My Trainer/scripts/ShvCaller.dll",managed}}));
                Check(trainer.kind=="script"&&trainer.loadable&&trainer.HasAsi()&&trainer.IsCard()&&trainer.version=="1.2","asi card, got "+trainer.kind+"/"+trainer.name+"/"+trainer.version+"/"+trainer.not_loadable_reason);
                Check(trainer.asi_files.Select(f=>f.path).SequenceEqual(new[]{"MyTrainer.asi","MyTrainer.ini","MyTrainer/config.ini"}),"the game-folder part without the store ScriptHookV, the ASI loader, the readme or scripts/, got "+String.Join(",",trainer.asi_files.Select(f=>f.path)));
                string asiCopy=Encoding.ASCII.GetString(File.ReadAllBytes(blobs.PathOf(trainer.asi_files[0].sha256)));
                Check(asiCopy.Contains("GTAVXRHost.dll")&&!asiCopy.Contains("ScriptHookV.dll")&&asiCopy.Contains("KERNEL32.dll"),"the stored .asi imports GTAVXRHost.dll");
                Check(trainer.script_files.Select(f=>f.path).SequenceEqual(new[]{"ShvCaller.dll"})&&Encoding.ASCII.GetString(File.ReadAllBytes(blobs.PathOf(trainer.script_files[0].sha256))).Contains("GTAVXRHost.dll"),"its ScriptHookVDotNet script calls GTAVXRHost.dll");
                Check(trainer.shv_by_name==null,"no by-name lookup in it");
                // a single .asi that looks ScriptHookV.dll up by name; an .asi for the store EXE only (no ScriptHookV import)
                string solo=Path.Combine(root,"Solo.asi");File.WriteAllBytes(solo,EngineTests.FakeAsi(true,true));
                var sm=imp.ImportArchive(solo);
                Check(sm.kind=="script"&&sm.loadable&&sm.asi_files.Single().path=="Solo.asi"&&sm.shv_by_name!=null&&sm.shv_by_name.SequenceEqual(new[]{"Solo.asi"}),"a single .asi, its by-name lookup noted");
                var plain=imp.ImportArchive(zip("Plain.zip",new Dictionary<string,byte[]>{{"Plain.asi",EngineTests.FakeAsi(false,false)}}));
                Check(plain.kind=="script"&&!plain.loadable&&plain.not_loadable_reason=="ASI_WITHOUT_SCRIPTHOOKV","an .asi without ScriptHookV is kept, not loadable, got "+plain.not_loadable_reason);
                // 2.4.5: an .asi that only finds ScriptHookV.dll by name (no import) is loadable: its copy names GTAVXRHost.dll
                var dyn=imp.ImportArchive(zip("Dynamic.zip",new Dictionary<string,byte[]>{{"Dynamic.asi",EngineTests.FakeAsi(false,true)}}));
                string dynCopy=Encoding.ASCII.GetString(File.ReadAllBytes(blobs.PathOf(dyn.asi_files[0].sha256)));
                Check(dyn.kind=="script"&&dyn.loadable&&dyn.not_loadable_reason==null&&dynCopy.Contains("GTAVXRHost.dll\0")&&!dynCopy.Contains("\0ScriptHookV.dll\0"),
                    "a by-name-only .asi is loadable and renamed, got "+dyn.not_loadable_reason);
                // versions: a script host without the .asi marker (package v13) and one with it (v14)
                var old=lib.Version("155v2");old.id="167-old";old.build="167";old.mvga="none";old.mvga_arg=null;old.script_host="switch";old.script_host_arg="-gtavxrScriptHost";
                old.runtime=new List<FileRef>(old.runtime){Put(src,"GTAVXRHost.dll","host"),Put(src,"scripthost/ScriptHookVDotNet.asi","shvdn asi"),Put(src,"scripthost/ScriptHookVDotNet2.dll","shvdn api")};
                lib.SaveVersion(old);
                var vs=lib.Version("167-old");vs.id="167-scripts";vs.runtime=new List<FileRef>(vs.runtime){Put(src,"scripthost/gtavxr-host-features.txt","asi-game-folder")};lib.SaveVersion(vs);
                Check(!old.RunsAsiMods()&&!trainer.WorksWith(old)&&vs.RunsAsiMods()&&trainer.WorksWith(vs),"an .asi mod works only with a version whose host loads .asi mods");
                // the player's own file with the same name as one of the mod's
                Write(game,"MyTrainer.ini",text("player's own"));string playerSha=SafeFs.Hash(SafeFs.Under(game,"MyTrainer.ini"));
                string us=Path.Combine(root,"us");Write(us,"settings.xml",Encoding.UTF8.GetBytes(SettingsXml));
                var c=imp.CreateFrom("Trainer","167-scripts",us,UserTree.Snapshot(us),null,"test");
                c.mods=new List<string>{trainer.id};lib.SaveProfile(c);
                Check(ModConflicts.For(env,c,vs).Count==0,"one mod: no conflicts");
                var r=new PlaySession(env).Play(c.id,null);var rep=Report(r);
                string asiPath=SafeFs.Under(game,"MyTrainer.asi");
                Check(r.result=="played"&&r.script_host&&(string)rep["asi_env"]==asiPath&&(string)rep["asi_renamed"]=="1","the host was told to load the renamed .asi, got "+rep["asi_env"]+" / "+rep["asi_renamed"]);
                Check((string)rep["asi_ini"]=="[Keys]\r\nMenu=F4\r\n"&&(string)rep["asi_config"]=="speed=1\r\n","the mod's files beside it, the player's ini aside, got "+rep["asi_ini"]+" / "+rep["asi_config"]);
                Check((string)rep["scripts_seen"]=="ShvCaller.dll","its ScriptHookVDotNet script in scripts/, got "+rep["scripts_seen"]);
                Check(!File.Exists(asiPath)&&!Directory.Exists(SafeFs.Under(game,"MyTrainer"))&&!File.Exists(SafeFs.Under(game,"MyTrainer.log"))&&SafeFs.Hash(SafeFs.Under(game,"MyTrainer.ini"))==playerSha,"game folder: the mod out, the player's ini back");
                Check(File.ReadAllText(SafeFs.Under(game,"unrelated.txt"))=="not the mod's","a new file that is not the mod's stays");
                string mine=lib.AsiDir(c.id);
                Check(File.ReadAllText(Path.Combine(mine,"MyTrainer","config.ini"))=="speed=1\r\nchanged=1\n"&&File.ReadAllText(Path.Combine(mine,"MyTrainer.log"))=="log of MyTrainer.asi"&&
                    !File.Exists(Path.Combine(mine,"MyTrainer.asi"))&&!File.Exists(Path.Combine(mine,"MyTrainer.ini")),"what the mod wrote kept in the profile, its unchanged files not");
                File.Delete(SafeFs.Under(game,"unrelated.txt"));
                // next session: the profile's saved settings replace the mod's
                r=new PlaySession(env).Play(c.id,null);rep=Report(r);
                Check((string)rep["asi_config"]=="speed=1\r\nchanged=1\n","the profile's saved settings in the game, got "+((string)rep["asi_config"]).Replace("\r\n","|").Replace("\n","/"));
                File.Delete(SafeFs.Under(game,"unrelated.txt"));
                // the Suite dies while the game runs: recover keeps what the mod wrote and puts the player's file back
                var ps=new PlaySession(env);
                ps.Starter=psi=>{psi.Arguments+=" --suite-dies";return Process.Start(psi);};
                ps.AfterLaunched=child=>{child.WaitForExit();throw new SimulatedSuiteDeath();};
                bool died=false;try{ps.Play(c.id,null);}catch(SimulatedSuiteDeath){died=true;}
                Check(died&&File.Exists(asiPath)&&File.ReadAllText(SafeFs.Under(game,"MyTrainer.ini"))=="[Keys]\r\nMenu=F4\r\n","still staged after the crash");
                Check(new PlaySession(env).Recover()=="OK","recover");
                Check(!File.Exists(asiPath)&&!Directory.Exists(SafeFs.Under(game,"MyTrainer"))&&SafeFs.Hash(SafeFs.Under(game,"MyTrainer.ini"))==playerSha&&
                    File.ReadAllText(Path.Combine(mine,"MyTrainer","config.ini"))=="speed=1\r\nchanged=1\nchanged=1\nchanged=1\n","recover: the mod out, the player's ini back, the crash session's settings kept");
                File.Delete(SafeFs.Under(game,"unrelated.txt"));
                // two mods with the same game-folder file: listed before Play; Play refuses without touching the game folder
                var other=imp.ImportArchive(zip("Other Trainer.zip",new Dictionary<string,byte[]>{{"OtherTrainer.asi",EngineTests.FakeAsi(true,false)},{"MyTrainer.ini",text("other's")}}));
                c=lib.Profile(c.id);c.mods.Add(other.id);lib.SaveProfile(c);
                var conf=ModConflicts.For(env,c,vs);
                Check(conf.Count==1&&conf[0].kind=="asi-file"&&conf[0].verdict=="blocks"&&conf[0].what=="MyTrainer.ini"&&conf[0].mods.SequenceEqual(new[]{trainer.name,other.name}),"the same file in two .asi mods blocks, got "+String.Join(";",conf.Select(x=>x.kind+"/"+x.what)));
                bool twice=false;try{new PlaySession(env).Play(c.id,null);}catch(InvalidDataException e){twice=e.Message.StartsWith("ASI_FILE_TWICE");}
                Check(twice&&SafeFs.Hash(SafeFs.Under(game,"MyTrainer.ini"))==playerSha&&!File.Exists(asiPath)&&new PlaySession(env).Unfinished().Count==0,"Play refused, game folder untouched");
                // an .asi mod that brings a file of GTAVXR's: listed, and Play refuses
                c=lib.Profile(c.id);c.mods.Remove(other.id);
                var bad=imp.ImportArchive(zip("Bad.zip",new Dictionary<string,byte[]>{{"Bad.asi",EngineTests.FakeAsi(true,false)},{"GTAVXR_FixtureGame.exe",text("not ours")}}));
                c.mods.Add(bad.id);lib.SaveProfile(c);
                conf=ModConflicts.For(env,c,vs);
                Check(conf.Any(x=>x.kind=="asi-gtavxr"&&x.verdict=="blocks"&&x.what=="GTAVXR_FixtureGame.exe"&&x.mods.SequenceEqual(new[]{bad.name})),"an .asi mod with a GTAVXR file is listed, got "+String.Join(";",conf.Select(x=>x.kind+"/"+x.what)));
                string exeSha=vs.files.First(f=>f.path=="GTAVXR_FixtureGame.exe").sha256;
                bool refused=false;try{new PlaySession(env).Play(c.id,null);}catch(GameConflictException){refused=true;}
                Check(refused&&SafeFs.Hash(SafeFs.Under(game,"GTAVXR_FixtureGame.exe"))==exeSha&&!File.Exists(SafeFs.Under(game,"Bad.asi"))&&new PlaySession(env).Unfinished().Count==0,"Play refused, GTAVXR's file kept");
                // content mods: a data file both change is merged, a whole file both replace comes from the later one in the merge order
                var alpha=imp.ImportArchive(Oiv("Alpha Mod","1","update/update.rpf|common/data/carcols.ymt","update/update.rpf|x64/models/car.ytd"));
                var beta=imp.ImportArchive(Oiv("Beta Mod","1","update/update.rpf|common/data/carcols.ymt","update/update.rpf|x64/models/car.ytd"));
                var vc=lib.Version("167-scripts");vc.id="167-content";vc.content="profile";lib.SaveVersion(vc);
                string ud=Path.Combine(root,"ud");Write(ud,"settings.xml",Encoding.UTF8.GetBytes(SettingsXml));
                var d=imp.CreateFrom("Contenido","167-content",ud,UserTree.Snapshot(ud),null,"test");d.mods=new List<string>{alpha.id,beta.id};lib.SaveProfile(d);
                conf=ModConflicts.For(env,d,vc);
                var order=new ContentSources(env).Plan(d,new List<ModManifest>{alpha,beta}).Sources;
                string last=order.Last()==alpha.id?alpha.name:beta.name;
                Check(order.Count==2&&conf.Count==2&&conf.Any(x=>x.kind=="content-text"&&x.verdict=="merged"&&x.what=="update/update.rpf|common/data/carcols.ymt"&&x.mods.Count==2)&&
                    conf.Any(x=>x.kind=="content-whole"&&x.verdict=="wins"&&x.what=="update/update.rpf|x64/models/car.ytd"&&x.winner==last)&&conf[0].verdict=="wins",
                    "content: data file merged, whole file from the later mod, got "+String.Join(";",conf.Select(x=>x.kind+"/"+x.verdict+"/"+x.what+"/"+x.winner)));
            }
            public void RecoverLayer(){
                var gl=new GameLayer(env);
                // Write the journal for a change to 155v2 and stop before running it.
                var mods=new List<ModManifest>();var plan=gl.Preview(v2,mods);Check(!plan.unchanged,"change pending");
                var j=new ApplyJournal{schema="gtavxr-suite2-apply-v1",game_dir=SafeFs.Root(env.GameDir),version="155v2",mods=new List<string>(),
                    adds=v2.files.Where(f=>f.path=="common/data/one.txt").ToList(),removes=v1.files.Where(f=>f.path=="common/data/only125.txt").ToList(),
                    keeps=v2.files.Where(f=>f.path!="common/data/one.txt").ToList(),dirs=new List<string>(),started_utc="x"};
                // Half-done: the add already happened.
                blobs.Materialize(j.adds[0],SafeFs.Under(env.GameDir,j.adds[0].path));
                Json.Atomic(Path.Combine(env.DataRoot,"game","apply.json"),j);
                Check(gl.Pending,"pending");
                Check(new PlaySession(env).Recover()=="OK","recover");
                Check(!gl.Pending&&gl.State().version=="155v2"&&!File.Exists(Path.Combine(env.GameDir,"common","data","only125.txt")),"rolled forward");
            }
            public void SameIdNewFiles(){
                // A version re-imported under the same id (167-scripts v2) changes its files, not its id: the game gets the new bytes.
                var gl=new GameLayer(env);var mods=new List<ModManifest>();
                Check(gl.State().version=="155v2"&&gl.Preview(v2,mods).unchanged,"155v2 installed");
                var again=Json.Read<VersionManifest>(Path.Combine(env.DataRoot,"versions","155v2.json"));
                var newer=Put(Path.Combine(root,"src"),"common/data/one.txt","ONE v2 of the same id");
                again.files=again.files.Select(f=>f.path=="common/data/one.txt"?newer:f).ToList();
                string one=Path.Combine(env.GameDir,"common","data","one.txt");
                Check(!gl.Preview(again,mods).unchanged,"same id, other files: not unchanged");
                gl.Apply(again,mods,null);
                Check(File.ReadAllText(one)=="ONE v2 of the same id"&&gl.Preview(again,mods).unchanged,"the new bytes are installed");
                gl.Apply(v2,mods,null);
                Check(SafeFs.Hash(one)==v2.files.First(f=>f.path=="common/data/one.txt").sha256&&gl.Preview(v2,mods).unchanged,"and back to the first files");
            }
            public void ProfileOps(){
                var imp=new Importers(env);
                var copy=imp.Duplicate(a.id,"Historia VR (copia)");
                Check(Sig(lib.Profile(copy.id).user)==Sig(lib.Profile(a.id).user)&&lib.Profile(copy.id).has_vr_settings,"copy has same saves and VR settings");
                var fresh=imp.CreateProfile("Nueva",v1.id,false,null);
                Check(lib.Profile(fresh.id).user.Count==1&&lib.Profile(fresh.id).user[0].relative=="settings.xml","new story keeps only settings.xml");
                Check(lib.Profile(fresh.id).has_vr_settings&&File.ReadAllText(lib.VrSettingsFile(fresh.id))==Importers.StartingVrSettings,"new profile starts with the Historia VR list");
                var normal=imp.CreateProfile("Normal",v1.id,true,a.id);
                Check(lib.Profile(normal.id).user.Count==Normal().Count&&lib.Profile(normal.id).has_vr_settings,"copy of normal saves + VR settings from A");
                lib.DeleteProfile(fresh.id);Check(lib.Profile(fresh.id)==null&&Directory.GetDirectories(Path.Combine(env.DataRoot,"recycle")).Length==1,"deleted to recycle");
            }
            VersionManifest Swapping(){
                var ours=Put(Path.Combine(root,"src"),"update/update.rpf","VR BUILD UPDATE RPF");
                var orig=SafeFs.Hash(Path.Combine(env.GameDir,"update","update.rpf"));
                var v=Json.Read<VersionManifest>(Path.Combine(env.DataRoot,"versions","125-dual.json"));
                v.id="125-swap";v.replaces=new List<ReplaceRef>{new ReplaceRef{path="update/update.rpf",sha256=ours.sha256,bytes=ours.bytes,original_sha256=orig,original_bytes=new FileInfo(Path.Combine(env.GameDir,"update","update.rpf")).Length}};
                lib.SaveVersion(v);var pa=lib.Profile(a.id);pa.version=v.id;lib.SaveProfile(pa);return v;
            }
            public void PlaySwapping(){
                var v=Swapping();string orig=v.replaces[0].original_sha256;
                var r=new PlaySession(env).Play(a.id,null);
                Check((string)Report(r)["rpf"]==v.replaces[0].sha256,"game saw the swapped file");
                Check(SafeFs.Hash(Path.Combine(env.GameDir,"update","update.rpf"))==orig,"original back after play");
                Check(new Blobs(env).Has(orig,v.replaces[0].original_bytes),"original kept in the store");
                Check(!r.swapped&&r.phase=="CLOSED","session closed, nothing swapped");
            }
            public void CrashSwapping(){
                var v=lib.Version("125-swap");string orig=v.replaces[0].original_sha256;
                var ps=new PlaySession(env);ps.AfterLaunched=child=>{child.WaitForExit();throw new SimulatedSuiteDeath();};
                bool threw=false;try{ps.Play(a.id,null);}catch(SimulatedSuiteDeath){threw=true;}
                Check(threw&&SafeFs.Hash(Path.Combine(env.GameDir,"update","update.rpf"))==v.replaces[0].sha256,"swapped file still in place after the crash");
                Check(new PlaySession(env).Recover()=="OK","recover");
                Check(SafeFs.Hash(Path.Combine(env.GameDir,"update","update.rpf"))==orig,"original restored by recover");
                var pa=lib.Profile(a.id);pa.version="125-dual";lib.SaveProfile(pa);
            }
            public void RepairFiles(){
                var gl=new GameLayer(env);var f=gl.State().files.First(x=>x.path=="common/shaders/s.fxc");string p=SafeFs.Under(env.GameDir,f.path);
                File.WriteAllText(p,"DAMAGED");Check(gl.Verify().Count==1,"damage detected");
                var fixedList=gl.Repair();Check(fixedList.Count==1&&gl.Verify().Count==0&&SafeFs.Hash(p)==f.sha256,"repaired from the store");
            }            public void Uninstall(){
                new GameLayer(env).Uninstall(null);
                var left=Directory.GetFiles(env.GameDir,"*",SearchOption.AllDirectories).Select(f=>f.Substring(env.GameDir.Length+1).Replace('\\','/')).OrderBy(x=>x).ToArray();
                Check(String.Join(",",left)=="GTA5.exe,update/update.rpf,update/update2.rpf,x64a.rpf","only the game's own files remain, got "+String.Join(",",left));
                Check(Directory.GetDirectories(env.GameDir).Length==1,"created folders removed");
                Check(new GameLayer(env).State()==null,"no state");
            }

            // ---------- test flights ----------
            Profile tp;string game{get{return env.GameDir;}}
            GameLayer Gl{get{return new GameLayer(env);}}
            void EnsureFlight(){
                if(tp!=null)return;
                // Normal play first: 125 stays installed. Then a test profile copied from B with 155v2 + MVGA.
                new PlaySession(env).Play(a.id,null);
                var imp=new Importers(env);
                tp=imp.CreateTestProfile("Prueba MVGA","155v2",b.id,false);
                tp=lib.Profile(tp.id);tp.mods.Add(mvga.id);lib.SaveProfile(tp);
            }
            void Back125(string what){
                var st=Gl.State();
                Check(st!=null&&st.version=="125-dual"&&st.mods.Count==0,what+": 125 without mods back in the game, got "+(st==null?"nothing":st.version+"+"+st.mods.Count));
                Check(File.Exists(Path.Combine(game,"common","data","only125.txt"))&&File.ReadAllText(Path.Combine(game,"common","data","one.txt"))=="one-v1",what+": 125 files");
                Check(!Directory.Exists(Path.Combine(game,"gtavxr_mvga147")),what+": MVGA files gone");
                Check(Gl.Verify().Count==0,what+": every GTAVXR file intact");
            }
            public void FlightStray(){
                EnsureFlight();
                string normal=Sig(Normal()),vr=NormalVr(),aSig=Sig(lib.Profile(a.id).user);
                File.WriteAllText(Path.Combine(game,"x64b.rpf"),"FIXTURE GAME DATB");File.WriteAllText(Path.Combine(game,"x64c.rpf"),"FIXTURE GAME DATC");
                var ps=new PlaySession(env);ps.Starter=psi=>{psi.Arguments+=" --fixture-stray";return Process.Start(psi);};
                var r=ps.Play(tp.id,null);
                Check(r.test&&r.phase=="CLOSED"&&r.result=="played"&&!r.aborted,"closed test flight, got "+r.phase+"/"+r.result);
                var rep=Report(r);
                Check((string)rep["mvga_seen"]=="1"&&((string)rep["args"]).Contains("-gtavxrMvga"),"the test ran 155 with MVGA");
                Back125("after the flight");
                foreach(string q in new[]{"stray/cache.bin","GTA5_crash.log","common/shaders/s.fxc"})Check(r.quarantined.Contains(q)&&File.Exists(Path.Combine(env.DataRoot,"quarantine",r.id,q.Replace('/','\\'))),"aside: "+q+" in "+String.Join(",",r.quarantined));
                Check(!Directory.Exists(Path.Combine(game,"stray"))&&!File.Exists(Path.Combine(game,"GTA5_crash.log")),"new files gone from the game folder");
                Check(r.changed_game_files.OrderBy(x=>x).SequenceEqual(new[]{"x64a.rpf","x64b.rpf"}),"changed game files reported (x64b same size + same date), got "+String.Join(",",r.changed_game_files));
                Check(r.missing_game_files!=null&&r.missing_game_files.SequenceEqual(new[]{"x64c.rpf"}),"missing game file reported, got "+(r.missing_game_files==null?"null":String.Join(",",r.missing_game_files)));
                Check(r.retail_check=="ok","retail files checked");
                Check(Sig(Normal())==normal&&NormalVr()==vr,"normal GTA restored");
                Check(Sig(lib.Profile(a.id).user)==aSig,"other profiles untouched");
                var save=SaveInfo.Latest(lib.UserDir(tp.id));Check(save!=null&&save.mission=="Prueba","test progress kept in the test profile");
                File.Delete(Path.Combine(game,"x64b.rpf"));
                Check(r.exit_marker&&!Directory.EnumerateFileSystemEntries(env.ExitMarkerDir).Any(),"exit marker placed and cleaned up");
                Check(r.exit_hang==null&&r.exit_dump==null,"a game that exits normally is not touched");
            }
            public void FlightExitHang(){
                EnsureFlight();
                string normal=Sig(Normal()),vr=NormalVr();
                var ps=new PlaySession(env);ps.Starter=psi=>{psi.Arguments+=" --fixture-exit-hang";return Process.Start(psi);};
                var r=ps.Play(tp.id,null);
                Check(r.phase=="CLOSED"&&r.result=="played"&&!r.aborted,"landed normally, got "+r.phase+"/"+r.result);
                Check(r.exit_marker&&r.exit_file_seen&&r.exit_hang=="closed-after-exit-file","stuck exit detected and closed, got "+r.exit_hang);
                Check(r.exit_dump!=null&&new FileInfo(r.exit_dump).Length>0,"thread stacks kept");
                Check(!ps.GameRunning()&&!Directory.EnumerateFileSystemEntries(env.ExitMarkerDir).Any(),"no game and no marker left");
                Back125("after the stuck exit");
                Check(Sig(Normal())==normal&&NormalVr()==vr,"normal GTA restored");
            }
            public void FlightAbort(){
                EnsureFlight();
                string normal=Sig(Normal()),vr=NormalVr();
                var ps=new PlaySession(env);ps.Starter=psi=>{psi.Arguments+=" --fixture-hang";return Process.Start(psi);};
                bool aborted=false,windowClose=true;System.Threading.Thread th=null;
                ps.AfterLaunched=child=>{th=new System.Threading.Thread(()=>{System.Threading.Thread.Sleep(900);windowClose=ps.CloseGame();aborted=ps.Abort();});th.IsBackground=true;th.Start();};
                var r=ps.Play(tp.id,null);
                Check(th!=null&&th.Join(10000),"abort thread finished");
                Check(aborted&&r.aborted&&r.phase=="CLOSED","aborted and closed");
                Check(!windowClose&&!r.close_requested,"close request without a game window ends nothing and is not recorded");
                Check(!ps.GameRunning(),"no fixture game left");
                Back125("after the abort");
                Check(Sig(Normal())==normal&&NormalVr()==vr,"normal GTA restored");
                var save=SaveInfo.Latest(lib.UserDir(tp.id));Check(save!=null&&save.mission=="Colgado","progress written before the abort kept");
                Check(!ps.Abort(),"nothing to abort afterwards");
                Check(r.exit_hang=="forced-by-user"&&r.exit_dump!=null&&new FileInfo(r.exit_dump).Length>0,"forced close keeps the stacks");
            }
            public void FlightCrash(){
                EnsureFlight();
                string normal=Sig(Normal()),vr=NormalVr();
                var ps=new PlaySession(env);
                ps.Starter=psi=>{psi.Arguments+=" --suite-dies";return Process.Start(psi);};
                ps.AfterLaunched=child=>{child.WaitForExit();throw new SimulatedSuiteDeath();};
                bool threw=false;try{ps.Play(tp.id,null);}catch(SimulatedSuiteDeath){threw=true;}
                Check(threw&&Gl.State().version=="155v2","the test version is still in the game after the crash");
                Check(new PlaySession(env).Recover()=="OK","recover");
                Back125("after recover");
                var r=new PlaySession(env).Last();Check(r.test&&r.phase=="CLOSED"&&r.retail_check=="ok","flight closed by recover");
                Check(Sig(Normal())==normal&&NormalVr()==vr,"normal GTA restored");
            }
            public void FlightStartFails(){
                EnsureFlight();
                string normal=Sig(Normal());
                var ps=new PlaySession(env);ps.Starter=psi=>{throw new IOException("simulated start failure");};
                bool threw=false;try{ps.Play(tp.id,null);}catch(IOException){threw=true;}
                Check(threw,"start failure surfaces");
                Back125("after the failed start");
                var r=new PlaySession(env).Last();Check(r.phase=="CLOSED"&&r.result=="cancelled"&&new PlaySession(env).Unfinished().Count==0,"closed as cancelled");
                Check(Sig(Normal())==normal,"normal GTA restored");
            }
            public void FlightUndo(){
                EnsureFlight();
                var beforeSave=SaveInfo.Latest(lib.UserDir(tp.id));Check(beforeSave!=null&&beforeSave.mission=="Tras corte","save before this flight");
                var r=new PlaySession(env).Play(tp.id,null);
                Check(SaveInfo.Latest(lib.UserDir(tp.id)).mission=="Prueba","flight wrote progress");
                new Importers(env).UndoSession(tp.id,r.id);
                var p=lib.Profile(tp.id);
                Check(SaveInfo.Latest(lib.UserDir(tp.id)).mission=="Tras corte","undo brings back the save from before the flight");
                UserTree.Verify(lib.UserDir(tp.id),p.user);
                new Importers(env).BackToStart(tp.id);
                p=lib.Profile(tp.id);
                Check(SaveInfo.Latest(lib.UserDir(tp.id))==null&&!p.has_vr_settings,"starting point: copy of B (no saves, no VR settings)");
                Check(Directory.GetDirectories(Path.Combine(lib.ProfileDir(tp.id),"history")).Any(d=>d.EndsWith("-start")),"state before going back kept in history");
            }
            public void FlightPromote(){
                EnsureFlight();
                string aSig=Sig(lib.Profile(a.id).user);
                Check(lib.Mod(mvga.id).tested_utc==null,"MVGA not tested yet");
                new Importers(env).Promote(tp.id,a.id);
                var pa=lib.Profile(a.id);
                Check(pa.version=="155v2"&&pa.mods.SequenceEqual(new[]{mvga.id})&&!pa.test,"A takes version + mods and stays a normal profile");
                Check(Sig(pa.user)==aSig,"A keeps its saves");
                Check(lib.Mod(mvga.id).tested_utc!=null,"MVGA marked tested");
            }

            public void ImportRelease(){
                // Fixture repository release in the launcher format.
                Directory.CreateDirectory(Path.Combine(env.DevRepo,"work"));File.WriteAllText(Path.Combine(env.DevRepo,"work","AGENT_LEASE.json"),"{}");
                string rel=Path.Combine(env.DevRepo,"local-cache","play","releases","native-xr-build155-mvga265-v1");Directory.CreateDirectory(Path.Combine(rel,"trial-profile"));
                Func<string,string,Dictionary<string,object>> pay=(relative,text)=>{string p=Path.Combine(rel,"src",relative.Replace('\\','_'));Directory.CreateDirectory(Path.GetDirectoryName(p));File.WriteAllText(p,text);return new Dictionary<string,object>{{"bytes",new FileInfo(p).Length},{"source",p},{"sha256",SafeFs.Hash(p)},{"original",""},{"relative",relative}};};
                var payloads=new List<object>{pay("GTAVXR_SourceBaseline.exe","exe"),pay("common\\data\\a.txt","a"),pay("gtavxr_mvga147\\common\\x.xml","mv")};
                File.WriteAllText(Path.Combine(rel,"gtavxr_native_openxr.dll"),"bridge");File.WriteAllText(Path.Combine(rel,"gtavxr_trainer.dll"),"trainer");
                File.WriteAllText(Path.Combine(rel,"trial-profile","vr-settings.ini"),"[VR]\nseed=1\n");
                var release=new Dictionary<string,object>{{"name","native-xr-build155-mvga265-v1"},{"retail_sha256",LegacyGamePolicy.Legacy3889()["GTA5.exe"]},{"bridge",Path.Combine(rel,"gtavxr_native_openxr.dll")},
                    {"payloads",payloads},{"files",new List<object>()},{"implicit_layers",new List<object>{new Dictionary<string,object>{{"disable","DISABLE_XR_APILAYER_reshade_1"}}}}};
                File.WriteAllText(Path.Combine(rel,"release.json"),Json.Encode(release));
                string sha=SafeFs.Hash(Path.Combine(rel,"release.json"));
                foreach(string s in new[]{"s1","s2"}){string d=Path.Combine(env.LegacySessionsRoot,s);Directory.CreateDirectory(d);
                    File.WriteAllText(Path.Combine(d,"launch.json"),Json.Encode(new Dictionary<string,object>{{"release_sha256",sha},{"arguments",new[]{"-noSocialClub","-logfile",Path.Combine(d,"native-runtime.log"),"-gtavxrHudMirror"}}}));}
                var imp=new Importers(env);
                Check(imp.ImportableReleases().Count==0,"not importable without stable pointer or contract");
                File.WriteAllText(Path.Combine(env.DevRepo,"local-cache","play","current.json"),Json.Encode(new Dictionary<string,object>{{"name","native-xr-build155-mvga265-v1"}}));
                Check(imp.ImportableReleases().Count==1,"stable release importable");
                var v=imp.ImportRelease("native-xr-build155-mvga265-v1",null);
                Check(v.id=="155"&&v.channel=="stable"&&v.mvga=="required","id/channel/mvga required");
                Check(String.Join(" ",v.launch.args)=="-noSocialClub -logfile {LOGFILE} -gtavxrHudMirror","recorded args normalized");
                Check(v.files.Count==2&&v.files.All(f=>!f.path.StartsWith("gtavxr_mvga147")),"MVGA split from version files");
                Check(v.runtime.Count==2&&v.vr_settings_seed!=null&&v.launch.disable_layers.SequenceEqual(new[]{"DISABLE_XR_APILAYER_reshade_1"}),"runtime, seed and layer");
                var m=lib.Mods().Single(x=>x.files.Any(f=>f.path=="gtavxr_mvga147/common/x.xml"));Check(m.kind=="mvga-native"&&m.files.Count==1&&m.files[0].path=="gtavxr_mvga147/common/x.xml","MVGA mod created");
                int mods=lib.Mods().Count;imp.ImportRelease("native-xr-build155-mvga265-v1",null);Check(lib.Mods().Count==mods,"no duplicate MVGA on re-import");
                Check(!imp.ContractStale(lib.Version("155")),"release without contract never stale");
                // 2.4.1: a contract release an older Suite imported without its new fields (2.3.9 + 167-neural) comes back complete.
                string nrel=Path.Combine(env.DevRepo,"local-cache","play","releases","native-xr-neural-suite-v3");Directory.CreateDirectory(nrel);
                string nexe=Path.Combine(nrel,"GTAVXR_Neural.exe");File.WriteAllText(nexe,"exe");
                foreach(string f in new[]{"gtavxr_native_openxr.dll","gtavxr_trainer.dll","openxr_loader.dll"})File.WriteAllText(Path.Combine(nrel,f),f);
                File.WriteAllText(Path.Combine(nrel,"release.json"),Json.Encode(new Dictionary<string,object>{{"name","native-xr-neural-suite-v3"},{"retail_sha256",LegacyGamePolicy.Legacy3889()["GTA5.exe"]},
                    {"bridge",Path.Combine(nrel,"gtavxr_native_openxr.dll")},{"files",new List<object>()},
                    {"payloads",new List<object>{new Dictionary<string,object>{{"bytes",new FileInfo(nexe).Length},{"source",nexe},{"sha256",SafeFs.Hash(nexe)},{"relative","GTAVXR_Neural.exe"}}}}}));
                string launchFile=Path.Combine(nrel,"suite-launch.json");
                File.WriteAllText(launchFile,Json.Encode(new SuiteLaunchFile{schema="gtavxr-suite2-launch-v1",id="167-neural",build="167",title="neural",args=new List<string>{"-logfile","{LOGFILE}"},mvga="none",
                    session_mode="suite",settings_mode="suite",runtime_files=new List<string>{"openxr_loader.dll"},
                    neural=new NeuralContract{dlss_dir_env="GTAVXR_DLSS_DIR",models=new List<string>{"nvngx_dlss.dll"},settings_section="Graphics",keys=new List<string>{"upscale"},paths_env=new List<string>()}}));
                var nv=imp.ImportRelease("native-xr-neural-suite-v3",null);
                Check(nv.contract_sha256==SafeFs.Hash(launchFile)&&!imp.ContractStale(nv),"contract hash recorded; fresh import not stale");
                // What 2.3.9 stored: no modes, no loader, no graphics, no hash.
                nv.neural=null;nv.contract_sha256=null;nv.launch.session_mode="legacy-repo";nv.launch.settings_mode="legacy-repo";nv.runtime.RemoveAll(f=>f.path=="openxr_loader.dll");lib.SaveVersion(nv);
                Check(imp.ContractStale(lib.Version("167-neural")),"older-Suite manifest detected as stale");
                imp.ImportRelease("native-xr-neural-suite-v3",null);var back=lib.Version("167-neural");
                Check(back.neural!=null&&back.launch.session_mode=="suite"&&back.runtime.Any(f=>f.path=="openxr_loader.dll")&&!imp.ContractStale(back),"re-import restores graphics, mode and loader under the same id");
                File.AppendAllText(launchFile," ");Check(imp.ContractStale(lib.Version("167-neural")),"a changed contract file is stale again");
                // Disagreeing receipts are refused.
                string d3=Path.Combine(env.LegacySessionsRoot,"s3");Directory.CreateDirectory(d3);
                File.WriteAllText(Path.Combine(d3,"launch.json"),Json.Encode(new Dictionary<string,object>{{"release_sha256",sha},{"arguments",new[]{"-logfile","x","-other"}}}));
                bool threw=false;try{imp.RecordedArgs(sha);}catch(InvalidDataException){threw=true;}Check(threw,"disagreeing receipts refused");
            }
            public void ProjectPacks(){
                // The three fixed packs build 166 put in the library, used by three profiles.
                string src=Path.Combine(root,"src");
                Func<string,string,ModManifest> pack=(id,name)=>{var m=new ModManifest{id="content-"+id,name=name,version=id,category="map",loadable=true,kind="content-native",family="content-"+id,
                    files=new List<FileRef>{Put(src,"gtavxr_content/"+id+"/manifest.txt","GTAVXR_CONTENT 1\n"+id)},args=new List<string>{"-gtavxrContent="+id},added_utc=DateTime.UtcNow.ToString("o")};lib.SaveMod(m);return m;};
                var all3=pack("wov_v4ever","World of Variety + Plus + V4EVER");var two=pack("wov","World of Variety + Plus");var solo=pack("wov_solo","World of Variety");
                var imp=new Importers(env);
                Func<string,List<string>,Profile> mk=(name,mods)=>{string u=Path.Combine(root,"u"+Ids.New().Substring(0,6));Write(u,"settings.xml",Encoding.UTF8.GetBytes(SettingsXml));
                    var p=imp.CreateFrom(name,"155v2",u,UserTree.Snapshot(u),null,"test");p=lib.Profile(p.id);p.mods=mods;lib.SaveProfile(p);return p;};
                var pa=mk("A",new List<string>{mvga.id,all3.id});var pb=mk("B",new List<string>{two.id});var pc=mk("C",new List<string>{solo.id});
                // World of Variety alone: only the WoV-only pack can be replaced yet.
                string zip=Path.Combine(root,"WoV.zip");
                using(var z=System.IO.Compression.ZipFile.Open(zip,System.IO.Compression.ZipArchiveMode.Create))System.IO.Compression.ZipFileExtensions.CreateEntryFromFile(z,Oiv("World of Variety","10","update/update.rpf|common/data/levels/gta5/popcycle.dat"),"wov.oiv");
                var wov=imp.ImportArchive(zip);
                Check(lib.Mod(solo.id)==null&&lib.Profile(pc.id).mods.SequenceEqual(new[]{wov.id}),"WoV-only pack replaced by the WoV card");
                Check(lib.Mod(two.id)!=null&&lib.Mod(all3.id)!=null&&lib.Profile(pb.id).mods.SequenceEqual(new[]{two.id}),"packs with Plus stay until Plus is imported");
                // Plus and V4EVER from one folder: an .oiv and a loose pack (the player's files are only read).
                string folder=Path.Combine(root,"my mods");Directory.CreateDirectory(Path.Combine(folder,"update","x64","dlcpacks","v4evermod"));
                string loose=Path.Combine(folder,"update","x64","dlcpacks","v4evermod","dlc.rpf");File.WriteAllText(loose,"RPF7 v4ever");
                File.Copy(Oiv("World Of Variety Plus 2026","10","update/update.rpf|common/data/levels/gta5/popcycle.dat"),Path.Combine(folder,"World Of Variety Addon Pack.oiv"));
                imp.ImportArchive(folder);
                var plus=lib.Mods().Single(m=>m.source!=null&&m.source.catalog=="wovplus");var v4=lib.Mods().Single(m=>m.source!=null&&m.source.catalog=="v4ever");
                Check(File.Exists(loose)&&File.ReadAllText(loose)=="RPF7 v4ever"&&(File.GetAttributes(loose)&FileAttributes.ReadOnly)==0,"the player's loose pack stays where it was, untouched");
                Check(lib.Mod(two.id)==null&&lib.Mod(all3.id)==null,"all fixed packs gone");
                Check(lib.Profile(pa.id).mods.SequenceEqual(new[]{mvga.id,wov.id,plus.id,v4.id}),"A: MVGA kept, the pack becomes WoV + Plus + V4EVER");
                Check(lib.Profile(pb.id).mods.SequenceEqual(new[]{wov.id,plus.id}),"B: WoV + Plus");
                Check(lib.Mods().Count(m=>m.IsCard()&&m.kind=="content-source")==3,"three cards: WoV, Plus, V4EVER (V4EVER usable alone)");
                bool threw=false;try{imp.ImportArchive(Path.Combine(root,"src"));}catch(InvalidDataException e){threw=e.Message=="NO_CONTENT_MODS";}Check(threw,"a folder without content mods is refused");
            }
            // An .oiv package (zip) with one small content file per target ("archive|path" or a plain path).
            string Oiv(string name,string major,params string[] targets){
                string d=Path.Combine(root,"oiv-"+Ids.New().Substring(0,8));Directory.CreateDirectory(Path.Combine(d,"content"));
                var sb=new StringBuilder("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<package version=\"2.2\" target=\"Five\"><metadata><name>"+name+"</name><version><major>"+major+"</major><minor>0</minor></version><author><displayName>Fixture</displayName></author></metadata><content>");
                int i=0;
                foreach(string t in targets){
                    string src="f"+(i++)+".txt";File.WriteAllText(Path.Combine(d,"content",src),name+" "+t);int bar=t.IndexOf('|');
                    if(bar<0)sb.Append("<add source=\""+src+"\">"+t+"</add>");
                    else sb.Append("<archive path=\""+t.Substring(0,bar)+"\" createIfNotExist=\"False\" type=\"RPF7\"><add source=\""+src+"\">"+t.Substring(bar+1)+"</add></archive>");
                }
                sb.Append("</content></package>");File.WriteAllText(Path.Combine(d,"assembly.xml"),sb.ToString());
                string oiv=Path.Combine(root,Ids.New().Substring(0,8)+".oiv");System.IO.Compression.ZipFile.CreateFromDirectory(d,oiv);return oiv;
            }
            public void ContentMods(){
                var imp=new Importers(env);var cs=new ContentSources(env);
                // World of Variety inside a .zip with its No Umbrellas option, WoV Plus as a bare .oiv, V4EVER as a loose pack, a mod nobody knows.
                string zip=Path.Combine(root,"World of Variety 8.zip");
                using(var z=System.IO.Compression.ZipFile.Open(zip,System.IO.Compression.ZipArchiveMode.Create)){
                    System.IO.Compression.ZipFileExtensions.CreateEntryFromFile(z,Oiv("World of Variety","8","update/update.rpf|common/data/levels/gta5/popcycle.dat","update/update.rpf|x64/data/metadata/pedpersonality.ymt"),"World of Variety/World of Variety.oiv");
                    System.IO.Compression.ZipFileExtensions.CreateEntryFromFile(z,Oiv("NO UMBRELLAS","1","update/update.rpf|common/data/ai/scenarios.meta"),"Options/No Umbrellas.oiv");
                    using(var s=new StreamWriter(z.CreateEntry("Read me.txt").Open()))s.Write("read me");
                }
                var wov=imp.ImportArchive(zip);
                Check(wov.kind=="content-source"&&wov.source.catalog=="wov"&&wov.IsCard()&&wov.name=="World of Variety"&&wov.version=="8.0","WoV card from the zip, got "+wov.name+"/"+(wov.source==null?"-":wov.source.catalog));
                var nu=lib.Mods().Single(m=>m.source!=null&&m.source.catalog=="wov-no-umbrellas");
                Check(!nu.IsCard()&&nu.source.option_of=="wov","No Umbrellas kept as WoV's option, not a card");
                int count=lib.Mods().Count;Check(imp.ImportArchive(zip).id==wov.id&&lib.Mods().Count==count,"same archive again: nothing new");
                var plus=imp.ImportArchive(Oiv("World Of Variety Plus 10.1","10","update/update.rpf|common/data/levels/gta5/popcycle.dat"));
                Check(plus.source.catalog=="wovplus"&&plus.IsCard(),"WoV Plus card");
                string v4zip=Path.Combine(root,"V4EVER.zip");
                using(var z=System.IO.Compression.ZipFile.Open(v4zip,System.IO.Compression.ZipArchiveMode.Create))using(var s=new StreamWriter(z.CreateEntry("V4EVER/v4evermod/dlc.rpf").Open()))s.Write("RPF7 fixture");
                var v4=imp.ImportArchive(v4zip);
                Check(v4.source.format=="pack"&&v4.source.pack=="v4evermod"&&v4.source.catalog=="v4ever"&&v4.files.Single().path=="dlcpacks/v4evermod/dlc.rpf","V4EVER pack card");
                var own=imp.ImportArchive(Oiv("My Own Cars","2","update/update.rpf|common/data/carcols.ymt"));
                Check(own.source.catalog==null&&own.IsCard()&&own.name=="My Own Cars"&&own.version=="2.0","an unknown .oiv is one on/off card");
                // The builder reads the mods through links to the store; rebuilding that view never unprotects the stored files.
                string tree=cs.Tree(wov);var f0=wov.files.First(f=>f.path.StartsWith("content/"));
                Check(File.ReadAllText(SafeFs.Under(tree,f0.path))==File.ReadAllText(blobs.PathOf(f0.sha256)),"source view");
                SafeFs.DeleteName(SafeFs.Under(tree,f0.path));cs.Tree(wov);
                Check((File.GetAttributes(blobs.PathOf(f0.sha256))&FileAttributes.ReadOnly)!=0,"stored file still read-only after the view was rebuilt");

                // Options of a profile on a version that reads content profiles.
                var v3=lib.Version("155v2");v3.id="166";v3.build="166";v3.content="profile";v3.mvga="none";v3.mvga_arg=null;lib.SaveVersion(v3);
                string uc=Path.Combine(root,"uc");Write(uc,"settings.xml",Encoding.UTF8.GetBytes(SettingsXml.Replace("</video>","</video>\r\n  <graphics>\r\n    <PedVarietyMultiplier value=\"1.000000\" />\r\n    <VehicleVarietyMultiplier value=\"1.000000\" />\r\n  </graphics>")));
                var c=imp.CreateFrom("Mundo","166",uc,UserTree.Snapshot(uc),null,"test");
                var onlyPlus=cs.Plan(c,new List<ModManifest>{plus});
                Check(onlyPlus.Sources.Count==0&&onlyPlus.Skipped.SequenceEqual(new[]{plus.id}),"WoV Plus without World of Variety is left out");
                Check(cs.Build(c,v3,new List<ModManifest>{plus},null)==null,"nothing to build without World of Variety");
                c.mods=new List<string>{plus.id,wov.id,v4.id};   // listed out of order on purpose
                c.options=new Dictionary<string,string>{{"wovplus.extra_density","on"},{"wovplus.extra_density.peds_density","120"},{"wov.traffic.deformation","off"},{"wovplus.deformation","plus"},{"wov.peds.umbrellas","off"},{"wov.variety","2.0"}};
                lib.SaveProfile(c);c=lib.Profile(c.id);
                var srcs=c.mods.Select(x=>lib.Mod(x)).ToList();
                var plan=cs.Plan(c,srcs);
                Check(plan.Sources.SequenceEqual(new[]{wov.id,nu.id,plus.id,v4.id}),"merge order WoV, its No Umbrellas option, Plus, V4EVER; got "+String.Join(",",plan.Sources.Select(x=>lib.Mod(x).name)));
                Check(plan.Options.DensitySource==plus.id&&plan.Options.SourceDensity,"Plus is the density source and its density is on");
                string g;
                Check(plan.Options.Gameconfig.TryGetValue("AmbientPedsMultiplier",out g)&&g=="120"&&plan.Options.Gameconfig["AmbientPedsMultiplier_Base"]=="84"&&plan.Options.Gameconfig["VehicleAmbientDensityMultiplier"]=="150","density sliders to gameconfig values, got "+String.Join(",",plan.Options.Gameconfig.Select(x=>x.Key+"="+x.Value))+" sd="+plan.Options.SourceDensity);
                var plusStates=Content.ContentCatalog.States(Content.ContentCatalog.Mod("wovplus"),c.Option,"en",Content.ContentCatalog.States(Content.ContentCatalog.Mod("wov"),c.Option,"en"));
                var deform=plusStates.Single(x=>x.Key=="wovplus.deformation");
                Check(deform.Value=="game"&&deform.Blocked!=null,"Plus deformation falls back to the game's when WoV deformation is off");
                Check(plan.Options.Rules.Any(r=>r.Source==plus.id&&r.Kind=="fields"&&r.Target=="*handling.meta"),"game deformation rules on Plus");
                Check(plan.SettingsXml["PedVarietyMultiplier"]=="2.000000","variety option");
                string key=cs.Key(plan,v3);
                Check(cs.Key(cs.Plan(c,srcs),v3)==key,"same choice, same key");
                // A version whose EXE reads pedpersonality as XML gets its own build (its merge is served; elsewhere refused).
                var psoXml=v3.pso_xml;v3.pso_xml=new List<string>{"CPedModelInfo__PersonalityDataList"};
                Check(cs.Key(cs.Plan(c,srcs),v3)!=key,"pso_xml in the key");v3.pso_xml=psoXml;
                var bo=new GtavxrSuite.Content.BuildOptions();
                Check(GtavxrSuite.Content.ContentBuilder.PsoXmlRefusal("CPedModelInfo__PersonalityDataList",bo)!=null,"pedpersonality merge refused where the EXE reads only PSO");
                Check(GtavxrSuite.Content.ContentBuilder.PsoXmlRefusal("CPedModelInfo__InitDataList",bo)==null&&GtavxrSuite.Content.ContentBuilder.PsoXmlRefusal("CScenarioPointManifest",bo)==null,
                    "roots GTA loads through fwPsoStoreLoader stay XML");
                bo.PsoXmlRoots.Add("CPedModelInfo__PersonalityDataList");
                Check(GtavxrSuite.Content.ContentBuilder.PsoXmlRefusal("CPedModelInfo__PersonalityDataList",bo)==null&&GtavxrSuite.Content.ContentBuilder.PsoXmlRefusal("camMetadataStore",bo)!=null,
                    "served only for the roots the version lists");
                c.options["wovplus.extra_density.peds_density"]="130";Check(cs.Key(cs.Plan(c,srcs),v3)!=key,"another option, another key");c.options["wovplus.extra_density.peds_density"]="120";
                // WoV traffic off: a Plus option that needs it and has no fallback applies nothing.
                c.options["wov.traffic"]="off";var off=cs.Plan(c,srcs);c.options.Remove("wov.traffic");
                Check(off.Options.Off.Contains("traffic")&&!off.Options.Rules.Any(r=>r.Target=="*handling.meta"),"blocked choice without fallback applies nothing");
                // Play with a profile already built for this choice: it is reused, the game gets it as links, the variety goes to settings.xml.
                var built=new ModManifest{id="content-built-"+key.Substring(0,16).ToLowerInvariant(),name="World of Variety + World Of Variety Plus + V4EVER",version=key.Substring(0,8),category="map",added_utc=DateTime.UtcNow.ToString("o"),
                    loadable=true,kind="content-native",family="content-built",built_key=key,args=new List<string>{"-gtavxrContent=suite"},
                    files=new List<FileRef>{Put(Path.Combine(root,"src"),"gtavxr_content/suite/manifest.txt","GTAVXR_CONTENT 1\n"),Put(Path.Combine(root,"src"),"gtavxr_content/suite/dlcpacks/v4evermod/dlc.rpf","RPF7 built")}};
                lib.SaveMod(built);
                Check(cs.Build(c,v3,srcs,null).id==built.id,"same choice reuses the built profile");
                var play=new PlaySession(env).Play(c.id,null);
                Check(play.result=="played"&&play.mods.Contains(built.id)&&!play.mods.Contains(wov.id),"Play serves the built profile, not the sources");
                var rep=Report(play);
                Check(((string)rep["args"]).Contains("-gtavxrContent=suite"),"content profile argument");
                Check(((string)rep["args"]).Contains(ContentSources.HeapArg),"content profile: larger game heap argument");
                Check(((string)rep["settings_seen"]).Contains("<PedVarietyMultiplier value=\"2.000000\" />"),"variety in the profile's settings.xml");
                string inGame=Path.Combine(env.GameDir,"gtavxr_content","suite","dlcpacks","v4evermod","dlc.rpf");var pf=built.files[1];
                Check(File.Exists(inGame)&&(File.GetAttributes(inGame)&FileAttributes.ReadOnly)!=0,"content file in the game, protected like the stored one");
                // Mods off: the content profile leaves the game and the stored files stay protected.
                c=lib.Profile(c.id);c.mods.Clear();lib.SaveProfile(c);
                var r2=new PlaySession(env).Play(c.id,null);
                Check(r2.result=="played"&&!File.Exists(inGame),"content profile removed with the mods off");
                Check((File.GetAttributes(blobs.PathOf(pf.sha256))&FileAttributes.ReadOnly)!=0,"stored file still read-only after the game copy was removed");
                Check(((string)Report(r2)["settings_seen"]).Contains("<PedVarietyMultiplier value=\"1.000000\" />"),"variety back to the game's 1.0 without World of Variety");
            }
            public void ArchiveMod(string fresh){
                string zip=Path.Combine(root,"DriveV 7.5.zip");
                using(var z=System.IO.Compression.ZipFile.Open(zip,System.IO.Compression.ZipArchiveMode.Create)){var e=z.CreateEntry("update/x64/data/handling.meta");using(var s=new StreamWriter(e.Open()))s.Write("<CHandlingDataMgr/>");}
                var m=new Importers(env).ImportArchive(zip);
                Check(!m.loadable&&m.category=="driving"&&m.archive!=null&&new Blobs(env).Has(m.archive.sha256,m.archive.bytes),"kept, not loadable, driving");
                var p=lib.Profiles().First();p.mods.Add(m.id);lib.SaveProfile(p);
                var removedFrom=lib.DeleteMod(m.id);Check(removedFrom.Count==1&&!lib.Profile(p.id).mods.Contains(m.id),"delete removes it from profiles");
            }
        }
    }
    public class SimulatedSuiteDeath : Exception { public SimulatedSuiteDeath():base("simulated"){} }

    // The fixture "game": records what it sees, changes the save and VR settings like a real session, exits.
    public static class FixtureGame {
        public static int Run(string[] args){
            string session=Environment.GetEnvironmentVariable("GTAVXR_SESSION_DIR");
            string sroot=Environment.GetEnvironmentVariable("GTAVXR_SESSIONS_ROOT");
            string fresh=!String.IsNullOrEmpty(sroot)?Path.GetDirectoryName(Path.GetDirectoryName(sroot))
                :Path.GetDirectoryName(Path.GetDirectoryName(Path.GetDirectoryName(Path.GetDirectoryName(Path.GetDirectoryName(session)))));
            var env=Env.ForFixture(fresh);
            string vrFile=Environment.GetEnvironmentVariable("GTAVXR_VR_SETTINGS");if(String.IsNullOrEmpty(vrFile))vrFile=env.LegacySettingsPath;
            string loader=Environment.GetEnvironmentVariable("GTAVXR_LOADER_PATH");
            string save=Directory.GetFiles(env.UserDataDir,"SGTA5*",SearchOption.AllDirectories).Select(f=>SaveInfo.Read(f)).Where(s=>s!=null).Select(s=>File.ReadAllBytes(s.file)).Select(b=>Encoding.Unicode.GetString(b,4,0x200).Split('\0')[0]).FirstOrDefault()??"";
            string leaseFile=Path.Combine(env.DevRepo,"local-cache","agent-locks","play-runtime.lease");string leaseAgent="";
            if(File.Exists(leaseFile)){var l=Json.Read<Dictionary<string,object>>(leaseFile);leaseAgent=l["agent"] as string;}
            var rep=new Dictionary<string,object>{{"save_seen",save},{"vr_seen",File.Exists(vrFile)?File.ReadAllText(vrFile):""},
                {"settings_seen",File.ReadAllText(Path.Combine(env.UserDataDir,"settings.xml"))},{"user_play",Environment.GetEnvironmentVariable("GTAVXR_USER_PLAY")},
                {"session_dir",session},{"bridge",Environment.GetEnvironmentVariable("GTAVXR_BRIDGE_PATH")},{"layer",Environment.GetEnvironmentVariable("DISABLE_XR_APILAYER_fixture")},
                {"args",String.Join(" ",args)},{"lease_agent",leaseAgent},{"rpf",SafeFs.Hash(Path.Combine(env.GameDir,"update","update.rpf"))},
                {"mvga_seen",File.Exists(Path.Combine(env.GameDir,"gtavxr_mvga147","common","data","timecycle.xml"))?"1":"0"},
                {"sessions_root",sroot},{"vr_settings",Environment.GetEnvironmentVariable("GTAVXR_VR_SETTINGS")},{"loader",loader},
                {"loader_seen",!String.IsNullOrEmpty(loader)&&File.Exists(loader)?File.ReadAllText(loader):""},{"xr_json",Environment.GetEnvironmentVariable("XR_RUNTIME_JSON")},
                {"dlss_dir",Environment.GetEnvironmentVariable("GTAVXR_DLSS_DIR")},{"gx_dev_root",Environment.GetEnvironmentVariable("GX_DEV_ROOT")}};
            // script host sessions: the host folder and the game's scripts/ as the game sees them
            string scriptsPath=Environment.GetEnvironmentVariable("GTAVXR_SCRIPTS_PATH")??"",live=Path.Combine(env.GameDir,"scripts");
            Func<string,string> tree=d=>d.Length>0&&Directory.Exists(d)?String.Join("|",Directory.GetFiles(d,"*",SearchOption.AllDirectories).Select(f=>f.Substring(d.TrimEnd('\\').Length+1).Replace('\\','/')).OrderBy(x=>x,StringComparer.Ordinal)):"";
            rep["scripts_path"]=scriptsPath;rep["host_seen"]=tree(scriptsPath);rep["scripts_seen"]=tree(live);
            rep["scripts_ini"]=File.Exists(Path.Combine(live,"OpenWorldInteriors.ini"))?File.ReadAllText(Path.Combine(live,"OpenWorldInteriors.ini")):"";
            rep["shvdn_root"]=File.Exists(Path.Combine(env.GameDir,"ScriptHookVDotNet3.dll"))?"1":"0";
            // 2.4.4: .asi mods the host is told to load (GTAVXR_ASI_FILES) and their files beside them
            string asiEnv=Environment.GetEnvironmentVariable("GTAVXR_ASI_FILES")??"";var asiList=asiEnv.Length>0?asiEnv.Split('|'):new string[0];
            rep["asi_env"]=asiEnv;
            rep["asi_renamed"]=asiList.Length>0&&asiList.All(a=>{if(!File.Exists(a))return false;string s=Encoding.ASCII.GetString(File.ReadAllBytes(a));return s.Contains("GTAVXRHost.dll")&&!s.Contains("ScriptHookV.dll");})?"1":"0";
            string asiCfg=Path.Combine(env.GameDir,"MyTrainer","config.ini"),asiIni=Path.Combine(env.GameDir,"MyTrainer.ini");
            rep["asi_config"]=File.Exists(asiCfg)?File.ReadAllText(asiCfg):"";rep["asi_ini"]=File.Exists(asiIni)?File.ReadAllText(asiIni):"";
            File.WriteAllText(Path.Combine(session,"fixture-report.json"),Json.Encode(rep));
            if(args.Contains("--fixture-no-write"))return 0;
            // an .asi mod changing its settings and writing a log beside itself; something else writing a file of its own
            foreach(string a in asiList){
                string cfg=Path.Combine(Path.ChangeExtension(a,null),"config.ini");if(File.Exists(cfg))File.AppendAllText(cfg,"changed=1\n");
                File.WriteAllText(Path.ChangeExtension(a,".log"),"log of "+Path.GetFileName(a));
            }
            if(asiList.Length>0)File.WriteAllText(Path.Combine(env.GameDir,"unrelated.txt"),"not the mod's");
            bool dies=args.Contains("--suite-dies"),hang=args.Contains("--fixture-hang");
            // a script mod saving something (an outfit) in its scripts/ folder
            if(scriptsPath.Length>0&&Directory.Exists(live)){
                Directory.CreateDirectory(Path.Combine(live,"Outfits"));
                File.WriteAllText(Path.Combine(live,"Outfits",dies?"crash.xml":"new.xml"),dies?"saved before the crash":"saved in game");
            }
            string title=dies?"(Autoguardado) Tras corte (60.0%) - y":hang?"(Autoguardado) Colgado (10.0%) - z":"(Autoguardado) Prueba (50.0%) - y";
            string target=Path.Combine(env.UserDataDir,"Default","SGTA50015");Directory.CreateDirectory(Path.GetDirectoryName(target));
            File.WriteAllBytes(target,EngineTests.FakeSave(title,8192));
            if(File.Exists(vrFile))File.AppendAllText(vrFile,"changed=1\n");
            if(args.Contains("--fixture-stray")){
                // A badly behaved test: new files in the game folder, a game file and a GTAVXR file changed.
                Directory.CreateDirectory(Path.Combine(env.GameDir,"stray"));File.WriteAllText(Path.Combine(env.GameDir,"stray","cache.bin"),"stray");
                File.WriteAllText(Path.Combine(env.GameDir,"GTA5_crash.log"),"crash");
                File.AppendAllText(Path.Combine(env.GameDir,"x64a.rpf"),"CHANGED BY THE TEST");
                string quiet=Path.Combine(env.GameDir,"x64b.rpf");var when=File.GetLastWriteTimeUtc(quiet);
                File.WriteAllText(quiet,"FIXTURE GAME DATX");File.SetLastWriteTimeUtc(quiet,when);
                File.Delete(Path.Combine(env.GameDir,"x64c.rpf"));
                File.AppendAllText(Path.Combine(env.GameDir,"common","shaders","s.fxc"),"TOUCHED");
            }
            // Stuck in its own exit, like the source build: announces the final exit (only when the Suite placed in-<pid>), then never ends.
            if(args.Contains("--fixture-exit-hang")){
                string pid=Process.GetCurrentProcess().Id.ToString();
                // The Suite places in-<pid> just after the start: wait for it like the real game, which exits much later.
                for(int i=0;i<200&&!File.Exists(Path.Combine(env.ExitMarkerDir,"in-"+pid));++i)System.Threading.Thread.Sleep(50);
                if(File.Exists(Path.Combine(env.ExitMarkerDir,"in-"+pid)))File.WriteAllText(Path.Combine(env.ExitMarkerDir,"out-"+pid),"");
                while(true)System.Threading.Thread.Sleep(250);
            }
            // "Does not load": stays open until the Suite ends it.
            if(hang)while(true)System.Threading.Thread.Sleep(250);
            if(dies){
                // Tell the Suite test harness to stop following this session (simulated Suite death).
                File.WriteAllText(Path.Combine(session,"suite-dies.flag"),"1");
            }
            return 0;
        }
    }
}
