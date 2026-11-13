using System;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Collections.Generic;
using System.Xml;
using System.Xml.Linq;

namespace GtavxrSuite {
    public class Recipe {
        public string schema {get;set;} public string id {get;set;} public int version {get;set;}
        public string inputSha256 {get;set;} public string runtime {get;set;} public string assemblyPrefix {get;set;}
        public string[] dependencies {get;set;} public string[] exclusions {get;set;}
        public List<RecipeAction> actions {get;set;}
    }
    public class RecipeAction {
        public string kind {get;set;} public string source {get;set;} public string sha256 {get;set;} public long bytes {get;set;}
        public string container {get;set;} public string destination {get;set;} public string admission {get;set;}
    }
    public static class Recipes {
        public static Recipe DraftCopyRecipe(string archive,string id,string assemblyPrefix,int expected,string output) {
            Ids.Check(id);output=SafeFs.Root(output);if(Directory.Exists(output))throw new InvalidDataException("Fresh draft directory required");
            var inventory=Importer.Inspect(archive,SafeFs.Under(output,"scratch"));
            var selected=inventory.operations.Where(o=>o.source!=null&&o.source.StartsWith(assemblyPrefix,StringComparison.OrdinalIgnoreCase)).ToArray();
            if(selected.Length!=expected||selected.Length==0||selected.Any(o=>o.type!="add"))throw new InvalidDataException("Observed copy-operation contract differs from draft expectation");
            Directory.CreateDirectory(output);var actions=new List<RecipeAction>();var cache=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);
            foreach(var o in selected){string file=SafeFs.Under(output,"selected/"+actions.Count.ToString("D5")+".bin");Extract(archive,o.source,file,output,cache);actions.Add(new RecipeAction{kind="copy",source=o.source,container=o.container,destination=o.destination,sha256=SafeFs.Hash(file),bytes=new FileInfo(file).Length,admission="PREPARE_ONLY"});}
            var r=new Recipe{schema="gtavxr-suite-recipe-v1",id=id,version=1,inputSha256=inventory.sha256,runtime="legacy3889",assemblyPrefix=assemblyPrefix,dependencies=new string[0],exclusions=new string[0],actions=actions};
            Validate(r);Json.New(SafeFs.Under(output,"recipe.json"),r);Json.New(SafeFs.Under(output,"draft.json"),new{status="DRAFT_FOR_CONSUMER_REVIEW",selected_operations=selected.Length,other_operations=inventory.operations.Count-selected.Length,input_sha256=inventory.sha256,active=false,compatible=false});return r;
        }
        public static Recipe[] Order(Recipe[] recipes) {
            if(recipes==null||recipes.Length>1000)throw new InvalidDataException("Recipe count");
            var map=new Dictionary<string,Recipe>(StringComparer.OrdinalIgnoreCase);
            foreach(var r in recipes){Validate(r);if(map.ContainsKey(r.id))throw new InvalidDataException("Duplicate recipe");map.Add(r.id,r);}
            var sorted=new List<Recipe>();var active=new HashSet<string>(StringComparer.OrdinalIgnoreCase);var done=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach(var r in recipes)Visit(r,map,active,done,sorted);return sorted.ToArray();
        }
        private static void Visit(Recipe r,Dictionary<string,Recipe> map,HashSet<string> active,HashSet<string> done,List<Recipe> sorted) {
            if(done.Contains(r.id))return;if(!active.Add(r.id))throw new InvalidDataException("Dependency cycle");
            foreach(string dep in r.dependencies??new string[0]) {if(!map.ContainsKey(dep))throw new InvalidDataException("Missing dependency: "+dep);Visit(map[dep],map,active,done,sorted);}
            foreach(string excluded in r.exclusions??new string[0]) if(map.ContainsKey(excluded))throw new InvalidDataException("Explicit recipe conflict: "+excluded);
            active.Remove(r.id);done.Add(r.id);sorted.Add(r);
        }
        public static void Validate(Recipe r) {
            if(r==null||r.schema!="gtavxr-suite-recipe-v1"||r.version<1||r.runtime!="legacy3889"||!SafeFs.IsHash(r.inputSha256)||r.actions==null||r.actions.Count==0||r.actions.Count>20000)throw new InvalidDataException("Invalid recipe");
            Ids.Check(r.id);var seen=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach(var a in r.actions) {
                string dest=SafeFs.Relative(a.destination);if(a.container!=null&&a.container.Length>0)foreach(var c in a.container.Split(new[]{"::"},StringSplitOptions.None))SafeFs.Relative(c);
                if(!seen.Add((a.container??"")+"::"+dest))throw new InvalidDataException("Duplicate recipe target requires explicit merge");
                if(a.kind!="copy"||!SafeFs.IsHash(a.sha256)||a.bytes<0||a.bytes>4L*1024*1024*1024||String.IsNullOrEmpty(a.source)||a.source.Split(new[]{"!/"},StringSplitOptions.None).Length>3)throw new InvalidDataException("Unsupported action");
                foreach(var path in a.source.Split(new[]{"!/"},StringSplitOptions.None))SafeFs.Relative(path);
                if(a.admission!="PREPARE_ONLY")throw new InvalidDataException("No native admission may be inferred from a recipe");
            }
        }
        public static object Prepare(string archive,Recipe recipe,string output) {
            Validate(recipe);SafeFs.NoLinks(archive);
            if(!String.Equals(SafeFs.Hash(archive),recipe.inputSha256,StringComparison.OrdinalIgnoreCase))throw new InvalidDataException("Recipe package hash mismatch");
            output=SafeFs.Root(output);if(Directory.Exists(output)||File.Exists(output))throw new InvalidDataException("Fresh prepared generation required");
            SafeFs.Budget(output,checked(recipe.actions.Sum(a=>a.bytes)*2));Directory.CreateDirectory(output);
            var rows=new List<object>();var nestedCache=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);
            for(int i=0;i<recipe.actions.Count;i++) {
                var a=recipe.actions[i];string p=SafeFs.Under(output,"content/"+i.ToString("D5")+".bin");
                Extract(archive,a.source,p,output,nestedCache);SafeFs.AssertFile(p,a.sha256,a.bytes);
                rows.Add(new{file=p,sha256=a.sha256,bytes=a.bytes,container=a.container,destination=a.destination,admission="NOT_NATIVE_QUALIFIED"});
            }
            var result=new{schema="gtavxr-suite-prepared-v1",recipe=recipe.id,version=recipe.version,input_sha256=recipe.inputSha256,files=rows,active=false,blockers=new[]{"Exact Legacy3889 consumer/schema/baseline admission","Qualified native mount or selective RPF writer and readback","Combined Story/VR wearer validation"}};
            Json.New(SafeFs.Under(output,"prepared.json"),result);return result;
        }
        private static void Extract(string archive,string source,string output,string scratch,Dictionary<string,string> cache) {
            string[] parts=source.Split(new[]{"!/"},StringSplitOptions.None);
            using(var f=File.OpenRead(archive))using(var z=new ZipArchive(f,ZipArchiveMode.Read,false))ExtractAt(z,parts,0,output,scratch,cache,"");
        }
        private static void ExtractAt(ZipArchive zip,string[] parts,int index,string output,string scratch,Dictionary<string,string> cache,string prefix) {
            string requested=SafeFs.Relative(parts[index]);
            var found=zip.Entries.Where(e=>e.FullName.Replace('\\','/').Equals(requested,StringComparison.OrdinalIgnoreCase)).ToArray();
            if(found.Length!=1)throw new InvalidDataException("Recipe source missing/ambiguous");var entry=found[0];
            if(index==parts.Length-1) {Directory.CreateDirectory(Path.GetDirectoryName(output));using(var s=entry.Open())using(var d=new FileStream(output,FileMode.CreateNew,FileAccess.Write,FileShare.None)){Importer.CopyBounded(s,d,entry.Length);d.Flush(true);}return;}
            if(entry.Length>4L*1024*1024*1024)throw new InvalidDataException("Nested recipe input limit");
            string key=prefix+requested+"!/",nested;
            if(!cache.TryGetValue(key,out nested)) {
                SafeFs.Budget(scratch,entry.Length*2);nested=SafeFs.Under(scratch,"nested-"+Guid.NewGuid().ToString("N")+".zip");
                using(var s=entry.Open())using(var d=new FileStream(nested,FileMode.CreateNew,FileAccess.Write,FileShare.None)){Importer.CopyBounded(s,d,entry.Length);d.Flush(true);}cache.Add(key,nested);
            }
            using(var f=File.OpenRead(nested))using(var z=new ZipArchive(f,ZipArchiveMode.Read,false))ExtractAt(z,parts,index+1,output,scratch,cache,key);
        }
        // Three-way, record/key-aware merge. No schema-less last-file-wins and no runtime admission.
        public static XDocument MergeRecords(string baseline,string incoming,string installed,string keyElement) {
            Ids.Check(keyElement);var b=Xml(baseline);var m=Xml(incoming);var c=Xml(installed);
            if(b.Root.Name!=m.Root.Name||b.Root.Name!=c.Root.Name)throw new InvalidDataException("XML root mismatch");
            var bm=Records(b,keyElement);var mm=Records(m,keyElement);var cm=Records(c,keyElement);
            foreach(string key in bm.Keys.Union(mm.Keys).ToArray()) {
                XElement bv=bm.ContainsKey(key)?bm[key]:null,mv=mm.ContainsKey(key)?mm[key]:null,cv=cm.ContainsKey(key)?cm[key]:null;
                if(XNode.DeepEquals(bv,mv))continue;
                if(XNode.DeepEquals(cv,mv))continue;
                if(bv==null||mv==null||cv==null) {
                    if(!XNode.DeepEquals(cv,bv))throw new InvalidDataException("Record conflict: "+key);
                    if(cv!=null)cv.Remove();if(mv!=null)c.Root.Add(new XElement(mv));continue;
                }
                if(bv.HasAttributes||mv.HasAttributes||cv.HasAttributes)throw new InvalidDataException("Attributed record needs explicit schema");
                var fields=bv.Elements().Select(e=>e.Name).Union(mv.Elements().Select(e=>e.Name)).ToArray();
                foreach(var field in fields) {
                    if(bv.Elements(field).Count()>1||mv.Elements(field).Count()>1||cv.Elements(field).Count()>1)throw new InvalidDataException("Repeated field requires typed merge");
                    var bf=bv.Element(field);var mf=mv.Element(field);var cf=cv.Element(field);
                    if(XNode.DeepEquals(bf,mf)||XNode.DeepEquals(cf,mf))continue;
                    if(!XNode.DeepEquals(cf,bf))throw new InvalidDataException("Field conflict: "+key+"/"+field);
                    if(cf!=null){if(mf!=null)cf.ReplaceWith(new XElement(mf));else cf.Remove();}else if(mf!=null)cv.Add(new XElement(mf));
                }
            }
            return c;
        }
        private static XDocument Xml(string text) {using(var s=new StringReader(text))using(var r=XmlReader.Create(s,new XmlReaderSettings{DtdProcessing=DtdProcessing.Prohibit,XmlResolver=null,MaxCharactersInDocument=32*1024*1024}))return XDocument.Load(r);}
        private static Dictionary<string,XElement> Records(XDocument d,string key) {
            if(d.Root==null||d.Root.HasAttributes)throw new InvalidDataException("Unsupported record root");var records=new Dictionary<string,XElement>(StringComparer.OrdinalIgnoreCase);
            foreach(var e in d.Root.Elements()){var k=e.Elements(key).ToArray();if(k.Length!=1||String.IsNullOrWhiteSpace(k[0].Value)||records.ContainsKey(k[0].Value))throw new InvalidDataException("Missing/duplicate record identity");records.Add(k[0].Value,e);}return records;
        }
    }
}
