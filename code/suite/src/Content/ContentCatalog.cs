using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Web.Script.Serialization;

namespace GtavxrSuite.Content {
    // resources/embedded/content-catalog.json: the cards and Personalizar pages of the known content mods.
    public class CatRule { public List<string> sources {get;set;} public string kind {get;set;} public string target {get;set;} public string list {get;set;} public string key {get;set;} public string only {get;set;} public List<string> fields {get;set;} public bool block {get;set;} public bool invert {get;set;} }
    public class CatEffect {
        public List<string> categories {get;set;} public List<CatRule> rules {get;set;} public List<string> add_sources {get;set;}
        public Dictionary<string,string> settings_xml {get;set;} public bool? scenarios_full {get;set;} public bool? source_density {get;set;} public bool? source_streaming {get;set;}
        public List<string> drop_packs {get;set;}
    }
    public class CatValue { public string id {get;set;} public Dictionary<string,string> title {get;set;} public CatEffect effect {get;set;} }
    public class CatGameconfig { public string tag {get;set;} public double at_min {get;set;} public double at_max {get;set;} }
    public class CatRequire { public string option {get;set;} public List<string> values {get;set;} public string @else {get;set;} public Dictionary<string,string> reason {get;set;} }
    public class CatOption {
        public string id {get;set;} public string type {get;set;} public string @default {get;set;}
        public Dictionary<string,string> title {get;set;} public Dictionary<string,string> hint {get;set;} public Dictionary<string,string> group {get;set;}
        public bool vr_cost {get;set;} public CatEffect on {get;set;} public CatEffect off {get;set;} public List<CatValue> values {get;set;}
        public int min {get;set;} public int max {get;set;} public int step {get;set;} public string unit {get;set;} public List<CatGameconfig> gameconfig {get;set;}
        public List<CatOption> children {get;set;} public List<CatRequire> requires {get;set;}
        // script mods: the key this option writes in the mod's own .ini (switch: ini_on / ini_off, default "1" / "0"; choice / list:
        // the value id; a value id "mod" writes nothing, the mod's own value stays)
        public string ini {get;set;} public string ini_on {get;set;} public string ini_off {get;set;}
        public string values_from {get;set;}    // the catalog's value_lists entry appended to values (type "list": radio stations)
    }
    public class CatMatch { public string oiv_name {get;set;} public string pack {get;set;} public string script_dll {get;set;} }
    public class CatMod {
        public string id {get;set;} public string name {get;set;} public string author {get;set;} public string category {get;set;} public string option_of {get;set;}
        public CatMatch match {get;set;} public Dictionary<string,string> about {get;set;} public Dictionary<string,string> known_issue {get;set;}
        public List<string> requires_mods {get;set;} public bool density_source {get;set;} public List<CatOption> options {get;set;}
        public string ini_file {get;set;} public string ini_section {get;set;}   // script mods: its settings file (path inside scripts/) and section
    }
    public class Catalog { public string schema {get;set;} public string notes {get;set;} public List<CatMod> mods {get;set;} public Dictionary<string,List<CatValue>> value_lists {get;set;} }

    public sealed class OptionState { public string Key,Value;public CatOption Option;public string Blocked;public bool Visible; }
    // What a profile's options make of its content sources: the build options + what the profile stages besides.
    public sealed class ContentPlan {
        public readonly List<string> Sources=new List<string>();           // mod ids in merge order
        public readonly BuildOptions Options=new BuildOptions();
        public readonly Dictionary<string,string> SettingsXml=new Dictionary<string,string>();
        public readonly List<string> Skipped=new List<string>();           // mod ids left out (a required mod is off), with why
        public string Canon;                                               // what the build depends on, for the cache key
    }

