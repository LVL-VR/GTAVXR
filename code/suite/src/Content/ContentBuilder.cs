using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Xml.Linq;

namespace GtavxrSuite.Content {
    public sealed class BuildOptions {
        public HashSet<string> Off=new HashSet<string>();   // traffic | peds | police | scenarios | driving_ai
        public bool ScenariosFull;                           // "WoV completo": WoV's regions even where they drop 3889 Story points
        public string DensitySource;                         // source whose density / streaming values can be dropped (WoV Plus)
        public bool SourceDensity=true,SourceStreaming;      // its density on, its streaming buffers off (VR memory risk)
        public readonly List<SourceRule> Rules=new List<SourceRule>();          // parts of a source left out (Personalizar)
        public readonly Dictionary<string,string> Gameconfig=new Dictionary<string,string>();  // tag -> value set after the merge
        public readonly HashSet<string> DropPacks=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        public bool Detail;                                  // report every added / changed key (catalog census; not at Play)
        public HashSet<string> PsoXmlRoots=new HashSet<string>(StringComparer.Ordinal);  // VersionManifest.pso_xml of the version played
    }
    // What a switch leaves out of one source before the merge. The merge keeps every 3889 entry / field a mod does not
    // change, so taking an entry or field out of the mod's copy means "the game's own" for exactly that part.
    //   drop    the source's file(s) matching Target are not used
    //   items   list entries whose key matches Key are taken out (List: only lists whose path contains it)
    //   fields  the Fields of the entries whose key matches Key (all entries when Key is null) are taken out
    //   lines   text files (.dat / .ipl): lines matching Key are taken out; Block = also the indented lines under them
    //   pairs   relationships.dat: the relations "GROUP Rel TARGET" matching Key are taken out
    //   popcycle popcycle.dat: List = peds | cars | cops columns of every schedule row back to the 3889 row
    //   pack    a file inside a DLC pack the mod installs (Target "<pack>|<inner path>"): after the merge, the entries
    //           whose key matches Key (Invert: that do NOT match) are removed from what the game reads
    // Key matches the entry key's value (case-insensitive regex), or its hash written h:xxxxxxxx.
    public sealed class SourceRule {
        public string Source,Kind,Target,List,Key,Only;public string[] Fields;public bool Block,Invert;   // Only: "added" | "changed" (vs 3889)
        public bool Matches(string target){return Glob(Target).IsMatch(target);}
        public static Regex Glob(string g){return new Regex("^"+Regex.Escape((g??"*").ToLowerInvariant()).Replace("\\*",".*").Replace("\\?",".")+"$");}
    }
    public sealed class ContentFile { public string Mount,Relative,Source,What,Category;public long Bytes;public uint RscVer; }
    public sealed class BuildResult {
        public string Id,Folder;                             // Folder holds manifest.txt and every file this build wrote
        public readonly List<ContentFile> Files=new List<ContentFile>();
        public readonly OrderedMap<string,string> Mounts=new OrderedMap<string,string>(StringComparer.OrdinalIgnoreCase);
        public readonly List<KeyValuePair<string,string>> Streams=new List<KeyValuePair<string,string>>();
        public string Manifest;public Dictionary<string,object> Report;public List<string> ControlFailures=new List<string>();
    }

    // Builds a content profile from the player's own 3889 game files + imported mods: the merge (tools/mods/mods_merge.py),
    // the class C verdicts (mods_resources.py) and the profile folder the EXE serves read-only (mods_profile.py,
    // -gtavxrContent=<id>). Every merged file passes its control or the build stops.
    public sealed class ContentBuilder {
        public const int MaxMounts=64,MaxFiles=1024,MaxStreams=256;  // content/profile_manifest.h
        public static readonly Dictionary<string,string[]> Categories=new Dictionary<string,string[]>{
            {"traffic",new[]{"vehiclemodelsets.meta","vehicles.meta","handling.meta","vehiclelayouts.meta","vehiclelayouts_avisa.meta","nodes658.ynd","hei_hw1_12_strm_0.ymap"}},
            {"peds",new[]{"ambientpedmodelsets.meta","peds.meta","peds.ymt","pedcomponentsets.meta","pedhealth.meta","pedperception.meta","pedpersonality.ymt","relationships.dat","loadouts.meta","clip_sets.ymt","conditionalanims.meta","combattasks.ymt","*.awc"}},
            {"police",new[]{"dispatch.meta","dispatchtuning.ymt"}},
            {"scenarios",new[]{"scenarios.meta","propsets.meta","ambient_sd.ipl","agitatedtriggers.meta","movementevents.ymt","sp_manifest.ymt","*scenario-region*"}},
            {"driving_ai",new[]{"vehicleaitasks.ymt","taskdata.meta"}},
        };
        static readonly string[] Density={"ScenarioPedsMultiplier_Base","ScenarioPedsMultiplier","AmbientPedsMultiplier_Base","AmbientPedsMultiplier",
            "VehicleAmbientDensityMultiplier_Base","VehicleAmbientDensityMultiplier","VehicleParkedDensityMultiplier_Base","VehicleParkedDensityMultiplier",
            "VehicleLowPrioParkedDensityMultiplier_Base","VehicleLowPrioParkedDensityMultiplier"};
        static readonly string[] Streaming={"PhysicalStreamingBuffer","VirtualStreamingBuffer"};

        readonly GameFiles game;List<ModSource> sources;readonly BuildOptions opt;readonly Action<string,int,int> progress;
        PsoNames names;
        ContentBuilder(GameFiles g,List<ModSource> s,BuildOptions o,Action<string,int,int> p){game=g;sources=s;opt=o;progress=p??((a,b,c)=>{});}

        public static string Category(string name,bool region){
            string n=name.ToLowerInvariant();
            foreach(var c in Categories)
                if(c.Value.Contains(n)||(c.Value.Contains("*.awc")&&n.EndsWith(".awc",StringComparison.Ordinal))||(region&&c.Value.Contains("*scenario-region*")))return c.Key;
            return "core";
        }
        static string Flat(string p){return p.Replace("/","__").Replace("|","__");}

        sealed class Target{public string Archive,TargetPath;public List<KeyValuePair<string,string>> Mods=new List<KeyValuePair<string,string>>();}

