using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;

namespace GtavxrSuite.Content {
    // Entry- and field-level merge of a mod's XML onto its 3889 original. Port of tools/mods/mods_xml.py (rules and report
    // names identical) + the class B helpers of tools/mods/mods_pso.py. Every 3889 entry is kept; entries a mod adds are
    // added; a changed entry merges field by field (FieldRules: only the tuned fields); a mod's untouched entry (equal to the
    // untouched 3889 file) is no change, so a second mod never reverts the first (three-way); gameconfig pools / limits grow.
    public sealed class MergeMode {
        public bool Fold;                          // class B: string fields are hashed by the game (case does not count)
        public Func<string,string> KeyValue;       // class B: keys compare by hash
        public static readonly MergeMode Xml=new MergeMode();
        public static readonly MergeMode PsoMode=new MergeMode{Fold=true,KeyValue=XmlMerge.KeyHash};
    }
    public sealed class FieldRule { public HashSet<string> Fields;public HashSet<string> Flags; }

    public static class XmlMerge {
        public static readonly string[] Keys={"Name","name","modelName","PoolName","handlingName","child","Hash","ScenarioName","DispatchType","zoneName","PersonalityName","parent"};
        // What WoV / Plus tune, measured per field (evidence/mods-suite-20261029-v1/field-merge/RESULT.md).
        public static readonly Dictionary<string,FieldRule> FieldRules=new Dictionary<string,FieldRule>{
            {"CVehicleModelInfo__InitDataList/InitDatas",new FieldRule{
                Fields=new HashSet<string>{"frequency","maxNum","maxNumOfSameColor","swankness","drivers","identicalModelSpawnDistance","trailers","additionalTrailers","damageMapScale","damageOffsetScale"},
                Flags=new HashSet<string>{"FLAG_DONT_SPAWN_AS_AMBIENT","FLAG_DONT_SPAWN_IN_CARGEN","FLAG_SMALL_WORKER","FLAG_AVERAGE_CAR","FLAG_RICH_CAR","FLAG_POOR_CAR","FLAG_DELIVERY",
                    "FLAG_ATTACH_TRAILER_IN_CITY","FLAG_ATTACH_TRAILER_ON_HIGHWAY","FLAG_SPAWN_BOAT_ON_TRAILER","FLAG_HAS_LIVERY","FLAG_LAW_ENFORCEMENT","FLAG_ALLOWS_RAPPEL"}}},
            {"CHandlingDataMgr/HandlingData",new FieldRule{Fields=new HashSet<string>{"fCollisionDamageMult","fWeaponDamageMult","fDeformationDamageMult"},Flags=new HashSet<string>()}},
        };
        public static readonly Regex Unknown=new Regex(@"\bhash_([0-9A-F]{8})\b"),Placeholder=new Regex(@"^hash_collision_\d+$");

