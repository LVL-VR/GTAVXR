using System;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Xml;
using System.Xml.Linq;
using System.Collections.Generic;

namespace GtavxrSuite {
    public class ModOperation {
        public string type {get;set;} public string source {get;set;} public string destination {get;set;}
        public string container {get;set;} public string family {get;set;} public string support {get;set;}
        public string detail {get;set;}
    }
    public class Inspection {
        public string schema="gtavxr-suite-inspection-v1";
        public string file; public string sha256; public string format; public string packageVersion;
        public List<string> warnings=new List<string>(); public List<ModOperation> operations=new List<ModOperation>();
        public List<string> members=new List<string>(); public bool executableCodeRun=false; public bool compatible=false; public long declaredExpandedBytes;
    }
    public static class Importer {
        // No size or count cap on a mod archive: the disk-space check (SafeFs.Budget) decides; members must still expand to
        // exactly their declared size.
        private const long MaxExpanded=Int64.MaxValue;
        private static string Virtual(string s) {
            if(s==null)throw new InvalidDataException("Missing virtual path");
            // OIV2.x uses one leading slash for the root INSIDE an archive. Never a host/UNC path.
            s=s.Replace('\\','/');if(s.StartsWith("//",StringComparison.Ordinal))throw new InvalidDataException("Virtual UNC rejected");
            return SafeFs.Relative(s.StartsWith("/",StringComparison.Ordinal)?s.Substring(1):s);
        }
        private static string Family(string path) {
            string ext=Path.GetExtension(path).ToLowerInvariant();
            if(new[]{".dll",".asi",".exe",".bat",".cmd",".ps1"}.Contains(ext))return "native-plugin-or-executable";
            if(new[]{".cs",".vb",".lua",".ysc"}.Contains(ext))return "script";
            if(new[]{".fxc",".fx",".hlsl"}.Contains(ext))return "shader";
            if(ext==".rpf")return "resource-container";
            if(new[]{".ytd",".ydr",".ydd",".ybn",".ymap",".ytyp",".yft"}.Contains(ext))return "resource";
            return "data";
        }
        public static Inspection Inspect(string path) {return Inspect(path,null);}
        public static Inspection Inspect(string path,string scratch) {
            SafeFs.NoLinks(path);var result=new Inspection {file=Path.GetFullPath(path),sha256=SafeFs.Hash(path)};
            using(var f=File.OpenRead(path)) {
                byte[] magic=new byte[8];int count=f.Read(magic,0,8);f.Position=0;
                if(count>=4 && magic[0]=='R'&&magic[1]=='a'&&magic[2]=='r'&&magic[3]=='!') {
                    result.format="RAR";RarInventory.TryInspect(path,result,scratch);return result;
                }
                if(count<4||magic[0]!='P'||magic[1]!='K') {result.format="unknown";result.warnings.Add("Unsupported container; never execute to discover behavior.");return result;}
                result.format="ZIP/OIV";
                using(var zip=new ZipArchive(f,ZipArchiveMode.Read,false)) InspectZip(zip,result,0,"",scratch);
            }
            result.warnings.Add("Inspection does not certify Legacy3889 resources, scripts, visuals or save compatibility.");return result;
        }
        private static void InspectZip(ZipArchive zip,Inspection result,int depth,string prefix,string scratch) {
            if(depth>2)throw new InvalidDataException("Archive nesting/member limit");
            long expanded=0;var names=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach(var e in zip.Entries) {
                string n=e.FullName.Replace('\\','/');bool directory=n.EndsWith("/",StringComparison.Ordinal);
                n=SafeFs.Relative(directory?n.TrimEnd('/'):n);
                if(!names.Add(n))throw new InvalidDataException("Case/duplicate archive entry");
                if(((uint)e.ExternalAttributes & 0xF0000000u)==0xA0000000u)throw new InvalidDataException("Archive symlink rejected");
                expanded=checked(expanded+e.Length);result.declaredExpandedBytes=checked(result.declaredExpandedBytes+e.Length);
                result.members.Add(prefix+n);
            }
            var assemblies=zip.Entries.Where(e=>e.FullName.Replace('\\','/').Equals("assembly.xml",StringComparison.OrdinalIgnoreCase)).ToArray();
            if(assemblies.Length>1)throw new InvalidDataException("Ambiguous assembly");
            if(assemblies.Length==1) {
                var e=assemblies[0];if(e.Length>4*1024*1024)throw new InvalidDataException("XML budget");
                using(var stream=e.Open()) {
                    var settings=new XmlReaderSettings {DtdProcessing=DtdProcessing.Prohibit,XmlResolver=null,MaxCharactersInDocument=4*1024*1024};
                    using(var reader=XmlReader.Create(stream,settings)) {
                        var doc=XDocument.Load(reader,LoadOptions.None);var root=doc.Root;
                        if(root==null||root.Name.LocalName!="package")throw new InvalidDataException("Not OIV package schema");
                        string version=(string)root.Attribute("version");
                        if(!new[]{"2.0","2.1","2.2"}.Contains(version))throw new InvalidDataException("Unrecognized OIV version");
                        result.packageVersion=version;
                        var content=root.Elements().SingleOrDefault(x=>x.Name.LocalName=="content");
                        if(content==null)throw new InvalidDataException("OIV has no content");
                        Walk(content,"",result,prefix,zip);
                    }
                }
            }
            // Inspect nested OIV manifests only; never execute or unpack a provider installer.
            foreach(var e in zip.Entries.Where(e=>e.FullName.EndsWith(".oiv",StringComparison.OrdinalIgnoreCase))) {
                if(e.Length>256L*1024*1024){
                    if(scratch==null){result.warnings.Add("Nested OIV requires an owned scratch directory: "+e.FullName);continue;}
                    SafeFs.Budget(scratch,checked(e.Length*2));Directory.CreateDirectory(SafeFs.Root(scratch));string temp=SafeFs.Under(scratch,"nested-"+Guid.NewGuid().ToString("N")+".oiv");
                    using(var s=e.Open())using(var d=new FileStream(temp,FileMode.CreateNew,FileAccess.Write,FileShare.None)){CopyBounded(s,d,e.Length);d.Flush(true);}
                    using(var f=File.OpenRead(temp))using(var nested=new ZipArchive(f,ZipArchiveMode.Read,false))InspectZip(nested,result,depth+1,prefix+e.FullName+"!/",scratch);
                    result.warnings.Add("Owned bounded spool retained: "+temp);continue;
                }
                using(var s=e.Open()) using(var mem=new MemoryStream()) {CopyBounded(s,mem,e.Length);mem.Position=0;using(var nested=new ZipArchive(mem,ZipArchiveMode.Read,false)) InspectZip(nested,result,depth+1,prefix+e.FullName+"!/",scratch);}
            }
        }
        public static void CopyBounded(Stream source,Stream dest,long exact) {
            long count=0;byte[] b=new byte[65536];int n;
            while((n=source.Read(b,0,b.Length))>0){count=checked(count+n);if(count>exact||count>MaxExpanded)throw new InvalidDataException("Actual expansion exceeds declared limit");dest.Write(b,0,n);}
            if(count!=exact)throw new InvalidDataException("Truncated archive member");
        }
        private static void Walk(XElement node,string container,Inspection r,string prefix,ZipArchive zip) {
            foreach(var n in node.Elements()) {
                string tag=n.Name.LocalName;
                if(tag=="archive") {
                    string archive=Virtual((string)n.Attribute("path"));
                    Walk(n,container.Length==0?archive:container+"::"+archive,r,prefix,zip);continue;
                }
                if(tag=="xml"||tag=="text"||tag=="pso") {
                    string path=Virtual((string)n.Attribute("path"));
                    r.operations.Add(new ModOperation {type=tag,container=container,destination=path,family="data-transform",support="NEEDS_VERSIONED_RECIPE",detail=n.ToString(SaveOptions.DisableFormatting)});
                    // Keep complete parent target/context, not a false empty RPF destination for nested XML add/remove.
                    continue;
                }
                if(tag=="add") {
                    string source="content/"+SafeFs.Relative((string)n.Attribute("source"));string destination=Virtual(n.Value.Trim());
                    var found=zip.Entries.Where(e=>e.FullName.Replace('\\','/').Equals(source,StringComparison.OrdinalIgnoreCase)).ToArray();
                    if(found.Length!=1)throw new InvalidDataException("OIV source missing/ambiguous: "+source);
                    string family=Family(destination);
                    r.operations.Add(new ModOperation {type="add",source=prefix+source,container=container,destination=destination,family=family,support="NEEDS_VERSIONED_RECIPE"});continue;
                }
                if(tag=="delete"||tag=="remove") {
                    r.operations.Add(new ModOperation {type=tag,container=container,destination=Virtual(n.Value.Trim()),family="delete",support="REQUIRES_OWNERSHIP_AND_BASELINE"});continue;
                }
                r.operations.Add(new ModOperation {type=tag,container=container,family="unknown",support="REJECTED_UNKNOWN_OPERATION",detail=n.ToString(SaveOptions.DisableFormatting)});
            }
        }
        public static object[] Conflicts(IEnumerable<Inspection> reports) {
            return reports.SelectMany(r=>r.operations.Select(o=>new {report=r.sha256,operation=o}))
                .Where(x=>!String.IsNullOrEmpty(x.operation.destination))
                .GroupBy(x=>(x.operation.container??"")+"::"+x.operation.destination,StringComparer.OrdinalIgnoreCase)
                .Where(g=>g.Select(x=>x.report).Distinct(StringComparer.OrdinalIgnoreCase).Count()>1)
                .Select(g=>(object)new {target=g.Key,kind="declared-target-overlap",resolution="REVIEW_REQUIRED_NOT_LAST_WINS",packages=g.Select(x=>x.report).Distinct().ToArray()}).ToArray();
        }
    }
}