        public static BuildResult Build(string gameDir,List<ModSource> sources,BuildOptions opt,string id,string outDir,Action<string,int,int> progress){
            if(!Regex.IsMatch(id??"","^[A-Za-z0-9_-]{1,40}$"))throw new InvalidDataException("content profile id must be [A-Za-z0-9_-]");
            if(Directory.Exists(outDir))throw new IOException("content build folder must be fresh");
            var g=new GameFiles(gameDir);opt=opt??new BuildOptions();
            foreach(var s in sources)foreach(var p in s.Packs)if(!opt.DropPacks.Contains(p.Name))g.ModPacks[p.Name]=p.File;
            var b=new ContentBuilder(g,sources,opt,progress);
            var rules=new List<Dictionary<string,object>>();
            b.sources=b.Prepare(Path.Combine(outDir,"prepared"),rules);
            var r=b.Run(id,outDir,rules);
            return r;
        }

        // ---------------------------------------------------------------- Personalizar: the parts of each source left out
        List<ModSource> Prepare(string dir,List<Dictionary<string,object>> rows){
            if(opt.Rules.Count==0&&opt.DropPacks.Count==0)return sources;
            var list=new List<ModSource>();var hits=new Dictionary<SourceRule,int>();foreach(var r in opt.Rules)if(r.Kind!="pack")hits[r]=0;
            foreach(var s in sources){
                var rules=opt.Rules.Where(r=>r.Source==s.Id&&r.Kind!="pack").ToList();
                var c=new ModSource{Id=s.Id,Name=s.Name,Root=s.Root};
                foreach(var p in s.Packs)if(opt.DropPacks.Contains(p.Name))rows.Add(Row("source",s.Id,"pack",p.Name,"action","pack-off"));else c.Packs.Add(p);
                foreach(var op in s.Ops){
                    string t=op.Target.ToLowerInvariant(),full=op.Archive.ToLowerInvariant()+"|"+t;
                    var pk=Regex.Match(op.Archive+"|"+op.Target,@"^(?:\|)?update/x64/dlcpacks/([^/|]+)/dlc\.rpf",RegexOptions.IgnoreCase);
                    if(pk.Success&&opt.DropPacks.Contains(pk.Groups[1].Value))continue;   // into (or the install of) a pack that is off
                    var mine=rules.Where(r=>r.Target!=null&&r.Target.Contains('|')?r.Matches(full):r.Matches(t)).ToList();
                    if(mine.Count==0){c.Ops.Add(op);continue;}
                    if(mine.Any(r=>r.Kind=="drop")){foreach(var r in mine.Where(x=>x.Kind=="drop"))hits[r]++;rows.Add(Row("source",s.Id,"target",full,"action","file-off"));continue;}
                    byte[] data=File.ReadAllBytes(op.Source);int removed=0;byte[] outb;
                    string ext=Path.GetExtension(t).ToLowerInvariant();
                    if(ext==".dat"||ext==".ipl"||ext==".txt"){
                        var lines=TextMerge.Lines(data);
                        foreach(var r in mine.Where(x=>x.Kind=="lines")){int k=Lines(lines,r);hits[r]+=k;removed+=k;}
                        foreach(var r in mine.Where(x=>x.Kind=="pairs")){int k=Pairs(lines,r);hits[r]+=k;removed+=k;}
                        foreach(var r in mine.Where(x=>x.Kind=="popcycle")){
                            string where;var b=game.BaseOf(op.Archive,t,out where);
                            if(b==null)throw new InvalidDataException("popcycle rule: no 3889 "+t);
                            int k=Popcycle(lines,TextMerge.Lines(b),r.List);hits[r]+=k;removed+=k;
                        }
                        outb=TextMerge.Bytes(lines);
                    }else{
                        if(Pso.IsPso(data))Names();
                        var root=XmlMerge.LoadAny(data,names);
                        HashSet<string> baseKeys=null;
                        if(mine.Any(x=>x.Only!=null)){string where;var bb=game.BaseOf(op.Archive,t,out where);baseKeys=BaseKeys(bb==null?null:XmlMerge.LoadAny(bb,Pso.IsPso(bb)?NamesNow():names));}
                        foreach(var r in mine){int k=r.Kind=="items"?Items(root,r,baseKeys):r.Kind=="fields"?Fields(root,r,baseKeys):0;hits[r]+=k;removed+=k;}
                        outb=root.ToXml();
                    }
                    string dest=Path.Combine(dir,s.Id,Flat(op.Archive),op.Target.Replace('/',Path.DirectorySeparatorChar));
                    Directory.CreateDirectory(Path.GetDirectoryName(dest));File.WriteAllBytes(dest,outb);
                    c.Ops.Add(new ModOp{Archive=op.Archive,Target=op.Target,Source=dest});
                    rows.Add(Row("source",s.Id,"target",full,"action","parts-off","removed",removed));
                }
                list.Add(c);
            }
            foreach(var kv in hits)rows.Add(Row("rule",kv.Key.Source+" "+kv.Key.Kind+" "+kv.Key.Target+(kv.Key.Key!=null?" "+kv.Key.Key:""),"hits",kv.Value));
            return list;
        }
        static Dictionary<string,object> Row(params object[] kv){var d=new Dictionary<string,object>();for(int i=0;i+1<kv.Length;i+=2)d[(string)kv[i]]=kv[i+1];return d;}
        static bool KeyMatch(Node item,string key){
            if(key==null)return true;
            string k=XmlMerge.KeyOf(item,MergeMode.Xml);if(k==null)return false;
            string v=k.Substring(k.IndexOf('=')+1);
            return Regex.IsMatch(v,key,RegexOptions.IgnoreCase)||Regex.IsMatch(XmlMerge.KeyHash(v),key,RegexOptions.IgnoreCase);
        }
        static int Items(Node root,SourceRule r,HashSet<string> baseKeys=null){
            int n=0;
            foreach(var kv in XmlMerge.Lists(root)){
                if(r.List!=null&&kv.Key.IndexOf(r.List,StringComparison.OrdinalIgnoreCase)<0)continue;
                n+=kv.Value.Children.RemoveAll(i=>KeyMatch(i,r.Key)!=r.Invert&&OnlyMatch(i,kv.Key,r,baseKeys));
            }
            return n;
        }
        // "added": the 3889 file has no entry with that key in that list; "changed": it has one.
        static bool OnlyMatch(Node item,string path,SourceRule r,HashSet<string> baseKeys){
            if(r.Only==null)return true;
            string k=XmlMerge.KeyOf(item,MergeMode.PsoMode);bool inBase=baseKeys!=null&&k!=null&&baseKeys.Contains(path+"\u0001"+k);
            return r.Only=="added"?!inBase:inBase;
        }
        static HashSet<string> BaseKeys(Node b){
            var s=new HashSet<string>();if(b==null)return s;
            foreach(var kv in XmlMerge.Lists(b))foreach(var i in kv.Value.Children){string k=XmlMerge.KeyOf(i,MergeMode.PsoMode);if(k!=null)s.Add(kv.Key+"\u0001"+k);}
            return s;
        }
        PsoNames NamesNow(){Names();return names;}
        // relationships.dat written back in the game's grammar (the groups list, then each group's relations).
        static int Pairs(List<string> lines,SourceRule r){
            List<string> groups;OrderedMap<string,string> rels;TextMerge.Relations(lines,out groups,out rels);
            var re=new Regex(r.Key,RegexOptions.IgnoreCase);int n=0;var keep=new OrderedMap<string,string>();
            foreach(var kv in rels.Items){var p=kv.Key.Split('\u0001');if(re.IsMatch(p[0]+" "+kv.Value+" "+p[1]))n++;else keep[kv.Key]=kv.Value;}
            if(n==0)return 0;
            int first=lines.FindIndex(l=>!l.StartsWith("#",StringComparison.Ordinal));
            var head=lines.Take(first<0?lines.Count:first).ToList();
            lines.Clear();lines.AddRange(head);lines.AddRange(groups);
            foreach(var g in groups){
                var mine=keep.Items.Where(kv=>kv.Key.Split('\u0001')[0]==g).ToList();if(mine.Count==0)continue;
                lines.Add("");lines.Add(g);
                foreach(var v in new[]{"Hate","Dislike","Like","Respect"}){var ts=mine.Where(x=>x.Value==v).Select(x=>x.Key.Split('\u0001')[1]).ToList();if(ts.Count>0)lines.Add("    "+v+" "+String.Join(" ",ts));}
            }
            return n;
        }
        // popcycle.dat rows: 10 numbers (#Peds #Scenario #Cars #prkdcrs #lowprkdcrs PercCopCars PercCopPed MaxScenPeds
        // MaxScenVehs MaxPreAssignedParked), then "peds" groups, then "cars" groups (Peds/PopCycle.cpp). One part of each row
        // of a schedule goes back to the same row of the 3889 schedule; a schedule 3889 lacks or with other rows stays.
        static readonly Dictionary<string,int[]> PopCols=new Dictionary<string,int[]>{{"peds",new[]{0,1,7}},{"cars",new[]{2,3,4,8,9}},{"cops",new[]{5,6}}};
        static int Popcycle(List<string> lines,List<string> baseLines,string part){
            int[] cols;if(part==null||!PopCols.TryGetValue(part,out cols))throw new InvalidDataException("popcycle part "+part);
            List<string> h;OrderedMap<string,List<string>> mine,orig;TextMerge.Schedules(lines,out h,out mine);TextMerge.Schedules(baseLines,out h,out orig);
            int n=0;var replaced=new Dictionary<List<string>,List<string>>();
            foreach(var kv in mine.Items){
                List<string> ob;if(!orig.TryGetValue(kv.Key,out ob))continue;
                var mr=kv.Value.Select((l,i)=>i).Where(i=>IsRow(kv.Value[i])).ToList();var orr=ob.Where(IsRow).ToList();
                if(mr.Count!=orr.Count)continue;
                var nb=new List<string>(kv.Value);
                for(int i=0;i<mr.Count;i++){string a=PopRow(kv.Value[mr[i]],orr[i],cols,part);if(a!=kv.Value[mr[i]]){nb[mr[i]]=a;n++;}}
                replaced[kv.Value]=nb;
            }
            if(n==0)return 0;
            // rebuild the file: same lines, every schedule block swapped for its edited copy
            var outl=new List<string>(h);
            foreach(var kv in mine.Items){List<string> nb;outl.AddRange(replaced.TryGetValue(kv.Value,out nb)?nb:kv.Value);outl.Add("");}
            lines.Clear();lines.AddRange(outl);
            return n;
        }
        static bool IsRow(string l){string s=l.Trim();return s.Length>2&&!s.StartsWith("/",StringComparison.Ordinal)&&Char.IsDigit(s[0]);}
        static string PopRow(string row,string orow,int[] cols,string part){
            var t=Regex.Split(row.Trim(),"\\s+").ToList();var o=Regex.Split(orow.Trim(),"\\s+").ToList();
            if(t.Count<10||o.Count<10)return row;
            foreach(int c in cols)t[c]=o[c];
            if(part!="cops"){
                int a=t.IndexOf(part),b=o.IndexOf(part);
                if(a<0||b<0)return row;
                int ae=part=="peds"?t.IndexOf("cars"):t.Count,be=part=="peds"?o.IndexOf("cars"):o.Count;
                if(ae<0)ae=t.Count;if(be<0)be=o.Count;
                t.RemoveRange(a,ae-a);t.InsertRange(a,o.Skip(b).Take(be-b));
            }
            return "      "+String.Join("  ",t);
        }
        // pack rules: after the merge, entries removed from a file inside a DLC pack the mod installs (served from the pack's
        // device in the profile, so the game reads the edited copy instead of the pack's own).
        void PackRules(BuildResult r,Dictionary<string,ContentFile> byKey,Dictionary<string,string> devices,string profile,List<Dictionary<string,object>> rows){
            foreach(var g in opt.Rules.Where(x=>x.Kind=="pack").GroupBy(x=>x.Target.ToLowerInvariant())){
                string pack=g.Key.Substring(0,g.Key.IndexOf('|')),inner=g.Key.Substring(g.Key.IndexOf('|')+1);
                if(opt.DropPacks.Contains(pack)||!sources.Any(s=>s.Packs.Any(p=>String.Equals(p.Name,pack,StringComparison.OrdinalIgnoreCase)))){
                    foreach(var x in g)rows.Add(Row("rule",x.Source+" pack "+x.Target+" "+x.Key,"hits",0,"reason","pack not in this profile"));continue;
                }
                string dev;if(!devices.TryGetValue(pack,out dev))devices[pack]=dev=PackDevice(pack);
                ContentFile f;byte[] data;
                if(byKey.TryGetValue(dev.ToLowerInvariant()+"\u0001"+inner,out f))data=File.ReadAllBytes(f.Source);
                else{data=game.Read("update/x64/dlcpacks/"+pack+"/dlc.rpf",inner);if(data==null)throw new InvalidDataException("pack rule: "+pack+" has no "+inner);}
                var root=XmlMerge.LoadAny(data,names);
                foreach(var x in g){int k=Items(root,x);rows.Add(Row("rule",x.Source+" pack "+x.Target+" "+x.Key+(x.Invert?" (keep)":""),"hits",k));}
                string own=Path.Combine(profile,"pack-"+pack+"-"+Flat(inner));File.WriteAllBytes(own,root.ToXml());
                if(f!=null){f.Source=own;f.Bytes=new FileInfo(own).Length;f.What="merged (pack rule)";}
                else AddFile(r,byKey,dev,inner,own,"pack-rule","pack");
            }
        }
        static int Fields(Node root,SourceRule r,HashSet<string> baseKeys=null){
            int n=0;var f=new HashSet<string>(r.Fields??new string[0]);
            foreach(var kv in XmlMerge.Lists(root)){
                if(r.List!=null&&kv.Key.IndexOf(r.List,StringComparison.OrdinalIgnoreCase)<0)continue;
                foreach(var i in kv.Value.Children)if(KeyMatch(i,r.Key)&&OnlyMatch(i,kv.Key,r,baseKeys))n+=i.Children.RemoveAll(c=>f.Contains(c.Tag));
            }
            return n;
        }
        static int Lines(List<string> lines,SourceRule r){
            var re=new Regex(r.Key,RegexOptions.IgnoreCase);int n=0;
            for(int i=0;i<lines.Count;){
                if(!re.IsMatch(lines[i].TrimEnd('\r'))){i++;continue;}
                lines.RemoveAt(i);n++;
                if(r.Block)while(i<lines.Count&&lines[i].Length>0&&(lines[i][0]==' '||lines[i][0]=='\t')){lines.RemoveAt(i);n++;}
            }
            return n;
        }