        public static string KeyHash(string v){
            var m=Regex.Match(v,@"^hash_([0-9A-F]{8})$");
            uint h=m.Success?UInt32.Parse(m.Groups[1].Value,NumberStyles.HexNumber):Joaat.Hash(v);
            return "h:"+h.ToString("x8");
        }
        static string Norm(MergeMode mode,string v){return mode.KeyValue!=null?mode.KeyValue(v):v.ToLowerInvariant();}
        public static string KeyOf(Node item,MergeMode mode){
            string k=item.Get("key");
            if(!String.IsNullOrEmpty(k))return "key="+Norm(mode,k);  // atMap entry
            foreach(string name in Keys){
                var c=item.Find(name);
                if(c!=null){string v=c.Get("value")??(c.Text??"").Trim();if(v.Length>0)return name+"="+Norm(mode,v);}
            }
            if(item.Find("Build")!=null&&item.Find("Platforms")!=null)  // gameconfig ConfigArray entry
                return "config="+String.Join("|",new[]{"Build","Platforms","Param"}.Select(t=>(item.FindText(t)??"").Trim().ToLowerInvariant()));
            return null;
        }
        // Lists in document order: Item entries, or siblings repeating one tag (dispatch.meta VehicleSets/Vehicle).
        public static List<KeyValuePair<string,Node>> Lists(Node root){
            var order=new List<string>();var map=new Dictionary<string,Node>();
            Walk(root,root.Tag,order,map);
            return order.Select(p=>new KeyValuePair<string,Node>(p,map[p])).ToList();
        }
        static void Walk(Node e,string path,List<string> order,Dictionary<string,Node> map){
            var tags=new HashSet<string>(e.Children.Select(c=>c.Tag));
            if(e.Count>0&&tags.Count==1&&(tags.Contains("Item")||e.Count>1)&&e.Children.Any(c=>c.Count>0)){
                if(!map.ContainsKey(path))order.Add(path);
                map[path]=e;return;
            }
            foreach(var c in e.Children)Walk(c,path+"/"+c.Tag,order,map);
        }
        public static Dictionary<string,Node> ListMap(Node root){var d=new Dictionary<string,Node>();foreach(var kv in Lists(root))d[kv.Key]=kv.Value;return d;}
        public static List<string> Keyed(IList<Node> items,MergeMode mode){
            var keys=new List<string>();var seen=new Dictionary<string,int>();
            foreach(var i in items){
                string k=KeyOf(i,mode);if(k==null)return null;
                int n;seen.TryGetValue(k,out n);seen[k]=++n;
                keys.Add(n==1?k:k+"#"+n);
            }
            return keys;
        }

        public static List<Dictionary<string,object>> Merge(Node baseRoot,Node modRoot,string modName,bool gameconfig,Node origRoot,MergeMode mode){
            var report=new List<Dictionary<string,object>>();
            var lbl=Lists(baseRoot);var lb=lbl.ToDictionary(x=>x.Key,x=>x.Value);var lm=Lists(modRoot);
            var lo=origRoot!=null?ListMap(origRoot):new Dictionary<string,Node>();
            foreach(var kv in lm){
                string path=kv.Key;Node em=kv.Value,eb;
                if(!lb.TryGetValue(path,out eb)){report.Add(Row("path",path,"action","list-missing-in-3889-kept-3889","mod",modName));continue;}
                var ib=eb.Children.ToList();var im=em.Children.ToList();
                var kb=Keyed(ib,mode);var km=Keyed(im,mode);
                Node eo;var io=lo.TryGetValue(path,out eo)?eo.Children.ToList():new List<Node>();
                var ko=io.Count>0?Keyed(io,mode):null;
                var orig=new Dictionary<string,Node>();if(ko!=null)for(int i=0;i<ko.Count;i++)orig[ko[i]]=io[i];
                if((kb==null||km==null)&&ib.Count==1&&im.Count==1){
                    // A one-entry array (gameconfig's ConfigArray holds the whole config): merge inside that entry.
                    report.AddRange(Merge(ib[0],im[0],modName,gameconfig,io.Count==1?io[0]:null,mode));continue;
                }
                if(kb==null||km==null){
                    bool same=ib.Count==im.Count&&ib.Zip(im,(a,b)=>a.Canon(mode.Fold)==b.Canon(mode.Fold)).All(x=>x);
                    report.Add(Row("path",path,"action",same?"unkeyed-same":"unkeyed-differs-kept-3889","mod",modName,"n3889",ib.Count,"nmod",im.Count));continue;
                }
                var pos=new Dictionary<string,int>();for(int n=0;n<kb.Count;n++)pos[kb[n]]=n;
                var added=new List<string>();var changed=new List<string>();var grown=new List<string>();
                var fields=new Dictionary<string,object>();var staleOnly=new List<string>();
                for(int i=0;i<km.Count;i++){
                    string k=km[i];Node item=im[i];int p;
                    if(!pos.TryGetValue(k,out p)){eb.Children.Add(item);added.Add(k);continue;}
                    Node old=ib[p];Node o;bool hasOrig=orig.TryGetValue(k,out o);
                    string ci=item.Canon(mode.Fold);
                    if(old.Canon(mode.Fold)==ci||(hasOrig&&o.Canon(mode.Fold)==ci))continue;
                    if(k.StartsWith("config=",StringComparison.Ordinal)){
                        // gameconfig: only the general config (Any build, Any platform) is merged; WoV's other configs are the
                        // 2245 originals, so 3889's stay.
                        if(k=="config=any|any|")report.AddRange(Merge(old,item,modName,gameconfig,hasOrig?o:null,mode));
                        else report.Add(Row("path",path,"key",k,"action","config-kept-3889","mod",modName));
                        continue;
                    }
                    if(gameconfig&&k.StartsWith("poolname=",StringComparison.OrdinalIgnoreCase)){
                        int? so=IntChild(old,"PoolSize"),sm=IntChild(item,"PoolSize");
                        if(so!=null&&sm!=null&&sm<=so)continue;  // the larger 3889 pool stays
                        grown.Add(k);eb.Children[p]=item;continue;
                    }
                    List<string> applied,keptFields;
                    if(!MergeFields(old,item,hasOrig?o:null,path,mode,out applied,out keptFields)){
                        changed.Add(k);fields[k]=new Dictionary<string,object>{{"whole_entry",true}};eb.Children[p]=item;continue;
                    }
                    if(applied.Count>0)changed.Add(k);else if(keptFields.Count>0)staleOnly.Add(k);
                    if(applied.Count>0||keptFields.Count>0)fields[k]=new Dictionary<string,object>{{"applied",applied},{"kept_3889",keptFields}};
                }
                var skm=new HashSet<string>(km);
                var kept=kb.Where(x=>!skm.Contains(x)).ToList();
                report.Add(Row("path",path,"n3889",ib.Count,"nmod",im.Count,"added",added,"changed",changed,"pools_grown",grown,
                    "kept_3889_only",kept,"fields",fields,"stale_only",staleOnly,"mod",modName));
            }
            // Scalars outside lists (e.g. population multipliers): the mod value wins, each one reported.
            var listPaths=new HashSet<string>(lbl.Select(x=>x.Key).Concat(lm.Select(x=>x.Key)));
            Scalars(baseRoot,modRoot,baseRoot.Tag,listPaths,report,modName,gameconfig,origRoot,mode);
            return report;
        }
        static Dictionary<string,object> Row(params object[] kv){var d=new Dictionary<string,object>();for(int i=0;i+1<kv.Length;i+=2)d[(string)kv[i]]=kv[i+1];return d;}

