using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace GtavxrSuite.Content {
    public class ContentSpecSource { public string id {get;set;} public string name {get;set;} public string kind {get;set;} public string root {get;set;} public string pack {get;set;} public string file {get;set;} }
    public class ContentSpec {
        public string game_dir {get;set;} public string id {get;set;} public List<ContentSpecSource> sources {get;set;}
        public List<string> off {get;set;} public bool scenarios_full {get;set;}
        public string density_source {get;set;} public bool source_density {get;set;} public bool source_streaming {get;set;}
        public List<ContentSpecRule> rules {get;set;} public Dictionary<string,string> gameconfig {get;set;} public List<string> drop_packs {get;set;} public bool detail {get;set;}
    }
    public class ContentSpecRule { public string source {get;set;} public string kind {get;set;} public string target {get;set;} public string list {get;set;} public string key {get;set;} public List<string> fields {get;set;} public bool block {get;set;} public bool invert {get;set;} public string only {get;set;} }
    public class OptionCase { public string name {get;set;} public Dictionary<string,string> options {get;set;} public List<string> pso_xml {get;set;} }
    public class OptionsSpec { public string game_dir {get;set;} public List<ContentSpecSource> sources {get;set;} public List<string> catalogs {get;set;} public List<OptionCase> cases {get;set;} }
    // Headless: --content-build SPEC.json FRESH_OUT RESULT.json. Reads the game folder only; never writes it, never starts GTA.
    // --content-options SPEC.json FRESH_OUT RESULT.json: every case = the catalog's plan for those option values, built.
    public static class ContentCli {
        public static object RunOptions(string specPath,string outDir){
            var spec=Json.Read<OptionsSpec>(specPath);Directory.CreateDirectory(outDir);
            var srcs=new Dictionary<string,ModSource>();
            foreach(var s in spec.sources)srcs[s.id]=s.kind=="pack"?ModSource.FromPack(s.id,s.name,s.pack,s.file):ModSource.FromOiv(s.id,s.name,s.root);
            var pairs=spec.sources.Select((s,i)=>new KeyValuePair<string,string>(s.id,spec.catalogs[i])).ToList();
            var results=new List<object>();
            foreach(var c in spec.cases){
                var opts=c.options??new Dictionary<string,string>();
                // option sources (No Umbrellas) are not cards: the plan adds them when an option asks for them
                var plan=ContentCatalog.Plan(pairs.Where(p=>{var m=ContentCatalog.Mod(p.Value);return m==null||m.option_of==null;}).ToList(),(k,d)=>{string v;return opts.TryGetValue(k,out v)?v:d;},
                    cat=>{var p=pairs.FirstOrDefault(x=>x.Value==cat);return p.Key;});
                if(c.pso_xml!=null)plan.Options.PsoXmlRoots=new HashSet<string>(c.pso_xml,StringComparer.Ordinal);
                var t0=DateTime.UtcNow;string dir=Path.Combine(outDir,c.name);
                try{
                    var r=ContentBuilder.Build(spec.game_dir,plan.Sources.Select(id=>srcs[id]).ToList(),plan.Options,"suite",dir,null);
                    var rules=((List<Dictionary<string,object>>)r.Report["rules"]).Where(x=>x.ContainsKey("rule")).ToList();
                    results.Add(new{name=c.name,status="BUILT",sources=plan.Sources,skipped=plan.Skipped,settings_xml=plan.SettingsXml,files=r.Files.Count,mounts=r.Mounts.Count,streams=r.Streams.Count,
                        rules=rules,dead_rules=rules.Count(x=>Convert.ToInt32(x["hits"])==0),gameconfig=r.Report["gameconfig_swapped"],off=r.Report["off"],
                        not_merged=r.Report.ContainsKey("not_class_a")?r.Report["not_class_a"]:null,
                        seconds=(DateTime.UtcNow-t0).TotalSeconds,list=r.Files.Select(f=>new{mount=f.Mount,relative=f.Relative,source=f.Source}).ToList(),manifest=Path.Combine(dir,"profile","manifest.txt")});
                }catch(Exception e){results.Add(new{name=c.name,status="FAILED",error=e.GetType().Name+": "+e.Message,stack=e.StackTrace});}
            }
            return new{status="OPTIONS_RUN",cases=results,gta_launched=false};
        }
        public static object Run(string specPath,string outDir){
            var spec=Json.Read<ContentSpec>(specPath);
            var sources=new List<ModSource>();
            foreach(var s in spec.sources){
                if(s.kind=="oiv")sources.Add(ModSource.FromOiv(s.id,s.name,s.root));
                else if(s.kind=="pack")sources.Add(ModSource.FromPack(s.id,s.name,s.pack,s.file));
                else throw new InvalidDataException("source kind "+s.kind);
            }
            var opt=new BuildOptions{Off=new HashSet<string>(spec.off??new List<string>()),ScenariosFull=spec.scenarios_full,
                DensitySource=spec.density_source,SourceDensity=spec.source_density,SourceStreaming=spec.source_streaming};
            foreach(var x in spec.rules??new List<ContentSpecRule>())opt.Rules.Add(new SourceRule{Source=x.source,Kind=x.kind,Target=x.target,List=x.list,Key=x.key,Fields=x.fields==null?null:x.fields.ToArray(),Block=x.block,Invert=x.invert,Only=x.only});
            foreach(var kv in spec.gameconfig??new Dictionary<string,string>())opt.Gameconfig[kv.Key]=kv.Value;
            foreach(var p in spec.drop_packs??new List<string>())opt.DropPacks.Add(p);
            opt.Detail=spec.detail;
            var t0=DateTime.UtcNow;var steps=new List<string>();
            var r=ContentBuilder.Build(spec.game_dir,sources,opt,spec.id,outDir,(a,b,c)=>{if(b==0||b==c)steps.Add(a+" "+b+"/"+c);});
            return new{status="CONTENT_BUILT",id=r.Id,folder=r.Folder,mounts=r.Mounts.Count,files=r.Files.Count,streams=r.Streams.Count,
                seconds=(DateTime.UtcNow-t0).TotalSeconds,steps=steps,
                list=r.Files.Select(f=>new{mount=f.Mount,relative=f.Relative,bytes=f.Bytes,rscver=f.RscVer,what=f.What,category=f.Category,source=f.Source}).ToList(),
                gta_launched=false};
        }
    }
}