        Dictionary<string,Target> Targets(IEnumerable<ModSource> use){
            var t=new Dictionary<string,Target>();
            foreach(var s in use)foreach(var op in s.Ops){
                string key=op.Archive+"\u0001"+op.Target.ToLowerInvariant();Target x;
                if(!t.TryGetValue(key,out x))t[key]=x=new Target{Archive=op.Archive,TargetPath=op.Target.ToLowerInvariant()};
                x.Mods.Add(new KeyValuePair<string,string>(s.Id,op.Source));
            }
            return t;
        }
        void Names(){
            if(names!=null)return;
            progress("names",0,1);
            names=new PsoNames();names.AddMetaNames();
            foreach(var s in sources){
                string content=Path.Combine(s.Root,"content");
                if(Directory.Exists(content))foreach(var f in Directory.GetFiles(content,"*",SearchOption.AllDirectories).OrderBy(x=>x,StringComparer.Ordinal))names.AddFile(f);
                foreach(var p in s.Packs){var a=game.Archive("update/x64/dlcpacks/"+p.Name+"/dlc.rpf");if(a!=null)foreach(var e in a.Files())if(TextLike(e.Name))try{AddBytes(a.Extract(e),e.Name);}catch(Exception){}}
            }
            foreach(var kv in game.UpdateTexts())AddBytes(kv.Value,kv.Key);
            progress("names",1,1);
        }
        static bool TextLike(string n){string e=Path.GetExtension(n).ToLowerInvariant();return e==".xml"||e==".meta"||e==".dat"||e==".txt"||e==".ipl"||e==".ymt";}
        void AddBytes(byte[] d,string name){
            int i=0;if(d.Length>=3&&d[0]==0xEF&&d[1]==0xBB&&d[2]==0xBF)i=3;
            while(i<d.Length&&i<64&&(d[i]==' '||d[i]=='\r'||d[i]=='\n'||d[i]=='\t'))i++;
            if(i<d.Length&&d[i]=='<')names.AddXml(d);
            else{string e=Path.GetExtension(name).ToLowerInvariant();if(e==".dat"||e==".ipl"||e==".txt"||e==".meta"||e==".xml")names.AddText(d);}
        }

