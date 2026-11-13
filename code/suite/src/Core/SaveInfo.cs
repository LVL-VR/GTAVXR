using System;
using System.IO;
using System.Linq;
using System.Text;
using System.Globalization;
using System.Text.RegularExpressions;

namespace GtavxrSuite {
    // Reads the title GTA V writes at the start of each story save:
    // "(Autoguardado) Cambios (22.5%) - 10/01/26 05:03:45" (UTF-16 from byte 4).
    public class SaveInfo {
        public string mission {get;set;}
        public double percent {get;set;}
        public bool autosave {get;set;}
        public DateTime written_utc {get;set;}
        public string file {get;set;}

        public static SaveInfo Read(string path){
            try{
                var fi=new FileInfo(path);if(!fi.Exists||fi.Length<0x210||fi.Length>8*1024*1024)return null;
                byte[] head=new byte[0x204];using(var s=File.OpenRead(path)){if(s.Read(head,0,head.Length)!=head.Length)return null;}
                string text=Encoding.Unicode.GetString(head,4,0x200);int nul=text.IndexOf('\0');if(nul>=0)text=text.Substring(0,nul);
                var m=Regex.Match(text,@"^\s*(\((?<auto>[^)]*)\)\s*)?(?<mission>.+?)\s*\((?<pct>[0-9]+(?:[.,][0-9]+)?)%\)");
                if(!m.Success)return null;
                double pct;Double.TryParse(m.Groups["pct"].Value.Replace(',','.'),NumberStyles.Float,CultureInfo.InvariantCulture,out pct);
                return new SaveInfo{mission=m.Groups["mission"].Value.Trim(),percent=pct,autosave=m.Groups["auto"].Success,written_utc=fi.LastWriteTimeUtc,file=path};
            }catch(Exception){return null;}
        }
        // The most recent story save inside a Rockstar user folder (Default\ or Profiles\*\).
        public static SaveInfo Latest(string userDir){
            if(userDir==null||!Directory.Exists(userDir))return null;
            var saves=Directory.GetFiles(userDir,"SGTA5*",SearchOption.AllDirectories)
                .Where(f=>Regex.IsMatch(Path.GetFileName(f),@"^SGTA5\d{4}$")).Select(Read).Where(s=>s!=null)
                .OrderByDescending(s=>s.written_utc).ToList();
            return saves.Count==0?null:saves[0];
        }
        public string Short(string lang){
            string p=percent.ToString("0.#",lang=="es"?CultureInfo.GetCultureInfo("es-ES"):CultureInfo.InvariantCulture);
            return mission+" · "+p+" %";
        }
    }
}
