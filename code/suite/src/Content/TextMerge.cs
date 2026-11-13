using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;

namespace GtavxrSuite.Content {
    // Insertion-ordered map (Python dict semantics: overwriting keeps the first position).
    public sealed class OrderedMap<K,V> {
        readonly List<K> order=new List<K>();readonly Dictionary<K,V> map;
        public OrderedMap(){map=new Dictionary<K,V>();}
        public OrderedMap(IEqualityComparer<K> cmp){map=new Dictionary<K,V>(cmp);}
        public V this[K k]{get{return map[k];}set{if(!map.ContainsKey(k))order.Add(k);map[k]=value;}}
        public bool TryGetValue(K k,out V v){return map.TryGetValue(k,out v);}
        public bool ContainsKey(K k){return map.ContainsKey(k);}
        public IEnumerable<K> Keys{get{return order;}}
        public IEnumerable<KeyValuePair<K,V>> Items{get{foreach(var k in order)yield return new KeyValuePair<K,V>(k,map[k]);}}
        public int Count{get{return order.Count;}}
        public OrderedMap<K,V> Copy(){var c=new OrderedMap<K,V>();foreach(var kv in Items)c[kv.Key]=kv.Value;return c;}
    }

    // Three-way merge of the text data files (port of tools/mods/mods_text.py), grammars from the game's parsers:
    //   popcycle.dat       Peds/PopCycle.cpp: POP_SCHEDULE: / name / rows / END_POP_SCHEDULE; '/' comment. Entry = a schedule.
    //   relationships.dat  Peds/Relationships.cpp: Hate/Dislike/Like/Respect = relations of the current group. Entry = pair.
    //   *.ipl (text)       section / lines / 'end'. Entry = one line (numbers by value).
    // Output: ASCII, CRLF, no BOM; the mods' comment lines are not copied.
    public static class TextMerge {
        static readonly string[] Rels={"Hate","Dislike","Like","Respect"};
        static readonly Regex Seps=new Regex("[ ,\\t]+");
        public static List<string> Lines(byte[] data){
            string s=new UTF8Encoding(false,false).GetString(data);
            if(s.Length>0&&s[0]=='﻿')s=s.Substring(1);
            var l=Regex.Split(s,@"\r\n|\r|\n|\x0B|\x0C|\x1C|\x1D|\x1E|\x85|\u2028|\u2029").ToList();
            if(l.Count>0&&l[l.Count-1].Length==0)l.RemoveAt(l.Count-1);  // splitlines: no empty last line
            return l;
        }
        public static string Canon(string line){
            return String.Join("\u0001",Seps.Split(line.Trim()).Where(t=>t.Length>0).Select(t=>Node.IsNumber(t)?Node.NumKey(t):t.ToLowerInvariant()));
        }
        public static byte[] Bytes(List<string> lines){
            var sb=new StringBuilder();foreach(var l in lines)sb.Append(l).Append("\r\n");
            var b=new byte[sb.Length];for(int i=0;i<sb.Length;i++){char c=sb[i];b[i]=c<128?(byte)c:(byte)'?';}
            return b;
        }

        // ---------------------------------------------------------------- popcycle.dat
        public static void Schedules(List<string> lines,out List<string> head,out OrderedMap<string,List<string>> blocks){
            head=new List<string>();blocks=new OrderedMap<string,List<string>>();List<string> cur=null;
            foreach(var l in lines){
                if(cur==null){
                    if(l.StartsWith("POP_SCHEDULE:",StringComparison.Ordinal))cur=new List<string>{l};
                    else if(blocks.Count==0)head.Add(l);
                    continue;
                }
                cur.Add(l);
                if(l.StartsWith("END_POP_SCHEDULE",StringComparison.Ordinal)){
                    var data=cur.Skip(1).Where(x=>!x.StartsWith("/",StringComparison.Ordinal)&&x.Length>2).ToList();  // the parser's comment / short-line skip
                    blocks[data[0].ToLowerInvariant()]=cur;cur=null;
                }
            }
        }
        public static string SchedCanon(List<string> block){return String.Join("\u0002",block.Where(x=>!x.StartsWith("/",StringComparison.Ordinal)&&x.Length>2).Select(Canon));}
        static Dictionary<string,object> MergePopcycle(byte[] b,IList<byte[]> mods,out byte[] output){
            List<string> head;OrderedMap<string,List<string>> res;Schedules(Lines(b),out head,out res);
            var orig=res.Copy();var changed=new List<string>();var added=new List<string>();int unchanged=0;
            var modBlocks=new List<OrderedMap<string,List<string>>>();
            foreach(var m in mods){
                List<string> h;OrderedMap<string,List<string>> mb;Schedules(Lines(m),out h,out mb);modBlocks.Add(mb);
                foreach(var kv in mb.Items){
                    List<string> ob;
                    if(orig.TryGetValue(kv.Key,out ob)&&SchedCanon(ob)==SchedCanon(kv.Value)){unchanged++;continue;}
                    (orig.ContainsKey(kv.Key)?changed:added).Add(kv.Key);
                    res[kv.Key]=kv.Value;
                }
            }
            var lines=new List<string>(head);
            foreach(var kv in res.Items){lines.AddRange(kv.Value);lines.Add("");}
            output=Bytes(lines);
            return new Dictionary<string,object>{{"changed",changed},{"added",added},{"unchanged_by_mod",unchanged},
                {"kept_3889_only",orig.Keys.Where(k=>modBlocks.All(mb=>!mb.ContainsKey(k))).ToList()}};
        }

