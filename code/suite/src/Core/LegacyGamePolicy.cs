using System;
using System.IO;
using System.Linq;
using System.Diagnostics;
using System.Collections.Generic;

namespace GtavxrSuite {
    public class GameInspection {
        public string schema {get;set;}public string root {get;set;}public string status {get;set;}public List<Payload> verified {get;set;}
        public bool game_modified {get;set;}public bool game_started {get;set;}public bool launch_admitted {get;set;}
        public List<string> modified {get;set;}   // base files that differ from the Steam 3889 bytes but are still a Legacy 3889 game
    }
    public static class LegacyGamePolicy {
        // The Steam Legacy 3889 bytes. A different store's GTA5.exe (Epic, Rockstar Launcher) or an update.rpf changed by
        // OpenIV is accepted when it is still the Legacy 3889 game; the exact bytes are only noted.
        private static readonly Dictionary<string,string> Known=new Dictionary<string,string>{
            {"GTA5.exe","677E4E355CFBDB13273B1D992407E3C261B3A108DC4DD5C8A0C4C1DA651802E5"},
            {"update/update.rpf","913A335314B4C3C616397782E4175D6A3CF217219750CB9B457B4A781A73DDC0"},
            {"update/update2.rpf","8E2022693D3BE6BF3961BF596045EF2762B34F6AA49D5AFB8083659296CA1393"}
        };
        // Store-game mods in the folder (ASI, ScriptHookV, proxies) never block: RetailPlugins sets aside only what GTAVXR cannot run.
        internal static GameInspection VerifyFiles(string root,IDictionary<string,string> hashes){
            root=SafeFs.Root(root);if(!Directory.Exists(root)||hashes==null||hashes.Count!=3||!new[]{"GTA5.exe","update/update.rpf","update/update2.rpf"}.All(hashes.ContainsKey))throw new InvalidDataException("NOT_LEGACY_3889");
            var proof=new List<Payload>();var modified=new List<string>();
            foreach(var item in hashes.OrderBy(x=>x.Key,StringComparer.Ordinal)){
                string path=SafeFs.Under(root,item.Key);var f=new FileInfo(path);
                if(!f.Exists||f.Length<=0)throw new InvalidDataException("NOT_LEGACY_3889");
                string sha=SafeFs.Hash(path);
                if(!String.Equals(sha,item.Value,StringComparison.OrdinalIgnoreCase)){
                    bool exe=item.Key=="GTA5.exe";
                    if(exe?!IsLegacy3889Exe(path):!IsRpf7(path))throw new InvalidDataException("NOT_LEGACY_3889");
                    modified.Add(item.Key);
                }
                proof.Add(new Payload{path=item.Key,sha256=sha,bytes=f.Length});
            }
            return new GameInspection{schema="gtavxr-suite-game-inspection-v1",root=root,status="BASELINE_FILES_ONLY_VERIFIED_NOT_LAUNCH_ADMISSION",verified=proof,modified=modified};
        }
        // Any store's executable of the Legacy 3889 game reports file version 1.0.3889.x.
        static bool IsLegacy3889Exe(string path){
            var v=FileVersionInfo.GetVersionInfo(path);
            return v.FileMajorPart==1&&v.FileMinorPart==0&&v.FileBuildPart==3889;
        }
        // A game archive, rewritten or not (RPF7 header).
        static bool IsRpf7(string path){
            using(var stream=File.OpenRead(path)){
                var header=new byte[4];
                if(stream.Read(header,0,header.Length)!=header.Length) return false;
                uint magic=BitConverter.ToUInt32(header,0);
                return magic==0x52504637||magic==0x37465052;
            }
        }
        public static GameInspection Inspect(string root){return VerifyFiles(root,Known);}
        public static Dictionary<string,string> Legacy3889(){return new Dictionary<string,string>(Known);}
        public static void Fixture(string fresh){
            string root=SafeFs.Under(fresh,"owned-game");Directory.CreateDirectory(SafeFs.Under(root,"update"));var hashes=new Dictionary<string,string>();
            foreach(string name in Known.Keys){string p=SafeFs.Under(root,name);File.WriteAllText(p,"OWNED FIXTURE; NOT GAME DATA "+name);hashes.Add(name,SafeFs.Hash(p));}
            if(VerifyFiles(root,hashes).launch_admitted)throw new InvalidDataException("Inspection cannot authorize launch");
            foreach(string name in new[]{"foreign.ASI","DXGI.DLL","ScriptHookV.dll"}){string p=SafeFs.Under(root,name);File.WriteAllText(p,"OWNED PLUGIN FIXTURE");VerifyFiles(root,hashes);File.Delete(p);}
            // An archive rewritten by a mod tool (still RPF7) is accepted and noted; a file that is not an archive is not.
            string file=SafeFs.Under(root,"update/update.rpf");string original=File.ReadAllText(file);
            File.WriteAllText(file,"RPF7 rewritten by a mod tool");var seen=VerifyFiles(root,hashes);
            if(seen.modified.Count!=1||seen.modified[0]!="update/update.rpf")throw new InvalidDataException("Modified 3889 archive not accepted");
            File.WriteAllBytes(file,BitConverter.GetBytes(0x52504637u));
            if(VerifyFiles(root,hashes).modified.Count!=1)throw new InvalidDataException("Retail RPF7 byte order not accepted");
            File.WriteAllText(file,original+"changed");bool drift=false;try{VerifyFiles(root,hashes);}catch{drift=true;}if(!drift)throw new InvalidDataException("A file that is not a game archive was accepted");
            File.WriteAllText(file,original);
            // An executable that is not GTA V 3889 (here: not a GTA executable at all) is refused.
            string exeFile=SafeFs.Under(root,"GTA5.exe");File.AppendAllText(exeFile,"x");bool other=false;try{VerifyFiles(root,hashes);}catch{other=true;}if(!other)throw new InvalidDataException("Another game executable was accepted");
        }
    }
}