        static Dictionary<string,Node> UniqueFields(Node e){
            if(e.Children.GroupBy(x=>x.Tag).Any(g=>g.Count()>1))return null;
            return e.Children.ToDictionary(x=>x.Tag);
        }
        // One changed entry, field by field, into cur (in place). orig = the untouched 3889 entry or null (an entry a
        // previous mod added). false = an entry repeats a field tag (no field identity): the caller takes the whole entry.
        public static bool MergeFields(Node cur,Node mod,Node orig,string path,MergeMode mode,out List<string> applied,out List<string> kept){
            applied=new List<string>();kept=new List<string>();
            var fc=UniqueFields(cur);var fm=UniqueFields(mod);var fo=orig!=null?UniqueFields(orig):new Dictionary<string,Node>();
            if(fc==null||fm==null||fo==null)return false;
            FieldRule rule;FieldRules.TryGetValue(path,out rule);
            foreach(var t in fm){
                string tag=t.Key;Node x=t.Value,o,c;fo.TryGetValue(tag,out o);fc.TryGetValue(tag,out c);
                string cx=x.Canon(mode.Fold);
                if((o!=null&&o.Canon(mode.Fold)==cx)||(c!=null&&c.Canon(mode.Fold)==cx))continue;
                if(rule!=null&&tag=="flags"&&c!=null){
                    var refSet=new HashSet<string>(Toks((o??c).Text));var mine=Toks(x.Text);var now=Toks(c.Text);
                    var plus=mine.Where(f=>!refSet.Contains(f)&&rule.Flags.Contains(f)&&!now.Contains(f)).ToList();
                    var mineSet=new HashSet<string>(mine);
                    var minus=new HashSet<string>(refSet.Where(f=>!mineSet.Contains(f)&&rule.Flags.Contains(f)));
                    var sym=new HashSet<string>(mineSet);sym.SymmetricExceptWith(refSet);
                    var ignored=sym.Where(f=>!rule.Flags.Contains(f)).OrderBy(f=>f,StringComparer.Ordinal).ToList();
                    var nw=now.Where(f=>!minus.Contains(f)).Concat(plus).ToList();
                    if(!nw.SequenceEqual(now)){
                        c.Text=String.Join(" ",nw);
                        applied.Add("flags"+String.Concat(plus.Select(f=>" +"+f))+String.Concat(minus.OrderBy(f=>f,StringComparer.Ordinal).Select(f=>" -"+f)));
                    }
                    if(ignored.Count>0)kept.Add("flags "+String.Join(" ",ignored));
                    continue;
                }
                if(rule!=null&&!rule.Fields.Contains(tag)){kept.Add(tag);continue;}
                if(c==null)cur.Children.Add(x);else cur.Children[cur.Children.IndexOf(c)]=x;
                applied.Add(tag);
            }
            return true;
        }
        static List<string> Toks(string s){return (s??"").Split((char[])null,StringSplitOptions.RemoveEmptyEntries).ToList();}

