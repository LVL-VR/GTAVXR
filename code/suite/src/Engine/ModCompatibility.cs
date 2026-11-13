using System;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace GtavxrSuite {
    sealed class ModCompatibility {
        readonly Blobs blobs;
        readonly string runtime;
        readonly string[] api;
        readonly Dictionary<string,HashSet<string>> exports=new Dictionary<string,HashSet<string>>(StringComparer.OrdinalIgnoreCase);
        readonly Dictionary<string,HashSet<int>> ordinals=new Dictionary<string,HashSet<int>>(StringComparer.OrdinalIgnoreCase);

        public ModCompatibility(Env env,string runtime) {
            blobs=new Blobs(env);
            this.runtime=runtime;
            api=new[]{"scripthost/ScriptHookVDotNet.asi","scripthost/ScriptHookVDotNet2.dll","scripthost/ScriptHookVDotNet3.dll"}
                .Select(path=>SafeFs.Under(runtime,path)).Where(File.Exists).ToArray();
        }

        public string Check(ModManifest mod) {
            if(mod.shv_by_name!=null&&mod.shv_by_name.Count!=0) return "MOD_DYNAMIC_LOOKUP · "+mod.shv_by_name[0];
            var files=(mod.script_files??new List<FileRef>()).Concat(mod.asi_files??new List<FileRef>())
                .Where(f=>new[]{".dll",".asi"}.Contains(Path.GetExtension(f.path),StringComparer.OrdinalIgnoreCase)).ToList();
            if(files.Count>128) return "MOD_INSPECTION_LIMIT";
            var managed=new List<string>();
            try {
                foreach(var file in files){
                    blobs.Verify(file.sha256,file.bytes);
                    string path=blobs.PathOf(file.sha256);
                    using(var pe=new PeInspection(path)){
                        if(pe.Machine!=0x8664 && !(pe.Machine==0x14c&&pe.Managed&&pe.IlOnly&&!pe.Requires32Bit))
                            return "MOD_ARCHITECTURE · "+file.path;
                        var imports=pe.Imports();
                        foreach(var import in imports){
                            string issue=CheckImport(import,files);
                            if(issue!=null) return issue+" · "+file.path;
                        }
                        if(pe.Managed) managed.Add(path);
                        if(file.path.EndsWith(".asi",StringComparison.OrdinalIgnoreCase)){
                            if(!imports.Any(i=>String.Equals(i.Library,ShvImports.HostDll,StringComparison.OrdinalIgnoreCase)))
                                return "MOD_RETAIL_OR_DYNAMIC_ASI · "+file.path;
                        }
                    }
                }
                if(managed.Count!=0){
                    var domain=AppDomain.CreateDomain("GTAVXR mod metadata",null,new AppDomainSetup {
                        ApplicationBase=Path.GetDirectoryName(typeof(ModCompatibility).Assembly.Location),
                        DisallowBindingRedirects=true, DisallowCodeDownload=true
                    });
                    try {
                        var inspector=(ManagedModInspector)domain.CreateInstanceAndUnwrap(typeof(ManagedModInspector).Assembly.FullName,
                                                                                          typeof(ManagedModInspector).FullName);
                        var result=inspector.Check(managed.ToArray(),api);
                        if(result.Error!=null) return result.Error;
                        foreach(var import in result.Imports){
                            string issue=CheckImport(import,files);
                            if(issue!=null) return issue;
                        }
                    } finally { AppDomain.Unload(domain); }
                }
            } catch(Exception error) {
                if(!(error is IOException||error is InvalidDataException||error is UnauthorizedAccessException||error is BadImageFormatException||
                     error is ArgumentException||error is OverflowException||error is InvalidOperationException)) throw;
                return "MOD_INSPECTION_FAILED · "+error.GetType().Name;
            }
            return null;
        }

        string CheckImport(PeImport import,List<FileRef> files) {
            string library=import.Library;
            if(String.IsNullOrWhiteSpace(library)||Path.GetFileName(library)!=library||library.IndexOf(':')>=0)
                return "MOD_LIBRARY_PATH · "+library;
            if(!library.EndsWith(".dll",StringComparison.OrdinalIgnoreCase)&&!library.EndsWith(".exe",StringComparison.OrdinalIgnoreCase))
                library+=".dll";
            if(ShvImports.IsShvName(library)||library.EndsWith(".exe",StringComparison.OrdinalIgnoreCase))
                return "MOD_RETAIL_LINK · "+library;
            if(library.StartsWith("api-ms-win-",StringComparison.OrdinalIgnoreCase)||library.StartsWith("ext-ms-win-",StringComparison.OrdinalIgnoreCase))
                return null; // API-set names are resolved by the OS loader.
            string path;
            if(library.Equals(ShvImports.HostDll,StringComparison.OrdinalIgnoreCase))
                path=SafeFs.Under(runtime,ShvImports.HostDll);
            else {
                var matches=files.Where(f=>Path.GetFileName(f.path).Equals(library,StringComparison.OrdinalIgnoreCase)).ToList();
                if(matches.Count>1) return "MOD_LIBRARY_AMBIGUOUS · "+library;
                path=matches.Count==1?blobs.PathOf(matches[0].sha256):Path.Combine(Environment.SystemDirectory,library);
            }
            string entry=import.Name??("#"+import.Ordinal);
            string request=library+"!"+entry+(import.Delayed?" (delay)":"");
            if(!File.Exists(path)) return "MOD_LIBRARY_MISSING · "+request;
            HashSet<string> names;
            if(import.Name!=null&&exports.TryGetValue(path,out names)) return HasName(names,import)?null:"MOD_EXPORT_MISSING · "+request;
            HashSet<int> numbers;
            if(import.Name==null&&ordinals.TryGetValue(path,out numbers)) return numbers.Contains(import.Ordinal)?null:"MOD_EXPORT_MISSING · "+request;
            using(var provider=new PeInspection(path)){
                if(provider.Machine!=0x8664) return "MOD_LIBRARY_ARCHITECTURE · "+request;
                if(import.Name==null){
                    numbers=provider.ExportOrdinals();
                    ordinals.Add(path,numbers);
                    return numbers.Contains(import.Ordinal)?null:"MOD_EXPORT_MISSING · "+request;
                }
                names=provider.ExportNames();
                exports.Add(path,names);
                if(!HasName(names,import))
                    return "MOD_EXPORT_MISSING · "+request;
            }
            return null;
        }

        static bool HasName(HashSet<string> names,PeImport import){
            return names.Contains(import.Name)||(import.Suffix!=null&&names.Contains(import.Name+import.Suffix));
        }
    }

    [Serializable]
    public sealed class ManagedModResult {
        public string Error;
        internal readonly List<PeImport> Imports=new List<PeImport>();
    }

    // A separate reflection-only context per mod. No assembly entry point, static constructor or custom attribute is invoked.
    public sealed class ManagedModInspector : MarshalByRefObject {
        public ManagedModResult Check(string[] plugins,string[] sealedApi) {
            var result=new ManagedModResult();
            var sources=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);
            var loaded=new Dictionary<string,Assembly>(StringComparer.OrdinalIgnoreCase);
            var apiNames=new HashSet<string>(sealedApi.Where(path=>path.EndsWith(".dll",StringComparison.OrdinalIgnoreCase))
                .Select(path=>AssemblyName.GetAssemblyName(path).Name),StringComparer.OrdinalIgnoreCase);
            foreach(string path in sealedApi.Concat(plugins)){
                if(new FileInfo(path).Length>64L*1024*1024) {result.Error="MOD_CLR_INSPECTION_LIMIT";return result;}
                var name=AssemblyName.GetAssemblyName(path);
                if(sources.ContainsKey(name.Name)) { result.Error="MOD_CLR_AMBIGUOUS · "+name.Name;return result; }
                sources.Add(name.Name,path);
            }
            ResolveEventHandler resolve=(sender,eventArgs)=>{
                var name=new AssemblyName(eventArgs.Name);
                string path;
                if(!sources.TryGetValue(name.Name,out path)){
                    if(name.Name=="mscorlib"||name.Name=="System"||name.Name.StartsWith("System.",StringComparison.Ordinal)||
                       name.Name=="Microsoft.CSharp"||name.Name=="Microsoft.VisualBasic"||name.Name=="netstandard"||name.Name=="WindowsBase")
                        return Assembly.ReflectionOnlyLoad(eventArgs.Name);
                    return null;
                }
                var provider=AssemblyName.GetAssemblyName(path);
                if(!(name.GetPublicKeyToken()??new byte[0]).SequenceEqual(provider.GetPublicKeyToken()??new byte[0]))return null;
                if(apiNames.Contains(name.Name)&&!HostVersionSupports(name.Version,provider.Version))return null;
                Assembly found;
                if(!loaded.TryGetValue(name.Name,out found)){
                    found=Assembly.ReflectionOnlyLoad(File.ReadAllBytes(path));
                    loaded.Add(name.Name,found);
                }
                return found;
            };
            AppDomain.CurrentDomain.ReflectionOnlyAssemblyResolve+=resolve;
            try {
                foreach(string path in plugins){
                    var bytes=File.ReadAllBytes(path);
                    var counts=ShvImports.ReferenceCounts(bytes);
                    if(counts.Any(count=>count>65536)) {result.Error="MOD_CLR_INSPECTION_LIMIT";return result;}
                    var assembly=resolve(this,new ResolveEventArgs(AssemblyName.GetAssemblyName(path).FullName));
                    if(assembly==null) {result.Error="MOD_CLR_ASSEMBLY";return result;}
                    foreach(var required in assembly.GetReferencedAssemblies()){
                        if(!apiNames.Contains(required.Name)) continue;
                        var provided=AssemblyName.GetAssemblyName(sources[required.Name]);
                        if(!HostVersionSupports(required.Version,provided.Version)){
                            result.Error="MOD_CLR_API_REVISION · "+required.Name+" "+required.Version+" → "+provided.Version;
                            return result;
                        }
                    }
                    var module=assembly.ManifestModule;
                    for(int i=1;i<=counts[0];i++) module.ResolveType(0x01000000|i);
                    for(int i=1;i<=counts[1];i++) module.ResolveMember(0x0a000000|i);
                    foreach(var type in assembly.GetTypes())
                        foreach(var method in type.GetMethods(BindingFlags.DeclaredOnly|BindingFlags.Static|BindingFlags.Instance|
                                                              BindingFlags.Public|BindingFlags.NonPublic)) {
                            if((method.Attributes&MethodAttributes.PinvokeImpl)==0) continue;
                            var data=method.GetCustomAttributesData().SingleOrDefault(a=>a.AttributeType.FullName==typeof(DllImportAttribute).FullName);
                            if(data==null){result.Error="MOD_PINVOKE_UNRESOLVED · "+method.Name;return result;}
                            string library=(string)data.ConstructorArguments[0].Value;
                            string entry=method.Name;
                            bool exact=false;
                            var charset=CharSet.None;
                            foreach(var argument in data.NamedArguments){
                                if(argument.MemberName=="EntryPoint"&&argument.TypedValue.Value!=null) entry=(string)argument.TypedValue.Value;
                                if(argument.MemberName=="ExactSpelling") exact=(bool)argument.TypedValue.Value;
                                if(argument.MemberName=="CharSet") charset=(CharSet)Convert.ToInt32(argument.TypedValue.Value);
                            }
                            int ordinal;
                            result.Imports.Add(entry.StartsWith("#",StringComparison.Ordinal)&&Int32.TryParse(entry.Substring(1),out ordinal)?
                                new PeImport{Library=library,Ordinal=ordinal}:new PeImport{Library=library,Name=entry,Suffix=exact?null:
                                    (charset==CharSet.Unicode||charset==CharSet.Auto?"W":"A")});
                        }
                }
            } catch(Exception error) {
                if(!(error is TypeLoadException||error is MissingMemberException||error is IOException||error is InvalidDataException||
                     error is BadImageFormatException||error is ArgumentException||error is InvalidOperationException||
                     error is NotSupportedException||error is ReflectionTypeLoadException)) throw;
                result.Error="MOD_CLR_UNRESOLVED · "+error.Message;
            } finally { AppDomain.CurrentDomain.ReflectionOnlyAssemblyResolve-=resolve; }
            return result;
        }

        static bool HostVersionSupports(Version required,Version provided) {
            if(required==new Version(0,0,0,0)) return provided.Major==2;
            return required.Major==provided.Major&&provided>=required;
        }
    }
}
