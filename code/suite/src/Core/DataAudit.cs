using System;
using System.IO;
using System.Linq;
using System.Collections.Generic;
using System.Xml;
using System.Text;

namespace GtavxrSuite {
    public class DataAuditRow {
        public string target {get;set;}public string sha256 {get;set;}public string check {get;set;}public string detail {get;set;}
        public string xmlRoot {get;set;}public uint resourceVersion {get;set;}public bool syntaxAccepted {get;set;}
    }
    public class DataAuditReport {
        public string schema="gtavxr-suite-data-audit-v1";public string status;public string recipe;public int rejected;
        public bool native_admitted=false;public bool source_modified=false;public DataAuditRow[] files;
    }
    public static class DataAudit {
        public static DataAuditReport Run(string generation) {
            generation=SafeFs.Root(generation);var recipe=Json.Read<Recipe>(SafeFs.Under(generation,"recipe.json"));Recipes.Validate(recipe);var rows=new List<DataAuditRow>();
            for(int i=0;i<recipe.actions.Count;i++) {
                var action=recipe.actions[i];string path=SafeFs.Under(generation,"content/"+i.ToString("D5")+".bin");SafeFs.AssertFile(path,action.sha256,action.bytes);
                var row=new DataAuditRow{target=(action.container??"")+"::"+action.destination,sha256=action.sha256,check="UNKNOWN_FORMAT_NOT_ADMITTED",syntaxAccepted=false};string ext=Path.GetExtension(action.destination).ToLowerInvariant();
                if(ext==".xml") {
                    row.check="XML_SYNTAX_ONLY";
                    try {
                        if(action.bytes>16*1024*1024)throw new InvalidDataException("XML audit bound exceeded");
                        var settings=new XmlReaderSettings{DtdProcessing=DtdProcessing.Prohibit,XmlResolver=null,MaxCharactersInDocument=16*1024*1024,MaxCharactersFromEntities=0};
                        using(var reader=XmlReader.Create(path,settings)){var doc=new XmlDocument{XmlResolver=null};doc.Load(reader);if(doc.DocumentElement==null)throw new InvalidDataException("XML root missing");row.xmlRoot=doc.DocumentElement.Name;}
                        row.syntaxAccepted=true;row.detail="Well-formed XML; no native schema or consumer compatibility inferred";
                    } catch(XmlException e){row.detail=e.Message;}catch(InvalidDataException e){row.detail=e.Message;}
                } else if(ext==".ytd") {
                    row.check="RSC7_HEADER_ONLY";
                    using(var file=File.OpenRead(path))using(var reader=new BinaryReader(file)){
                        if(file.Length<16)row.detail="Truncated resource header";
                        else {string magic=Encoding.ASCII.GetString(reader.ReadBytes(4));row.resourceVersion=reader.ReadUInt32();row.syntaxAccepted=magic=="RSC7"&&row.resourceVersion==13;row.detail=row.syntaxAccepted?"Observed RSC7/v13 header; interior layout, decompression, references and GPU consumer NOT validated":"Unsupported resource header";}
                    }
                } else if(ext==".dat") {row.check="OPAQUE_DATA_NOT_ADMITTED";row.detail="Requires its actual native parser/field contract; no extension-based certification";}
                rows.Add(row);
            }
            int rejected=rows.Count(r=>(r.check=="XML_SYNTAX_ONLY"||r.check=="RSC7_HEADER_ONLY")&&!r.syntaxAccepted);
            return new DataAuditReport{status=rejected>0?"SYNTAX_REVIEW_REQUIRED":"SYNTAX_CHECKS_ONLY_CONSUMER_PENDING",recipe=recipe.id,rejected=rejected,files=rows.ToArray()};
        }
    }
}