        static double? NumValue(Node e){double d;return Double.TryParse(e.Get("value"),NumberStyles.Float,CultureInfo.InvariantCulture,out d)?(double?)d:null;}
        static int? IntChild(Node e,string tag){var c=e.Find(tag);int v;return c!=null&&Int32.TryParse(c.Get("value"),NumberStyles.Integer,CultureInfo.InvariantCulture,out v)?(int?)v:null;}

        static void Scalars(Node b,Node m,string path,HashSet<string> listPaths,List<Dictionary<string,object>> report,string modName,bool gameconfig,Node orig,MergeMode mode){
            var bc=new Dictionary<string,Node>();foreach(var c in b.Children)bc[c.Tag]=c;   // last wins, like a Python dict
            var oc=new Dictionary<string,Node>();if(orig!=null)foreach(var c in orig.Children)oc[c.Tag]=c;
            var rep=m.Children.GroupBy(x=>x.Tag).ToDictionary(g=>g.Key,g=>g.Count());
            foreach(var c in m.Children.ToList()){
                string p=path+"/"+c.Tag;
                if(listPaths.Contains(p)||c.Tag=="Item"||rep[c.Tag]>1)continue;
                Node ocv;
                if(oc.TryGetValue(c.Tag,out ocv)&&ocv.Canon(mode.Fold)==c.Canon(mode.Fold))continue;  // the mod left it as 3889
                Node o;bc.TryGetValue(c.Tag,out o);
                if(o==null){
                    if(gameconfig)report.Add(Row("path",p,"action","mod-only-kept-3889","mod",modName));  // a block 3889 lacks: not added
                    else{b.Children.Add(c);report.Add(Row("path",p,"action","scalar-added","mod",modName));}
                }else if(c.Count==0&&o.Count==0){
                    if(gameconfig&&NumValue(o)!=null&&NumValue(c)!=null){
                        if(NumValue(c)>NumValue(o)){report.Add(Row("path",p,"action","limit-grown","old",o.Get("value"),"new",c.Get("value"),"mod",modName));o.Set("value",c.Get("value"));}
                    }else if(o.Canon(mode.Fold)!=c.Canon(mode.Fold)){
                        report.Add(Row("path",p,"action","scalar-changed","mod",modName));
                        o.Attrs.Clear();o.Attrs.AddRange(c.Attrs);o.Text=c.Text;
                    }
                }else if(o.Children.Concat(c.Children).All(x=>x.Tag=="Item"&&x.Count==0)){
                    if(o.Canon(mode.Fold)!=c.Canon(mode.Fold))report.Add(Row("path",p,"action","simple-list-differs-kept-3889","mod",modName));
                }else{
                    Node next;oc.TryGetValue(c.Tag,out next);
                    Scalars(o,c,p,listPaths,report,modName,gameconfig,next,mode);
                }
            }
        }