        // ---------------------------------------------------------------- merge (mods_merge.py main)
        sealed class Merged{public string Kind,Archive,TargetPath,Out;}
        List<Merged> MergeAll(IEnumerable<ModSource> use,string filesDir,Dictionary<string,object> report,List<Dictionary<string,object>> notA,List<string> failures){
            var merged=new List<Merged>();var rowsA=new List<Dictionary<string,object>>();var rowsT=new List<Dictionary<string,object>>();var rowsP=new List<Dictionary<string,object>>();
            var targets=Targets(use).Values.OrderBy(x=>x.Archive,StringComparer.Ordinal).ThenBy(x=>x.TargetPath,StringComparer.Ordinal).ToList();
            int n=0;
            foreach(var t in targets){
                progress("merge",n++,targets.Count);
                string ext=t.TargetPath.Contains('.')?t.TargetPath.Substring(t.TargetPath.LastIndexOf('.')+1):"";
                string where=null;byte[] b=null;
                if(ext=="meta"||ext=="xml"||ext=="dat"||ext=="ipl"||ext=="ymt")b=game.BaseOf(t.Archive,t.TargetPath,out where);
                bool text=ext=="dat"||ext=="ipl";string reason=null;
                if(ext=="ymt"&&b!=null&&!Pso.IsPso(b))reason="class C resource (.ymt, 3889 original is not PSO)";
                else if(ext=="ymt"&&b==null)reason="no 3889 original found (.ymt)";
                else if(ext!="meta"&&ext!="xml"&&ext!="dat"&&ext!="ipl"&&ext!="ymt")reason="class B/C or text format (."+ext+")";
                else if(b==null)reason="no 3889 original found (new file or pack not dumped)";
                else if(!text&&ext!="ymt"&&!IsXml(b))reason="3889 original is binary";
                var modsList=t.Mods.Select(m=>m.Value).ToList();
                if(reason!=null){notA.Add(new Dictionary<string,object>{{"archive",t.Archive},{"target",t.TargetPath},{"reason",reason},{"mods",modsList}});continue;}
                string rel=Path.Combine(Flat(t.Archive),t.TargetPath.Replace('/',Path.DirectorySeparatorChar));
                string dest=Path.Combine(filesDir,rel);Directory.CreateDirectory(Path.GetDirectoryName(dest));
                var mods=t.Mods.Select(m=>new KeyValuePair<string,byte[]>(m.Value,File.ReadAllBytes(m.Value))).ToList();
                if(ext=="ymt"){
                    Names();
                    var rows=new List<Dictionary<string,object>>();var fixedc=new Dictionary<string,int>{{"same_as_3889",0},{"mod_value",0}};int rebuilt;
                    var res=XmlMerge.MergePso(b,mods,names,rows,fixedc,out rebuilt);
                    string refused=PsoXmlRefusal(res.Tag,opt);
                    if(refused!=null){notA.Add(new Dictionary<string,object>{{"archive",t.Archive},{"target",t.TargetPath},{"reason",refused},{"mods",modsList},{"pso_only",true}});continue;}
                    File.WriteAllBytes(dest,res.ToXml());
                    var problems=XmlMerge.VerifyPso(XmlMerge.LoadAny(b,names),mods.Select(m=>XmlMerge.LoadAny(m.Value,names)).ToList(),Node.Load(dest),rows);
                    Fail(failures,t.TargetPath,problems);
                    var lists=rows.Where(r=>r.ContainsKey("added")).ToList();
                    rowsP.Add(new Dictionary<string,object>{{"archive",t.Archive},{"target",t.TargetPath},{"base",where},{"out",rel},{"mods",modsList},
                        {"added",lists.Sum(r=>((List<string>)r["added"]).Count)},{"changed",lists.Sum(r=>((List<string>)r["changed"]).Count)},
                        {"kept_3889_only",lists.Sum(r=>((List<string>)r["kept_3889_only"]).Count)},{"placeholders",fixedc},{"rebuilt_3889_hashes",rebuilt}});
                    if(opt.Detail)rowsP[rowsP.Count-1]["detail"]=lists.Select(r=>new Dictionary<string,object>{{"path",r["path"]},{"mod",r["mod"]},{"added",r["added"]},{"changed",r["changed"]},{"fields",r["fields"]}}).ToList();
                    merged.Add(new Merged{Kind="pso_merged",Archive=t.Archive,TargetPath=t.TargetPath,Out=rel});continue;
                }
                if(text){
                    byte[] outb;var rep=TextMerge.Merge(Path.GetFileName(t.TargetPath),b,mods.Select(m=>m.Value).ToList(),out outb);
                    if(rep==null){notA.Add(new Dictionary<string,object>{{"archive",t.Archive},{"target",t.TargetPath},{"reason","text format without a merge rule"},{"mods",modsList}});continue;}
                    File.WriteAllBytes(dest,outb);
                    Fail(failures,t.TargetPath,TextMerge.Verify(Path.GetFileName(t.TargetPath),b,mods.Select(m=>m.Value).ToList(),outb));
                    rep["archive"]=t.Archive;rep["target"]=t.TargetPath;rep["base"]=where;rep["out"]=rel;rep["mods"]=modsList;rowsT.Add(rep);
                    merged.Add(new Merged{Kind="text_merged",Archive=t.Archive,TargetPath=t.TargetPath,Out=rel});continue;
                }
                var rowsX=new List<Dictionary<string,object>>();
                var mx=XmlMerge.MergeXml(b,mods,rowsX);
                File.WriteAllBytes(dest,mx.ToXml());
                Fail(failures,t.TargetPath,XmlMerge.Verify(Node.Parse(b),mods.Select(m=>Node.Parse(m.Value)).ToList(),Node.Load(dest)));
                var lx=rowsX.Where(r=>r.ContainsKey("added")).ToList();
                rowsA.Add(new Dictionary<string,object>{{"archive",t.Archive},{"target",t.TargetPath},{"base",where},{"out",rel},{"mods",modsList},
                    {"added",lx.Sum(r=>((List<string>)r["added"]).Count)},{"changed",lx.Sum(r=>((List<string>)r["changed"]).Count)},
                    {"pools_grown",lx.Sum(r=>((List<string>)r["pools_grown"]).Count)},{"kept_3889_only",lx.Sum(r=>((List<string>)r["kept_3889_only"]).Count)},
                    {"stale_only",lx.Sum(r=>((List<string>)r["stale_only"]).Count)},
                    {"fields_kept_3889",lx.Sum(r=>((Dictionary<string,object>)r["fields"]).Values.OfType<Dictionary<string,object>>().Sum(f=>f.ContainsKey("kept_3889")?((List<string>)f["kept_3889"]).Count:0))},
                    {"other",rowsX.Where(r=>!r.ContainsKey("added")&&!Equals(r.ContainsKey("action")?r["action"]:null,"unkeyed-same")).ToList()}});
                if(opt.Detail)rowsA[rowsA.Count-1]["detail"]=lx.Select(r=>new Dictionary<string,object>{{"path",r["path"]},{"mod",r["mod"]},{"added",r["added"]},{"changed",r["changed"]},{"fields",r["fields"]}}).ToList();
                merged.Add(new Merged{Kind="merged",Archive=t.Archive,TargetPath=t.TargetPath,Out=rel});
            }
            progress("merge",targets.Count,targets.Count);
            report["merged"]=rowsA;report["text_merged"]=rowsT;report["pso_merged"]=rowsP;
            return merged;
        }
        // A merged .ymt is served as parser XML. fwPsoStoreLoader / PARSER.LoadObject tell PSO from XML, but these roots are read
        // by GTA with psoLoadFile alone (binary only; Release has no assert, so XML = NULL dereference: Suite session
        // 20261103T054408, pedpersonality). Such a merge is served only to a version whose EXE reads that root as XML too.
        public static readonly string[] PsoOnlyRoots={"CPedModelInfo__PersonalityDataList","camMetadataStore","CFiringPatternInfoManager",
            "sStatsMetadataTuning","CJunctionTemplateArray","rage__fwProfanityFilter"};
        public static string PsoXmlRefusal(string root,BuildOptions opt){
            if(!PsoOnlyRoots.Contains(root)||(opt!=null&&opt.PsoXmlRoots!=null&&opt.PsoXmlRoots.Contains(root)))return null;
            return "GTA reads "+root+" only as binary PSO and this GTAVXR version cannot read the merge as XML: the game keeps its own file";
        }
        static bool IsXml(byte[] b){int i=0;if(b.Length>=3&&b[0]==0xEF&&b[1]==0xBB&&b[2]==0xBF)i=3;while(i<b.Length&&i<200&&(b[i]==' '||b[i]=='\r'||b[i]=='\n'||b[i]=='\t'))i++;return i<b.Length&&b[i]=='<';}
        static void Fail(List<string> failures,string target,List<string> problems){foreach(var p in problems.Take(3))failures.Add(target+": "+p);}

