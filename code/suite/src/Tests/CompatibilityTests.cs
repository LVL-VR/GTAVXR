using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace GtavxrSuite {
    static class CompatibilityTests {
        static void Check(bool value,string name){
            if(!value) throw new InvalidDataException("COMPAT_FIXTURE:"+name);
        }

        internal static void Platform(string fresh){
            Check(PlatformCompatibility.SessionPathProblem(Path.Combine(fresh,"Biblioteca VR"))==null,"local path with spaces");
            Check(PlatformCompatibility.SessionPathProblem(Path.Combine(fresh,new String('x',380)))=="DATA_PATH_TOO_LONG","native buffer");
            string unicode=Path.Combine(fresh,"VR-🧪");
            Check(PlatformCompatibility.SessionPathProblem(unicode)==(PlaySession.AnsiPath(unicode)?null:"DATA_PATH_NOT_ANSI"),"actual ANSI consumer");
            Check(PlatformCompatibility.ScriptHostProblem(528040,_=>true)==null,"supported .NET and CRT");
            Check(PlatformCompatibility.ScriptHostProblem(528039,_=>true)=="HOST_NET48_REQUIRED","old framework");
            Check(PlatformCompatibility.ScriptHostProblem(533325,n=>n!="vcruntime140.dll")==
                  "HOST_SYSTEM_DEPENDENCY · vcruntime140.dll","missing VC runtime");
        }

        internal static void LargeOpenXr(string fresh){
            Directory.CreateDirectory(fresh);
            string path=Path.Combine(fresh,"runtime.dll"),manifest=Path.Combine(fresh,"moved.json");
            NeuralSuiteTests.FakeDll(path,"Fixture","Runtime","9.0.0.0",new[]{"xrNegotiationAlias"});
            using(var stream=File.OpenWrite(path)) stream.SetLength(129L*1024*1024);
            Json.New(manifest,new{runtime=new{library_path="runtime.dll",functions=new{xrNegotiateLoaderRuntimeInterface="xrNegotiationAlias"}}});
            var selected=XrRuntimes.Resolve(manifest,null);
            Check(XrRuntimes.NegotiationProblem(selected)==null,"large library with a valid export");
            selected.negotiation_export="missing";
            Check(XrRuntimes.NegotiationProblem(selected)=="XR_NEGOTIATION_EXPORT_MISSING","large library missing the export");
        }

        internal static object ModContracts(string fresh,string input){
            var env=Env.ForFixture(fresh);
            var blobs=new Blobs(env);
            string runtime=env.Dir("runtime");
            string provider=SafeFs.Under(runtime,"scripthost/ScriptHookVDotNet2.dll");
            Directory.CreateDirectory(Path.GetDirectoryName(provider));
            var mod=new ModManifest{kind="script",script_files=new List<FileRef>{
                blobs.PutFile(SafeFs.Under(input,"LegacyConsumer.dll"),"LegacyConsumer.dll")
            }};
            File.Copy(SafeFs.Under(input,"compatible/ScriptHookVDotNet2.dll"),provider);
            string issue=new ModCompatibility(env,runtime).Check(mod);
            Check(issue==null,"compatible assembly revision "+issue);
            File.Copy(SafeFs.Under(input,"missing/ScriptHookVDotNet2.dll"),provider,true);
            issue=new ModCompatibility(env,runtime).Check(mod);
            Check(issue!=null&&issue.StartsWith("MOD_CLR_UNRESOLVED",StringComparison.Ordinal),"different required method signature "+issue);
            string incompatible=issue;
            File.Copy(SafeFs.Under(input,"compatible/ScriptHookVDotNet2.dll"),provider,true);
            string third=SafeFs.Under(runtime,"scripthost/ScriptHookVDotNet3.dll");
            File.Copy(SafeFs.Under(input,"compatible/ScriptHookVDotNet3.dll"),third);
            mod.script_files.Add(blobs.PutFile(SafeFs.Under(input,"ModernConsumer.dll"),"ModernConsumer.dll"));
            issue=new ModCompatibility(env,runtime).Check(mod);
            Check(issue==null,"v2 and v3 consumers in one selection "+issue);
            var native=new ModManifest{kind="script",asi_files=new List<FileRef>{
                blobs.PutFile(SafeFs.Under(input,"NativeConsumer.asi"),"NativeConsumer.asi")
            }};
            string host=SafeFs.Under(runtime,ShvImports.HostDll);
            NeuralSuiteTests.FakeDll(host,"Fixture","Host","2.0.0.0",new[]{"RequiredNative"});
            issue=new ModCompatibility(env,runtime).Check(native);
            Check(issue==null,"named and ordinal imports including delay import "+issue);
            NeuralSuiteTests.FakeDll(host,"Fixture","Host","2.1.0.0",new[]{"OtherSymbol"});
            issue=new ModCompatibility(env,runtime).Check(native);
            Check(issue!=null&&issue.Contains("MOD_EXPORT_MISSING")&&issue.Contains("RequiredNative"),"incompatible export "+issue);
            string missingExport=issue;
            NeuralSuiteTests.FakeDll(host,"Fixture","Host","2.1.0.0",new[]{"RequiredNative"});
            var pinvoke=new ModManifest{kind="script",script_files=new List<FileRef>{
                blobs.PutFile(SafeFs.Under(input,"InteropConsumer.dll"),"InteropConsumer.dll")
            }};
            issue=new ModCompatibility(env,runtime).Check(pinvoke);
            Check(issue==null,"CLR P/Invoke resolves against sealed native Host "+issue);
            File.Delete(host);
            issue=new ModCompatibility(env,runtime).Check(pinvoke);
            Check(issue!=null&&issue.Contains("MOD_LIBRARY_MISSING"),"unavailable dependency "+issue);
            return new {status="PASS_MOD_COMPATIBILITY",managed_negative=incompatible,native_negative=missingExport,
                         dependency_negative=issue,read_only_inspection=true,gta_launched=false};
        }
    }
}