        // tools/mods/mods_xml.py merge_files: the 3889 file + mods in order -> merged tree + report.
        public static Node MergeXml(byte[] baseBytes,IList<KeyValuePair<string,byte[]>> mods,List<Dictionary<string,object>> report){
            var b=Node.Parse(baseBytes);var orig=Node.Parse(baseBytes);
            bool gc=b.Tag=="CGameConfig"||b.Tag=="fwAllConfigs";
            foreach(var m in mods)report.AddRange(Merge(b,Node.Parse(m.Value),m.Key,gc,orig,MergeMode.Xml));
            return b;
        }

        // ---------------------------------------------------------------- class B (tools/mods/mods_pso.py)
        static bool SameHash(string placeholder,Node e){
            string t=e!=null?(e.Text??"").Trim():"";
            if(t.Length==0||e.Count>0)return false;
            uint h=Joaat.Hash(placeholder);
            var m=Regex.Match(t,@"^hash_([0-9A-F]{8})$");
            return m.Success?h==UInt32.Parse(m.Groups[1].Value,NumberStyles.HexNumber):(h==Joaat.Hash(t,true)||h==Joaat.Hash(t,false));
        }
        static void FixPair(Node m,Node b,Dictionary<string,int> counts,HashSet<Node> skip){
            var seen=new Dictionary<string,int>();var bk=new Dictionary<string,List<Node>>();
            if(b!=null)foreach(var c in b.Children){List<Node> l;if(!bk.TryGetValue(c.Tag,out l))bk[c.Tag]=l=new List<Node>();l.Add(c);}
            for(int i=0;i<m.Count;i++){
                var c=m[i];int n;seen.TryGetValue(c.Tag,out n);seen[c.Tag]=n+1;
                List<Node> lst;Node o=bk.TryGetValue(c.Tag,out lst)&&n<lst.Count?lst[n]:null;
                if(c.Count==0&&Placeholder.IsMatch((c.Text??"").Trim())){
                    if(SameHash((c.Text??"").Trim(),o)){m[i]=o.DeepCopy();counts["same_as_3889"]++;}else counts["mod_value"]++;
                }else if(c.Count>0&&!skip.Contains(c))FixPair(c,o,counts,skip);
            }
        }
        public static Dictionary<string,int> FixPlaceholders(Node modRoot,Node origRoot){
            var counts=new Dictionary<string,int>{{"same_as_3889",0},{"mod_value",0}};
            var lo=ListMap(origRoot);var lm=Lists(modRoot);
            foreach(var kv in lm){
                var km=Keyed(kv.Value.Children,MergeMode.PsoMode);Node eo;lo.TryGetValue(kv.Key,out eo);
                var ko=eo!=null?Keyed(eo.Children,MergeMode.PsoMode):null;
                var om=new Dictionary<string,Node>();if(ko!=null)for(int i=0;i<ko.Count;i++)om[ko[i]]=eo[i];
                if(km!=null){var items=kv.Value.Children.ToList();for(int i=0;i<km.Count&&i<items.Count;i++){Node o;om.TryGetValue(km[i],out o);FixPair(items[i],o,counts,new HashSet<Node>());}}
            }
            FixPair(modRoot,origRoot,counts,new HashSet<Node>(lm.Select(x=>x.Value)));  // the fields outside lists
            return counts;
        }
        public static int RebuildUnknown(Node root){
            int n=0;
            foreach(var e in root.Iter()){
                if(e.Text!=null&&Unknown.IsMatch(e.Text))n++;
                foreach(var a in e.Attrs)if(Unknown.IsMatch(a.Value))n++;
                if(e.Text!=null&&Unknown.IsMatch(e.Text))e.Text=Unknown.Replace(e.Text,m=>Joaat.Preimage(UInt32.Parse(m.Groups[1].Value,NumberStyles.HexNumber)));
                for(int i=0;i<e.Attrs.Count;i++)
                    if(Unknown.IsMatch(e.Attrs[i].Value))e.Attrs[i]=new KeyValuePair<string,string>(e.Attrs[i].Key,Unknown.Replace(e.Attrs[i].Value,m=>Joaat.Preimage(UInt32.Parse(m.Groups[1].Value,NumberStyles.HexNumber))));
            }
            return n;
        }
        public static Node LoadAny(byte[] data,PsoNames names){
            if(Pso.IsPso(data))return new Pso(data,names).Tree();
            int i=data.Length>=3&&data[0]==0xEF&&data[1]==0xBB&&data[2]==0xBF?3:0;
            if(i==0)return Node.Parse(data);
            var d=new byte[data.Length-i];Buffer.BlockCopy(data,i,d,0,d.Length);return Node.Parse(d);
        }
        // The XML-form .ymt of the mods onto the 3889 binary PSO original; the result is XML (every loader accepts it).
        public static Node MergePso(byte[] baseBytes,IList<KeyValuePair<string,byte[]>> mods,PsoNames names,List<Dictionary<string,object>> report,Dictionary<string,int> fixedCounts,out int rebuilt){
            var b=LoadAny(baseBytes,names);var orig=b.DeepCopy();
            foreach(var m in mods){
                var mr=LoadAny(m.Value,names);
                foreach(var kv in FixPlaceholders(mr,orig)){int v;fixedCounts.TryGetValue(kv.Key,out v);fixedCounts[kv.Key]=v+kv.Value;}
                report.AddRange(Merge(b,mr,m.Key,false,orig,MergeMode.PsoMode));
            }
            rebuilt=RebuildUnknown(b);
            return b;
        }