        // ---------------------------------------------------------------- class C (mods_resources.py main)
        List<Dictionary<string,object>> ClassC(List<Dictionary<string,object>> notA,string filesDir){
            var rows=new List<Dictionary<string,object>>();int n=0;
            foreach(var x in notA){
                progress("resources",n++,notA.Count);
                string t=(string)x["target"],archive=(string)x["archive"];string ext=t.Substring(t.LastIndexOf('.')+1);
                if(ext!="ymt"&&ext!="ymap"&&ext!="ynd"&&ext!="awc"&&ext!="ydr"&&ext!="ybn")continue;
                // a PSO merge this version cannot read as XML: the game keeps its own file (not a whole-resource swap)
                if(x.ContainsKey("pso_only"))continue;
                string mod=((List<string>)x["mods"]).Last();   // the last mod that writes the file wins (whole file)
                bool launch;string where;byte[] b=game.ResourceBase(archive,t,out launch,out where);
                var info=new Dictionary<string,object>();string verdict;byte[] m=File.ReadAllBytes(mod);
                if(b==null){verdict="adopt";info["reason"]="new file (no 3889 copy)";}
                else if(ext=="ymt"){verdict=ResourceCompare.Scenario(b,m,info);if(launch){info["reason"]="launch copy: what WoV removes is its own edit";verdict="adopt";}}
                else if(ext=="ymap")verdict=ResourceCompare.Ymap(b,m,info);
                else if(ext=="ynd")verdict=ResourceCompare.Ynd(b,m,info);
                // a drawable / collision bounds a mod replaces as a whole (Open World Interiors' interior files): served by its name
                else if(ext=="ydr"||ext=="ybn"){verdict="adopt";info["reason"]="whole resource replaced (served by its streaming name)";}
                else{bool ov=game.AudioOverride(archive);verdict=ov?"keep3889":"adopt";info["reason"]=ov?"update override exists":"3889 copy is the launch file (no update override)";}
                var row=new Dictionary<string,object>{{"archive",archive},{"target",t},{"mod",mod},{"base",where},{"verdict",verdict}};
                foreach(var kv in info)row[kv.Key]=kv.Value;
                if(verdict=="adopt"){
                    string rel=Path.Combine(Flat(archive),t.Replace('/',Path.DirectorySeparatorChar));
                    row["out"]=rel;row["source"]=mod;
                }
                rows.Add(row);
            }
            return rows;
        }

