using System;
using System.IO;
using System.Linq;
using System.Collections.Generic;
using Microsoft.Win32;

namespace GtavxrSuite {
    // An OpenXR runtime manifest as Windows registers it, read without loading anything.
    public sealed class XrRuntime {
        public string manifest {get;set;}
        public string name {get;set;}        // Virtual Desktop | SteamVR | Meta Quest Link | the manifest's runtime name
        public string library {get;set;}     // runtime.library_path resolved against the manifest folder
        public string negotiation_export {get;set;}
        public bool active {get;set;}        // Windows' ActiveRuntime right now
        public string error {get;set;}       // null = usable; else why not (XR_MANIFEST_MISSING | XR_MANIFEST_BAD | XR_LIBRARY_MISSING)
    }

    // 2.4.0 (167-neural A3): no runtime is imposed. "auto" (default) uses Windows' active runtime, read again at every Play;
    // the player may pick one of the registered runtimes or a manifest file. Before a Play the manifest and its library_path
    // must exist; the external runtime itself is only recorded (path, version, hash), never refused by hash.
    public static class XrRuntimes {
        public const string Auto="auto";
        const string Key=@"SOFTWARE\Khronos\OpenXR\1";

        public static string RegistryActive(){
            try{using(var k=Registry.LocalMachine.OpenSubKey(Key)){if(k!=null){var v=k.GetValue("ActiveRuntime") as string;if(!String.IsNullOrEmpty(v))return v;}}}catch(Exception){}
            return null;
        }
        // AvailableRuntimes: value name = manifest path, DWORD 0 = enabled (the loader's own rule)
        public static List<string> RegistryAvailable(){
            var list=new List<string>();
            try{using(var k=Registry.LocalMachine.OpenSubKey(Key+@"\AvailableRuntimes")){if(k!=null)foreach(string n in k.GetValueNames()){object v=k.GetValue(n);if(v is int&&(int)v==0)list.Add(n);}}}catch(Exception){}
            return list;
        }

        public static XrRuntime Inspect(string manifest,string active){
            var r=new XrRuntime{manifest=manifest,active=active!=null&&String.Equals(Full(manifest),Full(active),StringComparison.OrdinalIgnoreCase)};
            r.name=Name(manifest,null);
            if(String.IsNullOrEmpty(manifest)||!File.Exists(manifest)){r.error="XR_MANIFEST_MISSING";return r;}
            try{
                if(new FileInfo(manifest).Length>1024*1024){r.error="XR_MANIFEST_BAD";return r;}
                var j=new System.Web.Script.Serialization.JavaScriptSerializer().Deserialize<Dictionary<string,object>>(File.ReadAllText(manifest));
                object rt;if(j==null||!j.TryGetValue("runtime",out rt)||!(rt is Dictionary<string,object>)){r.error="XR_MANIFEST_BAD";return r;}
                var d=(Dictionary<string,object>)rt;object lib,nm;
                if(!d.TryGetValue("library_path",out lib)||!(lib is string)||((string)lib).Length==0){r.error="XR_MANIFEST_BAD";return r;}
                string path=(string)lib;
                if(!Path.IsPathRooted(path))path=Path.Combine(Path.GetDirectoryName(Path.GetFullPath(manifest)),path);
                r.library=Path.GetFullPath(path);
                r.negotiation_export="xrNegotiateLoaderRuntimeInterface";
                object functions;
                if(d.TryGetValue("functions",out functions)){
                    var names=functions as Dictionary<string,object>;
                    if(names==null){r.error="XR_MANIFEST_BAD";return r;}
                    object export;
                    if(names.TryGetValue(r.negotiation_export,out export)){
                        r.negotiation_export=export as string;
                        if(String.IsNullOrEmpty(r.negotiation_export)||r.negotiation_export.Length>128||
                           r.negotiation_export.Any(c=>c<=32||c>126)){r.error="XR_MANIFEST_BAD";return r;}
                    }
                }
                if(d.TryGetValue("name",out nm)&&nm is string)r.name=Name(manifest,(string)nm);
                if(!File.Exists(r.library))r.error="XR_LIBRARY_MISSING";
            }catch(Exception){r.error="XR_MANIFEST_BAD";}
            return r;
        }
        public static string NegotiationProblem(XrRuntime runtime){
            if(runtime.error!=null)return runtime.error;
            try{
                // Static admission only. The bundled OpenXR loader still negotiates the real API and extensions at runtime.
                var file=new FileInfo(runtime.library);
                if(!file.Exists)return "XR_LIBRARY_MISSING";
                var exports=Dlss.PeExports(file.FullName);
                if(exports==null)return "XR_LIBRARY_NOT_X64";
                return exports.Contains(runtime.negotiation_export)?null:"XR_NEGOTIATION_EXPORT_MISSING";
            }catch(IOException){return "XR_LIBRARY_UNREADABLE";}
             catch(UnauthorizedAccessException){return "XR_LIBRARY_UNREADABLE";}
        }
        // The registered runtimes: the active one first, then every other enabled one (duplicates removed).
        public static List<XrRuntime> List(string active,IEnumerable<string> available){
            var seen=new HashSet<string>(StringComparer.OrdinalIgnoreCase);var list=new List<XrRuntime>();
            foreach(string m in new[]{active}.Concat(available??new string[0])){
                if(String.IsNullOrEmpty(m)||!seen.Add(Full(m)))continue;
                list.Add(Inspect(m,active));
            }
            return list;
        }
        // The runtime a Play uses: "auto" / null = the active runtime now; otherwise the chosen manifest.
        public static XrRuntime Resolve(string choice,string active){
            string m=String.IsNullOrEmpty(choice)||choice==Auto?active:choice;
            if(String.IsNullOrEmpty(m))return new XrRuntime{error="XR_NO_ACTIVE_RUNTIME"};
            return Inspect(m,active);
        }
        public static string Name(string manifest,string runtimeName){
            string n=((manifest??"")+" "+(runtimeName??"")).ToLowerInvariant();
            if(n.Contains("virtual desktop")||n.Contains("virtualdesktop"))return "Virtual Desktop";
            if(n.Contains("oculus")||n.Contains("meta"))return "Meta Quest Link";
            if(n.Contains("steamvr"))return "SteamVR";
            if(!String.IsNullOrEmpty(runtimeName))return runtimeName;
            return String.IsNullOrEmpty(manifest)?null:Path.GetFileNameWithoutExtension(manifest);
        }
        // The process that shows the runtime is up; null = no indicator for this runtime (neutral chip, no false warning).
        public static string[] StatusProcesses(string name){
            if(name=="Virtual Desktop")return new[]{"VirtualDesktop.Streamer"};
            if(name=="SteamVR")return new[]{"vrserver"};
            if(name=="Meta Quest Link")return new[]{"OVRServer_x64"};
            return null;
        }
        public static bool? Running(string name){
            var names=StatusProcesses(name);if(names==null)return null;
            foreach(string n in names){var p=System.Diagnostics.Process.GetProcessesByName(n);bool any=p.Length>0;foreach(var x in p)x.Dispose();if(any)return true;}
            return false;
        }
        static string Full(string p){try{return Path.GetFullPath(p??"");}catch(Exception){return p??"";}}
    }
}
