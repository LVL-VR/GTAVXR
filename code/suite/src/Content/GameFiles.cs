using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Xml.Linq;

namespace GtavxrSuite.Content {
    // The 3889 originals, read-only from the player's own game folder (no copy of Rockstar data is ever stored).
    // Archive paths are game-relative ('update/update.rpf', 'x64a.rpf', 'update/x64/dlcpacks/mpheist4/dlc.rpf');
    // '|' enters a nested archive ('update/update.rpf|x64/levels/gta5/paths.rpf').
    public sealed class GameFiles {
        public readonly string GameDir;readonly GtaKeys keys;
        readonly Dictionary<string,RpfArchive> open=new Dictionary<string,RpfArchive>(StringComparer.OrdinalIgnoreCase);
        // Packs a mod installs itself (WoV's dlc.rpf): Plus edits files inside them.
        public readonly Dictionary<string,string> ModPacks=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);
        List<string> baseX64;
        public GameFiles(string gameDir){GameDir=gameDir;keys=GtaKeys.Load(gameDir);}

        public RpfArchive Archive(string path){
            RpfArchive a;if(open.TryGetValue(path,out a))return a;
            int bar=path.LastIndexOf('|');
            if(bar>0){
                var parent=Archive(path.Substring(0,bar));
                if(parent==null)return null;
                var e=parent.Find(path.Substring(bar+1));
                a=e==null?null:parent.OpenNested(e);
            }else{
                string file=null;
                var m=System.Text.RegularExpressions.Regex.Match(path,@"^update/x64/dlcpacks/([^/]+)/dlc\.rpf$",System.Text.RegularExpressions.RegexOptions.IgnoreCase);
                string own;
                if(m.Success&&ModPacks.TryGetValue(m.Groups[1].Value,out own))file=own;
                else{string p=Path.Combine(GameDir,path.Replace('/',Path.DirectorySeparatorChar));if(File.Exists(p))file=p;}
                a=file==null?null:RpfArchive.Open(file,keys);
            }
            open[path]=a;
            return a;
        }
        public byte[] Read(string archive,string inner){
            var a=Archive(archive);if(a==null)return null;
            var e=a.Find(inner);return e==null?null:a.Extract(e);
        }
        public bool Exists(string archive,string inner){var a=Archive(archive);return a!=null&&a.Find(inner)!=null;}
        // The base x64*.rpf archives (launch files: never patched by update.rpf entries of the same path).
        public List<string> BaseX64(){
            if(baseX64==null)baseX64=Directory.GetFiles(GameDir,"x64*.rpf").Select(Path.GetFileName).OrderBy(x=>x,StringComparer.OrdinalIgnoreCase).ToList();
            return baseX64;
        }