    public static class ContentCatalog {
        static Catalog cat;static string hash;
        public static Catalog Get(){
            if(cat==null){
                using(var s=Assembly.GetExecutingAssembly().GetManifestResourceStream("GtavxrSuite.content-catalog.json"))using(var r=new StreamReader(s)){
                    string text=r.ReadToEnd();
                    cat=new JavaScriptSerializer{MaxJsonLength=4*1024*1024}.Deserialize<Catalog>(text);
                    using(var h=System.Security.Cryptography.SHA256.Create())hash=BitConverter.ToString(h.ComputeHash(System.Text.Encoding.UTF8.GetBytes(text))).Replace("-","");
                }
                if(cat.schema!="gtavxr-suite2-content-catalog-v1")throw new InvalidDataException("content catalog schema");
                foreach(var m in cat.mods)Lists(m.options);
            }
            return cat;
        }
        static void Lists(List<CatOption> opts){
            foreach(var o in opts??new List<CatOption>()){
                if(o.values_from!=null){
                    List<CatValue> l;if(cat.value_lists==null||!cat.value_lists.TryGetValue(o.values_from,out l))throw new InvalidDataException("content catalog list "+o.values_from);
                    o.values=(o.values??new List<CatValue>()).Concat(l).ToList();
                }
                Lists(o.children);
            }
        }
        public static string Hash{get{Get();return hash;}}
        public static CatMod Mod(string id){return id==null?null:Get().mods.FirstOrDefault(m=>m.id==id);}
        // 2.4.2: catalog mods that list "Enable MP Maps" as a requirement and never load the Online map data themselves (Open World
        // Interiors: its interiors and props come from Online DLC). A session with one starts with "Online maps in Story" on
        // (mods_mp_maps166=1). Kept here, not in content-catalog.json: the catalog hash is part of every content build key.
        static readonly string[] OnlineMapsCatalogs={"owi"};
        public static List<string> NeedsOnlineMaps(IEnumerable<GtavxrSuite.ModManifest> mods){
            return (mods??Enumerable.Empty<GtavxrSuite.ModManifest>()).Where(m=>m!=null&&m.kind=="script"&&m.source!=null&&OnlineMapsCatalogs.Contains(m.source.catalog))
                .Select(m=>{var c=Mod(m.source.catalog);return c!=null?c.name:m.name;}).Distinct().ToList();
        }
        public static CatMod MatchOiv(string name){return Get().mods.FirstOrDefault(m=>m.match!=null&&m.match.oiv_name!=null&&System.Text.RegularExpressions.Regex.IsMatch((name??"").Trim(),m.match.oiv_name,System.Text.RegularExpressions.RegexOptions.IgnoreCase));}
        public static CatMod MatchPack(string pack){return Get().mods.FirstOrDefault(m=>m.match!=null&&m.match.pack!=null&&System.Text.RegularExpressions.Regex.IsMatch(pack??"",m.match.pack,System.Text.RegularExpressions.RegexOptions.IgnoreCase));}
        public static CatMod MatchScript(IEnumerable<string> dlls){return Get().mods.FirstOrDefault(m=>m.match!=null&&m.match.script_dll!=null&&dlls.Any(d=>System.Text.RegularExpressions.Regex.IsMatch(d??"",m.match.script_dll,System.Text.RegularExpressions.RegexOptions.IgnoreCase)));}
        public static string T(Dictionary<string,string> t,string lang){string v;return t==null?"":t.TryGetValue(lang,out v)||t.TryGetValue("en",out v)?v:t.Values.FirstOrDefault()??"";}

        // Every option of a mod with its key ("wov.traffic.deformation"), value for this profile and why it is blocked.
        // known: the states of the mods before it (a requirement can name another mod's option).
        public static List<OptionState> States(CatMod m,Func<string,string,string> value,string lang,List<OptionState> known=null){
            var all=new List<OptionState>();
            Walk(m.id,m.options??new List<CatOption>(),value,all,true,lang,known??new List<OptionState>());
            return all;
        }
        static void Walk(string prefix,List<CatOption> opts,Func<string,string,string> value,List<OptionState> all,bool visible,string lang,List<OptionState> known){
            foreach(var o in opts){
                string key=prefix+"."+o.id;
                var st=new OptionState{Key=key,Option=o,Value=value(key,o.@default),Visible=visible};
                foreach(var rq in o.requires??new List<CatRequire>()){
                    string have=Resolved(all,rq.option)??Resolved(known,rq.option)??value(rq.option,null);
                    if(have!=null&&rq.values!=null&&!rq.values.Contains(have)){st.Blocked=T(rq.reason,lang);st.Value=rq.@else??(o.type=="switch"?"off":null);break;}   // null: no value applies
                }
                all.Add(st);
                if(o.children!=null)Walk(key,o.children,value,all,visible&&(o.type!="switch"||st.Value=="on"),lang,known);
            }
        }
        static string Resolved(List<OptionState> all,string key){var s=all.FirstOrDefault(x=>x.Key==key);return s==null?null:s.Value;}

