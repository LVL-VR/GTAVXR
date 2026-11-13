using System;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Web.Script.Serialization;

namespace GtavxrSuite {
    public static class KnownRecipes {
        public static Recipe[] All() {
            var assembly=Assembly.GetExecutingAssembly();var names=assembly.GetManifestResourceNames().Where(n=>n.StartsWith("GtavxrSuite.Recipe.",StringComparison.Ordinal)).OrderBy(n=>n).ToArray();
            return names.Select(n=>{using(var s=assembly.GetManifestResourceStream(n))using(var r=new StreamReader(s)){string text=r.ReadToEnd();StrictJson.Validate(text);var recipe=new JavaScriptSerializer{MaxJsonLength=16*1024*1024}.Deserialize<Recipe>(text);Recipes.Validate(recipe);return recipe;}}).ToArray();
        }
        public static object Prepare(Store s,string archive) {
            string hash=SafeFs.Hash(archive);var found=All().Where(r=>r.inputSha256.Equals(hash,StringComparison.OrdinalIgnoreCase)).ToArray();
            if(found.Length!=1)throw new InvalidDataException("No single reviewed built-in recipe matches these exact bytes; inspect first, do not infer from filename");
            string output=SafeFs.Under(s.Root,"prepared/"+found[0].id+"-"+Guid.NewGuid().ToString("N"));var receipt=Recipes.Prepare(archive,found[0],output);Json.New(SafeFs.Under(output,"recipe.json"),found[0]);var audit=DataAudit.Run(output);Json.New(SafeFs.Under(output,"data-audit.json"),audit);
            return new{status="PREPARED_NOT_NATIVE_ADMITTED",output=output,recipe=found[0].id,files=found[0].actions.Count,audit_status=audit.status,syntax_rejections=audit.rejected,preparation=receipt,active=false};
        }
    }
}