        // ---------------------------------------------------------------- relationships.dat
        public static void Relations(List<string> lines,out List<string> groups,out OrderedMap<string,string> rels){
            groups=new List<string>();rels=new OrderedMap<string,string>();string cur=null;
            foreach(var l in lines){
                if(l.StartsWith("#",StringComparison.Ordinal)||l.Trim().Length==0||l.Trim().StartsWith("//",StringComparison.Ordinal))continue;
                var toks=Seps.Split(l.Trim()).Where(t=>t.Length>0).ToList();
                if(Rels.Contains(toks[0])){
                    foreach(var t in toks.Skip(1))rels[cur+"\u0001"+t.ToUpperInvariant()]=toks[0];  // a later line for the same pair overrides
                    foreach(var t in toks.Skip(1))if(!groups.Contains(t.ToUpperInvariant()))groups.Add(t.ToUpperInvariant());  // unknown targets become groups
                }else{
                    cur=toks[0].ToUpperInvariant();if(!groups.Contains(cur))groups.Add(cur);
                }
            }
        }
        static Dictionary<string,object> MergeRelationships(byte[] b,IList<byte[]> mods,out byte[] output){
            var bl=Lines(b);List<string> groups;OrderedMap<string,string> res;Relations(bl,out groups,out res);
            var orig=res.Copy();var changed=new List<string>();var added=new List<string>();var groupsAdded=new List<string>();
            var modRels=new List<OrderedMap<string,string>>();
            foreach(var m in mods){
                List<string> g;OrderedMap<string,string> r;Relations(Lines(m),out g,out r);modRels.Add(r);
                foreach(var x in g)if(!groups.Contains(x)){groups.Add(x);groupsAdded.Add(x);}
                foreach(var kv in r.Items){
                    string ov;if(orig.TryGetValue(kv.Key,out ov)&&ov==kv.Value)continue;
                    string cv;res.TryGetValue(kv.Key,out cv);
                    if(cv!=kv.Value){var p=kv.Key.Split('\u0001');(orig.ContainsKey(kv.Key)?changed:added).Add(p[0]+" "+kv.Value+" "+p[1]);}
                    res[kv.Key]=kv.Value;
                }
            }
            int first=bl.FindIndex(l=>!l.StartsWith("#",StringComparison.Ordinal));
            var lines=bl.Take(first<0?bl.Count:first).ToList();lines.AddRange(groups);
            foreach(var g in groups){
                var mine=res.Items.Where(kv=>kv.Key.Split('\u0001')[0]==g).Select(kv=>new KeyValuePair<string,string>(kv.Key.Split('\u0001')[1],kv.Value)).ToList();
                if(mine.Count==0)continue;
                lines.Add("");lines.Add(g);
                foreach(var v in Rels){var ts=mine.Where(x=>x.Value==v).Select(x=>x.Key).ToList();if(ts.Count>0)lines.Add("    "+v+" "+String.Join(" ",ts));}
            }
            output=Bytes(lines);
            return new Dictionary<string,object>{{"changed",changed},{"added",added},{"groups_added",groupsAdded},
                {"kept_3889_only",orig.Keys.Count(k=>modRels.All(r=>!r.ContainsKey(k)))}};
        }