        // sources: the profile's content-source mods in its order (id, catalog id). find: library lookup of an option source.
        public static ContentPlan Plan(List<KeyValuePair<string,string>> sources,Func<string,string,string> value,Func<string,string> findOption){
            var plan=new ContentPlan();var o=plan.Options;
            var byCat=new Dictionary<string,string>();
            foreach(var s in sources)if(s.Value!=null&&!byCat.ContainsKey(s.Value))byCat[s.Value]=s.Key;
            var effects=new List<CatEffect>();var gc=new SortedDictionary<string,string>(StringComparer.Ordinal);var known=new List<OptionState>();
            foreach(var s in sources){
                var m=Mod(s.Value);
                if(m!=null&&m.requires_mods!=null&&m.requires_mods.Any(r=>!byCat.ContainsKey(r))){plan.Skipped.Add(s.Key);continue;}
                plan.Sources.Add(s.Key);
                if(m==null)continue;
                if(m.density_source){o.DensitySource=s.Key;o.SourceDensity=true;o.SourceStreaming=false;}
                var states=States(m,value,"en",known);known.AddRange(states);
                foreach(var st in states){
                    if(!st.Visible)continue;
                    if(st.Value==null)continue;  // blocked, no fallback value
                    var op=st.Option;
                    if(op.type=="switch"){var e=st.Value=="on"?op.on:op.off;if(e!=null)effects.Add(e);}
                    else if(op.type=="choice"){var v=(op.values??new List<CatValue>()).FirstOrDefault(x=>x.id==st.Value)??(op.values??new List<CatValue>()).FirstOrDefault(x=>x.id==op.@default);if(v!=null&&v.effect!=null)effects.Add(v.effect);}
                    else if(op.type=="slider"){
                        int n;if(!Int32.TryParse(st.Value,out n))Int32.TryParse(op.@default,out n);
                        n=Math.Max(op.min,Math.Min(op.max,n));
                        foreach(var g in op.gameconfig??new List<CatGameconfig>()){
                            double v=op.max==op.min?g.at_max:g.at_min+(n-op.min)*(g.at_max-g.at_min)/(op.max-op.min);
                            gc[g.tag]=Math.Round(v).ToString(System.Globalization.CultureInfo.InvariantCulture);
                        }
                    }
                }
            }
            // option sources (No Umbrellas) go right after the mod they belong to
            foreach(var e in effects)foreach(var a in e.add_sources??new List<string>()){
                var parent=Mod(a);string id=findOption(a);
                if(id==null||plan.Sources.Contains(id))continue;
                string pid=parent!=null&&parent.option_of!=null&&byCat.ContainsKey(parent.option_of)?byCat[parent.option_of]:null;
                int at=pid==null?plan.Sources.Count:plan.Sources.IndexOf(pid)+1;plan.Sources.Insert(at,id);
            }
            foreach(var e in effects){
                foreach(var c in e.categories??new List<string>())o.Off.Add(c);
                if(e.scenarios_full==true)o.ScenariosFull=true;
                if(e.source_density!=null)o.SourceDensity=e.source_density.Value;
                if(e.source_streaming!=null)o.SourceStreaming=e.source_streaming.Value;
                foreach(var p in e.drop_packs??new List<string>())o.DropPacks.Add(p);
                foreach(var kv in e.settings_xml??new Dictionary<string,string>())plan.SettingsXml[kv.Key]=kv.Value;
                foreach(var r in e.rules??new List<CatRule>())
                    foreach(var src in r.sources??new List<string>()){
                        string id;if(!byCat.TryGetValue(src,out id)||!plan.Sources.Contains(id))continue;
                        o.Rules.Add(new SourceRule{Source=id,Kind=r.kind,Target=r.target,List=r.list,Key=r.key,Only=r.only,Fields=r.fields==null?null:r.fields.ToArray(),Block=r.block,Invert=r.invert});
                    }
            }
            if(o.SourceDensity)foreach(var kv in gc)o.Gameconfig[kv.Key]=kv.Value;
            plan.Canon=new JavaScriptSerializer().Serialize(new{catalog=Hash,sources=plan.Sources,off=o.Off.OrderBy(x=>x).ToList(),full=o.ScenariosFull,ds=o.DensitySource,sd=o.SourceDensity,ss=o.SourceStreaming,
                gc=o.Gameconfig.OrderBy(x=>x.Key).Select(x=>x.Key+"="+x.Value).ToList(),packs=o.DropPacks.OrderBy(x=>x).ToList(),
                rules=o.Rules.Select(r=>String.Join("|",r.Source,r.Kind,r.Target,r.List,r.Key,r.Only,r.Fields==null?"":String.Join(",",r.Fields),r.Block,r.Invert)).ToList()});
            return plan;
        }
    }
}