        // tools/mods/mods_merge.py base_of: where the game really reads a target's 3889 original. where = report label.
        public byte[] BaseOf(string archive,string target,out string where){
            string t=target.ToLowerInvariant();where=null;
            if(String.Equals(archive,"update/update.rpf",StringComparison.OrdinalIgnoreCase)){
                var u=Read("update/update.rpf",t);if(u!=null){where="update.rpf";return u;}
                if(t.StartsWith("dlc_patch/",StringComparison.Ordinal)){
                    var parts=t.Split(new[]{'/'},3);string pa="update/x64/dlcpacks/"+parts[1]+"/dlc.rpf";
                    var f=Read(pa,parts[2]);if(f!=null){where="dlcpacks/"+parts[1]+"/dlc.rpf";return f;}
                    return null;
                }
                if(t.StartsWith("common/",StringComparison.Ordinal)){var f=Read("common.rpf",t.Substring(7));if(f!=null){where="common.rpf";return f;}return null;}
                if(t.StartsWith("x64/",StringComparison.Ordinal))
                    foreach(var x in BaseX64()){var f=Read(x,t.Substring(4));if(f!=null){where=x;return f;}}
                return null;
            }
            var m=System.Text.RegularExpressions.Regex.Match(archive,@"^update/x64/dlcpacks/([^/]+)/dlc\.rpf$",System.Text.RegularExpressions.RegexOptions.IgnoreCase);
            if(m.Success){var f=Read(archive,t);if(f!=null){where=archive;return f;}}
            return null;
        }
        // tools/mods/mods_resources.py find: the 3889 copy of a class C target; launch = it comes from a base x64 archive.
        public byte[] ResourceBase(string archive,string target,out bool launch,out string where){
            launch=false;where=null;string t=target.ToLowerInvariant();
            int bar=archive.IndexOf('|');
            if(bar>0){var b=Read(archive,t);if(b!=null){where=archive;return b;}}
            if(String.Equals(archive,"update/update.rpf",StringComparison.OrdinalIgnoreCase)||bar>0){
                var u=Read("update/update.rpf",t);if(u!=null){where="update.rpf";return u;}
                if(t.StartsWith("x64/",StringComparison.Ordinal))
                    foreach(var x in BaseX64()){var f=Read(x,t.Substring(4));if(f!=null){launch=true;where=x;return f;}}
                return null;
            }
            if(archive.EndsWith(".rpf",StringComparison.OrdinalIgnoreCase)){var f=Read(archive,t);if(f!=null){where=archive;return f;}}
            return null;
        }
        public bool AudioOverride(string archive){
            string name=Path.GetFileName(archive);
            return File.Exists(Path.Combine(GameDir,"update","x64","audio","sfx",name));
        }
        // update.rpf text files: names for PSO hashes (tools/mods build_names reads the same dumps).
        public IEnumerable<KeyValuePair<string,byte[]>> UpdateTexts(){
            var a=Archive("update/update.rpf");if(a==null)yield break;
            foreach(var e in a.Files()){
                string ext=Path.GetExtension(e.Name).ToLowerInvariant();
                if(e.IsResource||(ext!=".xml"&&ext!=".meta"&&ext!=".dat"&&ext!=".txt"&&ext!=".ipl"))continue;
                byte[] d;try{d=a.Extract(e);}catch(Exception){continue;}
                yield return new KeyValuePair<string,byte[]>(e.Path,d);
            }
        }
    }

    // One mod as it was imported: an OIV layout (assembly.xml + content/) and / or loose new DLC packs.
    public sealed class ModOp { public string Archive,Target,Source; }
    public sealed class ModPack { public string Name,File; }
    public sealed class ModSource {
        public string Id,Name,Root;
        public readonly List<ModOp> Ops=new List<ModOp>();
        public readonly List<ModPack> Packs=new List<ModPack>();
        public static ModSource FromOiv(string id,string name,string root){
            var s=new ModSource{Id=id,Name=name,Root=root};
            var asm=XDocument.Load(Path.Combine(root,"assembly.xml"));
            var content=asm.Root.Element("content");
            if(content!=null)WalkOps(s,content,"");
            foreach(var op in s.Ops.ToList()){
                var m=System.Text.RegularExpressions.Regex.Match(op.Target,@"^update/x64/dlcpacks/([^/]+)/dlc\.rpf$",System.Text.RegularExpressions.RegexOptions.IgnoreCase);
                if(op.Archive==""&&m.Success)s.Packs.Add(new ModPack{Name=m.Groups[1].Value,File=op.Source});
            }
            return s;
        }
        // A mod that is only a new DLC pack (V4EVER: one dlc.rpf added to dlclist).
        public static ModSource FromPack(string id,string name,string pack,string file){
            if(!System.Text.RegularExpressions.Regex.IsMatch(pack??"","^[A-Za-z0-9_]+$"))throw new InvalidDataException("bad pack name");
            var s=new ModSource{Id=id,Name=name,Root=Path.GetDirectoryName(file)};
            s.Packs.Add(new ModPack{Name=pack,File=file});
            return s;
        }
        // tools/mods/mods_merge.py operations: (archive, target, source) of every 'add'.
        static void WalkOps(ModSource s,XElement node,string archive){
            foreach(var ch in node.Elements()){
                string tag=ch.Name.LocalName;
                if(tag=="archive"){string p=((string)ch.Attribute("path")??"").Replace('\\','/');WalkOps(s,ch,archive.Length>0?archive+"|"+p:p);}
                else if(tag=="add"){
                    string src=((string)ch.Attribute("source")??"").Replace('\\','/');
                    if(src.Contains("..")||Path.IsPathRooted(src))throw new InvalidDataException("unsafe OIV source: "+src);
                    s.Ops.Add(new ModOp{Archive=archive,Target=(ch.Value??"").Trim().Replace('\\','/'),Source=Path.Combine(s.Root,"content",src.Replace('/',Path.DirectorySeparatorChar))});
                }
                else if(tag!="delete"&&tag!="xml")WalkOps(s,ch,archive);
            }
        }
    }
}