        // ---------------------------------------------------------------- controls (tools/mods/mods_merge.py verify*)
        static Dictionary<string,HashSet<string>> KeySets(Node root,Func<string,string> map){
            var res=new Dictionary<string,HashSet<string>>();
            foreach(var kv in Lists(root)){var k=Keyed(kv.Value.Children,MergeMode.Xml);if(k!=null)res[kv.Key]=new HashSet<string>(k.Select(map));}
            return res;
        }
        public static List<string> Verify(Node b,IList<Node> mods,Node dest){
            var kd=KeySets(dest,x=>x);var problems=new List<string>();
            foreach(var kv in KeySets(b,x=>x)){
                HashSet<string> have;kd.TryGetValue(kv.Key,out have);
                var lost=kv.Value.Where(x=>have==null||!have.Contains(x)).ToList();
                if(lost.Count>0&&!kv.Key.EndsWith("ConfigArray",StringComparison.Ordinal))problems.Add("3889 entries lost in "+kv.Key+": "+String.Join(",",lost.OrderBy(x=>x,StringComparer.Ordinal).Take(5)));
            }
            foreach(var m in mods)foreach(var kv in KeySets(m,x=>x)){
                HashSet<string> have;
                if(kv.Key.EndsWith("ConfigArray",StringComparison.Ordinal)||!kd.TryGetValue(kv.Key,out have))continue;
                var miss=kv.Value.Where(x=>!have.Contains(x)).ToList();
                if(miss.Count>0)problems.Add("mod entries missing in "+kv.Key+": "+String.Join(",",miss.OrderBy(x=>x,StringComparer.Ordinal).Take(5)));
            }
            return problems;
        }
        static readonly Regex KeyParts=new Regex(@"^([^=]+)=(.*?)(#\d+)?$"),HashedKey=new Regex(@"^(?:hash_|h:)([0-9a-f]{8})$");
        public static string HashKey(string k){
            var m=KeyParts.Match(k);if(!m.Success)return k;
            var u=HashedKey.Match(m.Groups[2].Value);
            uint h=u.Success?UInt32.Parse(u.Groups[1].Value,NumberStyles.HexNumber):Joaat.Hash(m.Groups[2].Value);
            return m.Groups[1].Value+"\u0001"+h.ToString("x8")+"\u0001"+m.Groups[3].Value;
        }
        // Identity as the game reads it: hash-field texts by joaat (collision texts and hash_XXXXXXXX compare equal).
        public static string HashCanon(Node e){var sb=new StringBuilder();HashCanonInto(e,sb);return sb.ToString();}
        static string Tok(string t){
            if(Node.IsNumber(t))return "n"+Node.NumKey(t);
            var m=Regex.Match(t,@"^hash_([0-9A-F]{8})$");
            return "h"+(m.Success?UInt32.Parse(m.Groups[1].Value,NumberStyles.HexNumber):Joaat.Hash(t)).ToString("x8");
        }
        static void HashCanonInto(Node e,StringBuilder sb){
            sb.Append('\u0001').Append(e.Tag).Append('\u0002');
            foreach(var a in e.Attrs.OrderBy(x=>x.Key,StringComparer.Ordinal)){sb.Append(a.Key).Append('\u0003');foreach(var t in Toks(a.Value))sb.Append(Tok(t)).Append(' ');sb.Append('\u0004');}
            foreach(var t in Toks(e.Text))sb.Append(Tok(t)).Append(' ');
            sb.Append('\u0005');foreach(var c in e.Children)HashCanonInto(c,sb);sb.Append('\u0006');
        }
        public static List<string> VerifyPso(Node b,IList<Node> mods,Node dest,List<Dictionary<string,object>> rows){
            var problems=new List<string>();
            var kd=KeySets(dest,HashKey);
            foreach(var kv in Lists(dest)){var k=Keyed(kv.Value.Children,MergeMode.Xml);if(k!=null&&k.Select(HashKey).Distinct().Count()!=k.Count)problems.Add("duplicate entries (same hash) in "+kv.Key);}
            foreach(var kv in KeySets(b,HashKey)){
                HashSet<string> have;kd.TryGetValue(kv.Key,out have);
                var lost=kv.Value.Where(x=>have==null||!have.Contains(x)).ToList();
                if(lost.Count>0)problems.Add("3889 entries lost in "+kv.Key+": "+lost.Count);
            }
            foreach(var m in mods)foreach(var kv in KeySets(m,HashKey)){
                HashSet<string> have;if(!kd.TryGetValue(kv.Key,out have))continue;
                var miss=kv.Value.Where(x=>!have.Contains(x)).ToList();
                if(miss.Count>0)problems.Add("mod entries missing in "+kv.Key+": "+miss.Count);
            }
            var changed=new Dictionary<string,HashSet<string>>();
            foreach(var r in rows)if(r.ContainsKey("changed")){
                HashSet<string> s;string p=(string)r["path"];if(!changed.TryGetValue(p,out s))changed[p]=s=new HashSet<string>();
                foreach(var x in (List<string>)r["changed"])s.Add(HashKey(x));
            }
            var lo=Lists(b);var ld=ListMap(dest);int differ=0;
            foreach(var kv in lo){
                var ko=Keyed(kv.Value.Children,MergeMode.Xml);Node ed;ld.TryGetValue(kv.Key,out ed);
                var kd2=ed!=null?Keyed(ed.Children,MergeMode.Xml):null;
                if(ko==null||kd2==null)continue;
                var dm=new Dictionary<string,Node>();for(int i=0;i<kd2.Count;i++)dm[HashKey(kd2[i])]=ed[i];
                HashSet<string> ch;changed.TryGetValue(kv.Key,out ch);
                for(int i=0;i<ko.Count;i++){
                    string k=HashKey(ko[i]);Node d;
                    if((ch==null||!ch.Contains(k))&&dm.TryGetValue(k,out d)&&HashCanon(kv.Value[i])!=HashCanon(d)){
                        differ++;if(differ<=3)problems.Add("unchanged 3889 entry reads back different: "+kv.Key+" "+ko[i]);
                    }
                }
            }
            return problems;
        }
    }
}
