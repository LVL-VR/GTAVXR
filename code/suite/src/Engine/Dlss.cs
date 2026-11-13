using System;
using System.IO;
using System.Linq;
using System.Text;
using System.Diagnostics;
using System.Collections.Generic;

namespace GtavxrSuite {
    // One DLSS model file as found on this PC (never packaged by GTAVXR: the player's own copy).
    public sealed class DlssModel {
        public string path {get;set;}
        public string version {get;set;}
        public string product {get;set;}
        public long bytes {get;set;}
        public string sha256 {get;set;}      // recorded only (rule 2): never compared
        public string error {get;set;}       // null = usable; DLSS_NOT_PE | DLSS_NOT_NVIDIA | DLSS_NO_NGX_EXPORTS
    }
    public sealed class DlssFolder {
        public string dir {get;set;}
        public string source {get;set;}      // chosen | suite | found
        public DlssModel sr {get;set;}       // nvngx_dlss.dll (Super Resolution): required
        public DlssModel nr {get;set;}       // nvngx_dlssnr.dll (neural): optional; without it NR stays off
    }

    // 2.4.0 (167-neural B3): the DLSS folder of this PC. Order: the folder the player chose, the Suite's own dlss folder, then a
    // bounded search of the player's folders and game libraries. A model is valid by CONTENT: a 64-bit PE whose version
    // resource names NVIDIA / DLSS and which exports the four NGX entry points GTAVXR's forwarder calls. Never by hash.
    public static class Dlss {
        public const string SrModel="nvngx_dlss.dll",NrModel="nvngx_dlssnr.dll";
        public static readonly string[] Exports={"NVSDK_NGX_D3D12_Init_Ext","NVSDK_NGX_D3D12_CreateFeature","NVSDK_NGX_D3D12_EvaluateFeature","NVSDK_NGX_D3D12_ReleaseFeature"};

        public static DlssModel Inspect(string file){
            var m=new DlssModel{path=file};
            try{
                var fi=new FileInfo(file);m.bytes=fi.Length;
                if(fi.Length<4096||fi.Length>512L*1024*1024){m.error="DLSS_NOT_PE";return m;}
                var names=PeExports(file);
                if(names==null){m.error="DLSS_NOT_PE";return m;}
                var vi=FileVersionInfo.GetVersionInfo(file);
                m.version=vi.FileVersion==null?null:vi.FileVersion.Replace(',','.').Replace(" ","");m.product=vi.ProductName;
                string id=((vi.CompanyName??"")+" "+(vi.ProductName??"")+" "+(vi.FileDescription??"")).ToLowerInvariant();
                if(!id.Contains("nvidia")&&!id.Contains("dlss")){m.error="DLSS_NOT_NVIDIA";return m;}
                if(Exports.Any(x=>!names.Contains(x))){m.error="DLSS_NO_NGX_EXPORTS";return m;}
                m.sha256=SafeFs.Hash(file);
            }catch(Exception){m.error="DLSS_NOT_PE";}
            return m;
        }
        // A folder is a DLSS folder when its Super Resolution model is valid; the neural model is taken when it is valid too.
        public static DlssFolder InspectFolder(string dir,string source){
            if(String.IsNullOrEmpty(dir)||!Directory.Exists(dir))return null;
            string sr=Path.Combine(dir,SrModel);if(!File.Exists(sr))return null;
            var f=new DlssFolder{dir=Path.GetFullPath(dir),source=source,sr=Inspect(sr)};
            string nr=Path.Combine(dir,NrModel);if(File.Exists(nr))f.nr=Inspect(nr);
            return f;
        }
        public static bool Usable(DlssFolder f){return f!=null&&f.sr!=null&&f.sr.error==null;}
        public static bool NeuralUsable(DlssFolder f){return Usable(f)&&f.nr!=null&&f.nr.error==null;}

        // chosen (the player's folder) first, then <Suite data>/dlss, then the search roots (each searched a few levels deep; there a
        // folder with the neural model is preferred).
        public static DlssFolder Find(string chosen,string suiteDir,IEnumerable<string> searchRoots){
            DlssFolder best=null;
            foreach(var c in new[]{Tuple.Create(chosen,"chosen"),Tuple.Create(suiteDir,"suite")}){
                var f=InspectFolder(c.Item1,c.Item2);
                if(Usable(f))return f;                              // the first usable folder in this order wins (the player's choice first)
                if(c.Item2=="chosen"&&f!=null)best=f;               // a chosen folder that stopped working is reported if nothing else works
            }
            foreach(string root in searchRoots??new string[0]){
                foreach(string d in Folders(root,3)){
                    var f=InspectFolder(d,"found");
                    if(NeuralUsable(f))return f;
                    if(Usable(f)&&!Usable(best))best=f;
                }
                if(Usable(best))return best;
            }
            return best;
        }
        // Where a player keeps DLSS files: their Desktop / Downloads / Documents and the game libraries (Steam).
        public static List<string> DefaultSearchRoots(){
            var list=new List<string>();
            foreach(var sf in new[]{Environment.SpecialFolder.Desktop,Environment.SpecialFolder.MyDocuments}){string p=Environment.GetFolderPath(sf);if(!String.IsNullOrEmpty(p))list.Add(p);}
            string home=Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);if(!String.IsNullOrEmpty(home))list.Add(Path.Combine(home,"Downloads"));
            foreach(string lib in Detect.SteamLibraries())list.Add(Path.Combine(lib,"steamapps","common"));
            return list.Where(Directory.Exists).Distinct(StringComparer.OrdinalIgnoreCase).ToList();
        }
        // breadth-first, at most `depth` levels and 4000 folders per root; no reparse points (no junction loops)
        static IEnumerable<string> Folders(string root,int depth){
            if(String.IsNullOrEmpty(root)||!Directory.Exists(root))yield break;
            var q=new Queue<Tuple<string,int>>();q.Enqueue(Tuple.Create(root,0));int seen=0;
            while(q.Count>0&&seen<4000){
                var t=q.Dequeue();seen++;yield return t.Item1;
                if(t.Item2>=depth)continue;
                string[] subs;try{subs=Directory.GetDirectories(t.Item1);}catch(Exception){continue;}
                foreach(string s in subs){try{if((new DirectoryInfo(s).Attributes&FileAttributes.ReparsePoint)!=0)continue;}catch(Exception){continue;}q.Enqueue(Tuple.Create(s,t.Item2+1));}
            }
        }

        // Shared with OpenXR. Only the PE headers and export table are inspected.
        public static HashSet<string> PeExports(string file){
            try{
                using(var image=new PeInspection(file))
                    return image.Machine==0x8664?image.ExportNames():null;
            }catch(InvalidDataException){return null;}
             catch(OverflowException){return null;}
        }
    }
}
