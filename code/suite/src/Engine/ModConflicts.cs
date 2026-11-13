using System;
using System.IO;
using System.Linq;
using System.Xml.Linq;
using System.Collections.Generic;

namespace GtavxrSuite {
    // 2.4.4 (user 2026-11-04 "vista de conflictos entre mods"): before Play, which of a profile's mods touch the same thing and what
    // the Suite does about it. Only what the profile would really load is compared (PlaySession.ActiveMods). Read only.
    //   blocks  - Play would stop: two mods place the same game / scripts / .asi file, or an .asi mod brings a file of GTAVXR's
    //   merged  - two content mods change the same text / XML game file: the Suite merges both, field by field
    //   wins    - two content mods replace the same whole game file (models, maps, sounds): the later one in the merge order is used
    public sealed class ModConflict {
        public string kind {get;set;}       // game-file | script-file | asi-file | asi-gtavxr | content-text | content-whole | dlc-pack
        public string verdict {get;set;}    // blocks | merged | wins
        public string what {get;set;}       // the file (archive|path inside it for content)
        public List<string> mods {get;set;} // mod names, in merge order for content
        public string winner {get;set;}     // verdict "wins": the mod whose file the game gets
    }
    public static class ModConflicts {
        static readonly string[] TextExt={".meta",".xml",".dat",".ipl",".ymt",".txt"};
        public static List<ModConflict> For(Env env,Profile p,VersionManifest v){
            var list=new List<ModConflict>();
            if(p==null||v==null)return list;
            var lib=new Library(env);var blobs=new Blobs(env);
            List<ModManifest> mods;
            try{mods=new PlaySession(env).ActiveMods(p,v);}catch(InvalidDataException){return list;}
            // game layer files (GameLayer.Plan refuses two owners), plus the version's own files for .asi mods
            var owners=new Dictionary<string,List<string>>(StringComparer.OrdinalIgnoreCase);
            foreach(var m in mods.Where(x=>x.kind!="script"&&x.kind!="content-source"))foreach(var f in m.files??new List<FileRef>())Add(owners,f.path,m.name);
            foreach(var kv in owners.Where(x=>x.Value.Count>1))list.Add(new ModConflict{kind="game-file",verdict="blocks",what=kv.Key,mods=kv.Value});
            var gtavxr=new HashSet<string>((v.files??new List<FileRef>()).Select(f=>f.path),StringComparer.OrdinalIgnoreCase);
            foreach(var k in owners.Keys)gtavxr.Add(k);
            // script mods: the same scripts/ file twice (PlaySession.StageScripts), the same game-folder file twice (StageAsi)
            var scripts=new Dictionary<string,List<string>>(StringComparer.OrdinalIgnoreCase);
            var asi=new Dictionary<string,List<string>>(StringComparer.OrdinalIgnoreCase);
            foreach(var m in mods.Where(x=>x.kind=="script")){
                foreach(var f in m.script_files??new List<FileRef>())Add(scripts,"scripts/"+f.path,m.name);
                foreach(var f in m.asi_files??new List<FileRef>()){
                    Add(asi,f.path,m.name);
                    if(gtavxr.Contains(f.path))list.Add(new ModConflict{kind="asi-gtavxr",verdict="blocks",what=f.path,mods=new List<string>{m.name}});
                }
            }
            foreach(var kv in scripts.Where(x=>x.Value.Count>1))list.Add(new ModConflict{kind="script-file",verdict="blocks",what=kv.Key,mods=kv.Value});
            foreach(var kv in asi.Where(x=>x.Value.Count>1))list.Add(new ModConflict{kind="asi-file",verdict="blocks",what=kv.Key,mods=kv.Value});
            // content mods, in the order the Suite merges them (ContentSources.Plan)
            var srcs=mods.Where(x=>x.kind=="content-source").ToList();
            if(srcs.Count>1){
                List<string> order;
                try{order=new ContentSources(env).Plan(p,srcs).Sources;}catch(Exception){order=srcs.Select(x=>x.id).ToList();}
                var targets=new Dictionary<string,List<string>>(StringComparer.OrdinalIgnoreCase);var packs=new Dictionary<string,List<string>>(StringComparer.OrdinalIgnoreCase);
                foreach(string id in order){
                    var m=srcs.FirstOrDefault(x=>x.id==id)??lib.Mod(id);if(m==null)continue;
                    foreach(var t in Targets(blobs,m))Add(targets,t,m.name);
                    if(m.source!=null&&m.source.pack!=null)Add(packs,m.source.pack,m.name);
                }
                foreach(var kv in targets.Where(x=>x.Value.Count>1)){
                    // "archive|path": the '|' is not valid in a Windows path, so the extension is taken by hand (not Path.GetExtension)
                    string leaf=kv.Key.Substring(Math.Max(kv.Key.LastIndexOf('|'),kv.Key.LastIndexOf('/'))+1);int dot=leaf.LastIndexOf('.');
                    string ext=dot<0?"":leaf.Substring(dot).ToLowerInvariant();bool text=TextExt.Contains(ext);
                    list.Add(new ModConflict{kind=text?"content-text":"content-whole",verdict=text?"merged":"wins",what=kv.Key,mods=kv.Value,winner=text?null:kv.Value.Last()});
                }
                foreach(var kv in packs.Where(x=>x.Value.Count>1))list.Add(new ModConflict{kind="dlc-pack",verdict="wins",what="dlcpacks/"+kv.Key,mods=kv.Value,winner=kv.Value.Last()});
            }
            return list.OrderBy(c=>c.verdict=="blocks"?0:c.verdict=="wins"?1:2).ThenBy(c=>c.what,StringComparer.OrdinalIgnoreCase).ToList();
        }
        static void Add(Dictionary<string,List<string>> d,string key,string name){
            List<string> l;if(!d.TryGetValue(key,out l))d[key]=l=new List<string>();
            if(!l.Contains(name))l.Add(name);
        }
        // A content source's game files as "archive|path" (its assembly.xml, read from the store; ModSource.WalkOps rules).
        static IEnumerable<string> Targets(Blobs blobs,ModManifest m){
            var asm=(m.files??new List<FileRef>()).FirstOrDefault(f=>String.Equals(f.path,"assembly.xml",StringComparison.OrdinalIgnoreCase));
            if(asm==null||!File.Exists(blobs.PathOf(asm.sha256)))yield break;
            XDocument doc;try{doc=XDocument.Load(blobs.PathOf(asm.sha256));}catch(Exception){yield break;}
            var content=doc.Root==null?null:doc.Root.Element("content");if(content==null)yield break;
            foreach(var t in Walk(content,""))yield return t;
        }
        static IEnumerable<string> Walk(XElement node,string archive){
            foreach(var ch in node.Elements()){
                string tag=ch.Name.LocalName;
                if(tag=="archive"){string p=((string)ch.Attribute("path")??"").Replace('\\','/');foreach(var t in Walk(ch,archive.Length>0?archive+"|"+p:p))yield return t;}
                else if(tag=="add")yield return (archive.Length>0?archive+"|":"")+(ch.Value??"").Trim().Replace('\\','/').ToLowerInvariant();
                else if(tag!="delete"&&tag!="xml")foreach(var t in Walk(ch,archive))yield return t;
            }
        }
    }
}