        // ---------------------------------------------------------------- profile (mods_profile.py main)
        Dictionary<string,string> TuDevices(){
            var d=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);
            var b=game.Read("update/update.rpf","common/data/extratitleupdatedata.meta");
            if(b==null)throw new InvalidDataException("extratitleupdatedata.meta missing in update.rpf");
            foreach(var it in Node.Parse(b).Iter().Where(e=>e.Tag=="Item")){
                string dev=it.FindText("deviceName"),p=it.FindText("path");
                if(!String.IsNullOrEmpty(dev)&&p!=null&&p.StartsWith("update:/dlc_patch/",StringComparison.OrdinalIgnoreCase))d[p.Substring(18).Trim('/').ToLowerInvariant()]=dev;
            }
            return d;
        }
        string PackDevice(string pack){
            var b=game.Read("update/x64/dlcpacks/"+pack+"/dlc.rpf","setup2.xml");
            if(b==null)throw new InvalidDataException("no setup2.xml for pack "+pack);
            string dev=Node.Parse(b).FindText("deviceName");
            if(dev==null||!Regex.IsMatch(dev,"^[A-Za-z0-9_]+$"))throw new InvalidDataException("bad deviceName in pack "+pack);
            return dev+":/";
        }
        static string FolderOf(string mount){
            if(mount=="common:/"||mount=="platform:/"||mount=="dlcpacks:/")return mount.Substring(0,mount.Length-2);
            if(mount.StartsWith("audio:/sfx/",StringComparison.OrdinalIgnoreCase))return "audio_"+mount.Substring(11).Trim('/');
            return "dev_"+mount.Substring(0,mount.IndexOf(':'));
        }
        string Route(string archive,string target,Dictionary<string,string> tu,Dictionary<string,string> devices,out string rel){
            string a=archive.Replace('\\','/'),t=target.Replace('\\','/');
            if(a=="update/update.rpf"){
                if(t.StartsWith("common/",StringComparison.Ordinal)){rel=t.Substring(7);return "common:/";}
                if(t.StartsWith("x64/",StringComparison.Ordinal)){rel=t.Substring(4);return "platform:/";}
                if(t.StartsWith("dlc_patch/",StringComparison.Ordinal)){
                    string rest=t.Substring(10);int sl=rest.IndexOf('/');string pack=rest.Substring(0,sl);rel=rest.Substring(sl+1);
                    string dev;if(!tu.TryGetValue(pack,out dev))throw new InvalidDataException("no title-update device for dlc_patch/"+pack);
                    return dev;
                }
            }
            var m=Regex.Match(a,@"^update/x64/dlcpacks/([^/]+)/dlc\.rpf$");
            if(m.Success){string pk=m.Groups[1].Value.ToLowerInvariant();string dev;if(!devices.TryGetValue(pk,out dev))devices[pk]=dev=PackDevice(m.Groups[1].Value);rel=t;return dev;}
            throw new InvalidDataException("no route for "+archive+" | "+target);
        }
        static uint RscVer(string path){
            using(var f=File.OpenRead(path)){var h=new byte[8];if(f.Read(h,0,8)==8&&h[0]=='R'&&h[1]=='S'&&h[2]=='C'&&h[3]=='7')return BitConverter.ToUInt32(h,4);}
            return 0;
        }
        void AddFile(BuildResult r,Dictionary<string,ContentFile> byKey,string mount,string rel,string src,string what,string cat){
            if(!Regex.IsMatch(rel,"^[A-Za-z0-9_./-]+$")||rel.Contains("..")||rel.StartsWith("/",StringComparison.Ordinal))throw new InvalidDataException("unsafe relative "+rel);
            string key=mount.ToLowerInvariant()+"\u0001"+rel.ToLowerInvariant();
            if(byKey.ContainsKey(key))throw new InvalidDataException("two sources for "+mount+rel);
            if(!File.Exists(src))throw new FileNotFoundException("missing source "+src);
            long len=new FileInfo(src).Length;if(len>=1L<<32)throw new InvalidDataException("file over 4 GiB (the manifest size is 32-bit): "+src);
            if(!r.Mounts.ContainsKey(mount))r.Mounts[mount]=FolderOf(mount);
            var f=new ContentFile{Mount=mount,Relative=rel,Source=src,Bytes=len,RscVer=RscVer(src),What=what,Category=cat};
            byKey[key]=f;r.Files.Add(f);
        }
        // Pools the source-built EXE sizes with 16-bit counters: the map data and static bounds stores each feed a fwBoxStreamer whose
        // arrays are atArray<..., u16> (boxstreamer.h, array.h: count < 65535). A larger PoolSize stops the game in fwBoxStreamer::Init
        // at CGame::Init (WoV Plus MapDataStore 155020, Suite 2.3.5 session 20261103T032803). Retail 3889 takes more; this EXE does not.
        public static readonly Dictionary<string,int> PoolLimits=new Dictionary<string,int>{{"MapDataStore",65534},{"StaticBounds",65534}};
        public static string ClampPools(string text,List<string> changed){
            foreach(var kv in PoolLimits){
                var pat=new Regex("(<PoolName>"+Regex.Escape(kv.Key)+"</PoolName>\\s*<PoolSize value=\")(\\d+)(\")");
                text=pat.Replace(text,m=>{long n;if(!Int64.TryParse(m.Groups[2].Value,out n)||n<=kv.Value)return m.Value;
                    changed.Add("PoolSize "+kv.Key+" "+m.Groups[2].Value+" -> "+kv.Value+" (GTAVXR EXE limit)");return m.Groups[1].Value+kv.Value+m.Groups[3].Value;});
            }
            return text;
        }
        static string SwapValues(string text,string reference,string[] tags,List<string> changed){
            foreach(var tag in tags){
                var pat=new Regex("(<"+Regex.Escape(tag)+" value=\")([^\"]*)(\")");
                var mine=pat.Matches(text);var refs=pat.Matches(reference);
                if(mine.Count!=refs.Count)throw new InvalidDataException("gameconfig "+tag+": "+mine.Count+" vs "+refs.Count+" occurrences");
                int i=0;
                text=pat.Replace(text,m=>{string v=refs[i++].Groups[2].Value;if(v!=m.Groups[2].Value)changed.Add(tag+" "+m.Groups[2].Value+" -> "+v);return m.Groups[1].Value+v+m.Groups[3].Value;});
            }
            return text;
        }