        // ---------------------------------------------------------------- text .ipl
        public static void Ipl(List<string> lines,out List<string> head,out OrderedMap<string,List<string>> secs){
            head=new List<string>();secs=new OrderedMap<string,List<string>>();string cur=null;
            foreach(var l in lines){
                string s=l.Trim();
                if(s.Length==0||s.StartsWith("#",StringComparison.Ordinal)||s.StartsWith("//",StringComparison.Ordinal)){
                    if(secs.Count==0&&cur==null&&s.StartsWith("#",StringComparison.Ordinal))head.Add(l);
                    continue;
                }
                if(cur==null){cur=s.ToLowerInvariant();if(!secs.ContainsKey(cur))secs[cur]=new List<string>();}
                else if(s.ToLowerInvariant()=="end")cur=null;
                else secs[cur].Add(s);
            }
        }
        static Dictionary<string,object> MergeIpl(byte[] b,IList<byte[]> mods,out byte[] output){
            List<string> head;OrderedMap<string,List<string>> res;Ipl(Lines(b),out head,out res);
            var orig=new Dictionary<string,HashSet<string>>();foreach(var kv in res.Items)orig[kv.Key]=new HashSet<string>(kv.Value.Select(Canon));
            int addedN=0,dropped=0;var secsAdded=new List<string>();
            foreach(var m in mods){
                List<string> h;OrderedMap<string,List<string>> ms;Ipl(Lines(m),out h,out ms);
                foreach(var kv in ms.Items){
                    if(!res.ContainsKey(kv.Key)){res[kv.Key]=new List<string>();secsAdded.Add(kv.Key);}
                    var have=new HashSet<string>(res[kv.Key].Select(Canon));HashSet<string> o;orig.TryGetValue(kv.Key,out o);
                    foreach(var x in kv.Value){string c=Canon(x);if(!have.Contains(c)&&(o==null||!o.Contains(c))){res[kv.Key].Add(x);have.Add(c);addedN++;}}
                    if(o!=null){var mine=new HashSet<string>(kv.Value.Select(Canon));dropped+=o.Count(x=>!mine.Contains(x));}
                }
            }
            var lines=new List<string>(head);
            foreach(var kv in res.Items){lines.Add(kv.Key);lines.AddRange(kv.Value);lines.Add("end");}
            output=Bytes(lines);
            return new Dictionary<string,object>{{"added",addedN},{"dropped_by_mod_kept",dropped},{"sections_added",secsAdded}};
        }

        // null = a text format without a merge rule.
        public static Dictionary<string,object> Merge(string name,byte[] b,IList<byte[]> mods,out byte[] output){
            output=null;name=name.ToLowerInvariant();
            if(name=="popcycle.dat")return MergePopcycle(b,mods,out output);
            if(name=="relationships.dat")return MergeRelationships(b,mods,out output);
            if(name.EndsWith(".ipl",StringComparison.Ordinal))return MergeIpl(b,mods,out output);
            return null;
        }

        // Control: re-read the output with the game grammar; no 3889 entry lost, every mod change present.
        public static List<string> Verify(string name,byte[] b,IList<byte[]> mods,byte[] dest){
            name=name.ToLowerInvariant();var problems=new List<string>();
            if(name=="popcycle.dat"){
                List<string> h;OrderedMap<string,List<string>> got,want;
                Schedules(Lines(dest),out h,out got);Schedules(Lines(b),out h,out want);var orig=want.Copy();
                foreach(var m in mods){OrderedMap<string,List<string>> mb;Schedules(Lines(m),out h,out mb);
                    foreach(var kv in mb.Items){List<string> o;if(!orig.TryGetValue(kv.Key,out o)||SchedCanon(o)!=SchedCanon(kv.Value))want[kv.Key]=kv.Value;}}
                foreach(var kv in want.Items){List<string> g;if(!got.TryGetValue(kv.Key,out g)||SchedCanon(g)!=SchedCanon(kv.Value))problems.Add("schedule "+kv.Key+" wrong or missing");}
            }else if(name=="relationships.dat"){
                List<string> g;OrderedMap<string,string> got,orig;Relations(Lines(dest),out g,out got);Relations(Lines(b),out g,out orig);
                var want=orig.Copy();
                foreach(var m in mods){OrderedMap<string,string> r;Relations(Lines(m),out g,out r);foreach(var kv in r.Items){string o;if(!orig.TryGetValue(kv.Key,out o)||o!=kv.Value)want[kv.Key]=kv.Value;}}
                foreach(var kv in want.Items){string v;got.TryGetValue(kv.Key,out v);if(v!=kv.Value)problems.Add("relation "+kv.Key.Replace('\u0001',' ')+": "+v+" != "+kv.Value);}
            }else{
                List<string> h;OrderedMap<string,List<string>> got;Ipl(Lines(dest),out h,out got);
                var gs=new Dictionary<string,HashSet<string>>();foreach(var kv in got.Items)gs[kv.Key]=new HashSet<string>(kv.Value.Select(Canon));
                foreach(var f in new[]{b}.Concat(mods)){
                    OrderedMap<string,List<string>> s;Ipl(Lines(f),out h,out s);
                    foreach(var kv in s.Items){HashSet<string> have;gs.TryGetValue(kv.Key,out have);int miss=kv.Value.Select(Canon).Distinct().Count(x=>have==null||!have.Contains(x));if(miss>0)problems.Add(kv.Key+": "+miss+" lines missing");}
                }
            }
            return problems;
        }
    }
}