        BuildResult Run(string id,string outDir,List<Dictionary<string,object>> rules){
            var r=new BuildResult{Id=id,Folder=outDir};
            string files=Path.Combine(outDir,"files");Directory.CreateDirectory(files);
            var report=new Dictionary<string,object>();var notA=new List<Dictionary<string,object>>();
            var merged=MergeAll(sources,files,report,notA,r.ControlFailures);
            report["not_class_a"]=notA;
            if(r.ControlFailures.Count>0)throw new InvalidDataException("merge control failed: "+String.Join(" | ",r.ControlFailures.Take(5)));
            var classc=ClassC(notA,files);report["classc"]=classc;

            var off=new HashSet<string>(opt.Off);
            if(!off.Contains("scenarios")&&(off.Contains("peds")||off.Contains("traffic")))off.Add("scenarios");  // never serve scenarios naming dropped sets
            var tu=TuDevices();var devices=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);
            var byKey=new Dictionary<string,ContentFile>();var skipped=new List<Dictionary<string,object>>();
            string profile=Path.Combine(outDir,"profile");Directory.CreateDirectory(profile);
            foreach(var kind in new[]{"merged","text_merged","pso_merged"})
                foreach(var e in merged.Where(x=>x.Kind==kind)){
                    string name=e.TargetPath.Substring(e.TargetPath.LastIndexOf('/')+1);string cat=Category(name,false);
                    if(off.Contains(cat)){skipped.Add(new Dictionary<string,object>{{"target",e.Archive+"|"+e.TargetPath},{"category",cat}});continue;}
                    string rel;string mount=Route(e.Archive,e.TargetPath,tu,devices,out rel);
                    AddFile(r,byKey,mount,rel,Path.Combine(files,e.Out),kind,cat);
                }
            // gameconfig: the density source's density / streaming values back to the merge without that source when off
            var gcChanges=new List<string>();ContentFile gc;
            if(byKey.TryGetValue("common:/\u0001data/gameconfig.xml",out gc)&&opt.DensitySource!=null&&sources.Any(s=>s.Id==opt.DensitySource)){
                var tags=(opt.SourceDensity?new string[0]:Density).Concat(opt.SourceStreaming?new string[0]:Streaming).ToArray();
                if(tags.Length>0){
                    var without=sources.Where(s=>s.Id!=opt.DensitySource).ToList();
                    var t=Targets(without).Values.FirstOrDefault(x=>x.Archive=="update/update.rpf"&&x.TargetPath=="common/data/gameconfig.xml");
                    if(t==null)throw new InvalidDataException("gameconfig without the density source missing");
                    string where;var b=game.BaseOf(t.Archive,t.TargetPath,out where);var rows=new List<Dictionary<string,object>>();
                    var refXml=Encoding.UTF8.GetString(XmlMerge.MergeXml(b,t.Mods.Select(m=>new KeyValuePair<string,byte[]>(m.Value,File.ReadAllBytes(m.Value))).ToList(),rows).ToXml());
                    string text=SwapValues(Encoding.UTF8.GetString(File.ReadAllBytes(gc.Source)),refXml,tags,gcChanges);
                    string own=Path.Combine(profile,"gameconfig.xml");File.WriteAllBytes(own,new UTF8Encoding(false).GetBytes(text));
                    gc.Source=own;gc.Bytes=new FileInfo(own).Length;gc.What="merged (profile gameconfig)";
                }
            }
            // values a switch / slider sets (WoV Plus extra density): every occurrence of the tag in the merged gameconfig
            if(opt.Gameconfig.Count>0){
                if(gc==null)throw new InvalidDataException("gameconfig values set but no merged gameconfig");
                string text=Encoding.UTF8.GetString(File.ReadAllBytes(gc.Source));
                foreach(var kv in opt.Gameconfig){
                    var pat=new Regex("(<"+Regex.Escape(kv.Key)+" value=\")([^\"]*)(\")");
                    if(pat.Matches(text).Count==0)throw new InvalidDataException("gameconfig has no "+kv.Key);
                    text=pat.Replace(text,m=>{if(m.Groups[2].Value!=kv.Value)gcChanges.Add(kv.Key+" "+m.Groups[2].Value+" -> "+kv.Value);return m.Groups[1].Value+kv.Value+m.Groups[3].Value;});
                }
                string own=Path.Combine(profile,"gameconfig.xml");File.WriteAllBytes(own,new UTF8Encoding(false).GetBytes(text));
                gc.Source=own;gc.Bytes=new FileInfo(own).Length;gc.What="merged (profile gameconfig)";
            }
            // pools GTAVXR's EXE cannot take as large as the mod sets them (ClampPools)
            if(gc!=null){
                string text=Encoding.UTF8.GetString(File.ReadAllBytes(gc.Source));string clamped=ClampPools(text,gcChanges);
                if(clamped!=text){string own=Path.Combine(profile,"gameconfig.xml");File.WriteAllBytes(own,new UTF8Encoding(false).GetBytes(clamped));
                    gc.Source=own;gc.Bytes=new FileInfo(own).Length;gc.What="merged (profile gameconfig)";}
            }
            foreach(var e in classc){
                string t=(string)e["target"],name=t.Substring(t.LastIndexOf('/')+1);bool region=t.Contains("/scenario/");string cat=Category(name,region);
                if(off.Contains(cat)){skipped.Add(new Dictionary<string,object>{{"target",t},{"category",cat}});continue;}
                string src;
                if((string)e["verdict"]=="adopt")src=(string)e["source"];
                else if((string)e["verdict"]=="keep3889"&&opt.ScenariosFull&&region)src=(string)e["mod"];
                else{skipped.Add(new Dictionary<string,object>{{"target",t},{"category",cat},{"reason",e["verdict"]}});continue;}
                if(name.EndsWith(".awc",StringComparison.OrdinalIgnoreCase)){
                    var m=Regex.Match((string)e["archive"],@"^x64/audio/sfx/([A-Za-z0-9_]+)\.rpf$");
                    if(!m.Success)throw new InvalidDataException("audio archive "+e["archive"]);
                    AddFile(r,byKey,"audio:/sfx/"+m.Groups[1].Value+"/",name,src,"classc-audio",cat);
                }else if(region){
                    string rel="levels/gta5/scenario/"+name;AddFile(r,byKey,"platform:/",rel,src,"classc-region",cat);Stream(r,byKey,name,"platform:/",rel);
                }else{
                    string rel="gtavxr_streamed/"+name;AddFile(r,byKey,"platform:/",rel,src,"classc-streamed",cat);Stream(r,byKey,name,"platform:/",rel);
                }
            }
            var packs=sources.SelectMany(s=>s.Packs).ToList();
            foreach(var p in packs){
                if(!Regex.IsMatch(p.Name,"^[A-Za-z0-9_]+$"))throw new InvalidDataException("bad pack name "+p.Name);
                AddFile(r,byKey,"dlcpacks:/",p.Name+"/dlc.rpf",p.File,"pack","pack");
            }
            var added=new List<string>();
            if(packs.Count>0){
                var b=game.Read("update/update.rpf","common/data/dlclist.xml");
                if(b==null)throw new InvalidDataException("dlclist.xml missing in update.rpf");
                string own=Path.Combine(profile,"dlclist.xml");File.WriteAllBytes(own,MergeDlclist(b,packs.Select(p=>p.Name).ToList(),added));
                AddFile(r,byKey,"common:/","data/dlclist.xml",own,"dlclist","pack");
            }
            PackRules(r,byKey,devices,profile,rules);
            if(r.Mounts.Count>MaxMounts||r.Files.Count>MaxFiles||r.Streams.Count>MaxStreams)throw new InvalidDataException("content limits: "+r.Mounts.Count+" mounts, "+r.Files.Count+" files, "+r.Streams.Count+" streams");
            var lines=new List<string>{"GTAVXR_CONTENT 1"};
            lines.AddRange(r.Mounts.Items.Select(m=>"M\t"+m.Key+"\t"+m.Value));
            lines.AddRange(r.Files.Select(f=>"F\t"+f.Mount+"\t"+f.Relative+"\t"+f.Bytes+"\t"+f.RscVer));
            lines.AddRange(r.Streams.Select(s=>"S\t"+s.Key+"\t"+s.Value));
            r.Manifest=String.Join("\n",lines)+"\n";
            if(r.Manifest.Any(c=>c>127))throw new InvalidDataException("manifest must be ASCII");
            File.WriteAllBytes(Path.Combine(profile,"manifest.txt"),Encoding.ASCII.GetBytes(r.Manifest));
            report["status"]="PROFILE_BUILT";report["id"]=id;report["off"]=off.OrderBy(x=>x).ToList();report["scenarios"]=opt.ScenariosFull?"full":"safe";
            report["sources"]=sources.Select(s=>s.Id).ToList();report["rules"]=rules;report["skipped"]=skipped;report["gameconfig_swapped"]=gcChanges;report["dlclist_added"]=added;
            report["mounts"]=r.Mounts.Count;report["files"]=r.Files.Count;report["streams"]=r.Streams.Count;report["gta_launched"]=false;
            r.Report=report;
            File.WriteAllText(Path.Combine(outDir,"build-report.json"),Json.Encode(report));
            return r;
        }
        void Stream(BuildResult r,Dictionary<string,ContentFile> byKey,string name,string mount,string rel){
            ContentFile f;
            if(!byKey.TryGetValue(mount.ToLowerInvariant()+"\u0001"+rel.ToLowerInvariant(),out f)||f.RscVer==0)throw new InvalidDataException("stream target is not a listed resource: "+name);
            if(r.Streams.Any(s=>String.Equals(s.Key,name,StringComparison.OrdinalIgnoreCase)))throw new InvalidDataException("two streams for "+name);
            r.Streams.Add(new KeyValuePair<string,string>(name,mount+rel));
        }
        static byte[] MergeDlclist(byte[] b,List<string> packs,List<string> added){
            string text=Encoding.UTF8.GetString(b);
            var have=new HashSet<string>(Regex.Matches(text,@"<Item>\s*dlcpacks:/([^/<]+)/\s*</Item>").Cast<Match>().Select(m=>m.Groups[1].Value.ToLowerInvariant()));
            var add=packs.Where(p=>!have.Contains(p.ToLowerInvariant())).ToList();added.AddRange(add);
            if(Regex.Matches(text,"</Paths>").Count!=1)throw new InvalidDataException("dlclist </Paths>");
            string eol=text.Contains("\r\n")?"\r\n":"\n";
            text=text.Replace("</Paths>",String.Concat(add.Select(p=>"\t<Item>dlcpacks:/"+p+"/</Item>"+eol+"\t"))+"</Paths>");
            return Encoding.UTF8.GetBytes(text);
        }
    }
}
