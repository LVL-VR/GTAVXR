using System;
using System.IO;
using System.Text;
using System.Linq;
using System.Diagnostics;
using System.Threading.Tasks;
using System.Collections.Generic;

namespace GtavxrSuite {
    public static class RarInventory {
        // Lists a .rar with the player's own installed 7-Zip (any version). Nothing from the mod is run or extracted here.
        public static bool TryInspect(string archive,Inspection result,string scratch) {
            string root=SevenZipDir();
            if(root==null){result.warnings.Add("7-Zip is not installed; RAR contents not listed.");return false;}
            string exe=Path.Combine(root,"7z.exe");
            var si=new ProcessStartInfo(exe,"l -slt -sccUTF-8 -- "+ProcessText.Quote(Path.GetFullPath(archive))){UseShellExecute=false,CreateNoWindow=true,WindowStyle=ProcessWindowStyle.Hidden,RedirectStandardOutput=true,RedirectStandardError=true,StandardOutputEncoding=Encoding.UTF8,StandardErrorEncoding=Encoding.UTF8};
            string listing;
            using(var p=new Process{StartInfo=si}) {
                if(!p.Start())throw new IOException("Inventory reader failed to start");
                var stdout=Task.Factory.StartNew(()=>ReadBounded(p.StandardOutput,Int32.MaxValue/2));var stderr=Task.Factory.StartNew(()=>ReadBounded(p.StandardError,16*1024*1024));
                p.WaitForExit();
                listing=stdout.Result;string errors=stderr.Result;if(p.ExitCode!=0)throw new InvalidDataException("RAR reader exit "+p.ExitCode+": "+errors);
            }
            if(scratch!=null){Directory.CreateDirectory(SafeFs.Root(scratch));string file=SafeFs.Under(scratch,"rar-listing.txt");using(var f=new FileStream(file,FileMode.CreateNew,FileAccess.Write,FileShare.None))using(var w=new StreamWriter(f,new UTF8Encoding(false,true))){w.Write(listing);}result.warnings.Add("Raw inventory retained: "+file);}
            Parse(listing,result);result.warnings.Add("RAR contents listed by the installed 7-Zip; nothing extracted or run.");return true;
        }
        // 2.4.0: the player's 7-Zip wherever it was installed: Program Files, its own registry key (HKCU / HKLM Path64 / Path), or
        // a folder on PATH. A folder counts only with both 7z.exe and 7z.dll.
        public static string SevenZipDir() {
            var dirs=new List<string>{Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles),"7-Zip"),
                Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86),"7-Zip")};
            foreach(var hive in new[]{Microsoft.Win32.Registry.CurrentUser,Microsoft.Win32.Registry.LocalMachine})
                foreach(string name in new[]{"Path64","Path"})
                    try{using(var k=hive.OpenSubKey(@"SOFTWARE\7-Zip")){var v=k==null?null:k.GetValue(name) as string;if(!String.IsNullOrEmpty(v))dirs.Add(v);}}catch(Exception){}
            foreach(string d in (Environment.GetEnvironmentVariable("PATH")??"").Split(';'))if(d.Trim().Length>0)dirs.Add(d.Trim().Trim('"'));
            foreach(string d in dirs){
                try{if(File.Exists(Path.Combine(d,"7z.exe"))&&File.Exists(Path.Combine(d,"7z.dll")))return Path.GetFullPath(d);}catch(Exception){}
            }
            return null;
        }
        private static string ReadBounded(StreamReader reader,int max) {
            var b=new StringBuilder();var chars=new char[8192];bool over=false;int n;
            while((n=reader.Read(chars,0,chars.Length))>0){if(b.Length<=max-n&&!over)b.Append(chars,0,n);else over=true;}
            if(over)throw new InvalidDataException("Inventory output exceeds bound");return b.ToString();
        }
        public static void Parse(string listing,Inspection result) {
            int separator=listing.IndexOf("----------",StringComparison.Ordinal);if(separator<0)throw new InvalidDataException("Unrecognized 7-Zip listing");
            var seen=new HashSet<string>(StringComparer.OrdinalIgnoreCase);var row=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);
            Action flush=()=>{if(row.Count==0)return;string path;if(!row.TryGetValue("Path",out path))throw new InvalidDataException("RAR member path missing");path=SafeFs.Relative(path);if(!seen.Add(path))throw new InvalidDataException("RAR duplicate member");
                foreach(var k in row.Keys)if(k.IndexOf("link",StringComparison.OrdinalIgnoreCase)>=0&&!String.IsNullOrWhiteSpace(row[k]))throw new InvalidDataException("RAR link rejected");
                string alternate;if(row.TryGetValue("Alternate Stream",out alternate)&&alternate=="+")throw new InvalidDataException("RAR alternate stream rejected");
                string enc;if(row.TryGetValue("Encrypted",out enc)&&enc=="+")throw new InvalidDataException("ARCHIVE_PASSWORD");
                string size;long bytes=0;if(row.TryGetValue("Size",out size)&&!Int64.TryParse(size,out bytes))throw new InvalidDataException("RAR size invalid");if(bytes<0)throw new InvalidDataException("RAR negative size");
                result.declaredExpandedBytes=checked(result.declaredExpandedBytes+bytes);result.members.Add(path);row.Clear();};
            bool footer=false;using(var reader=new StringReader(listing.Substring(separator))) {string line;while((line=reader.ReadLine())!=null){if(line.StartsWith("----------",StringComparison.Ordinal))continue;if(line.Length==0){flush();continue;}
                if(row.Count==0&&line.StartsWith("Warnings: ",StringComparison.Ordinal)){int count;if(footer||!Int32.TryParse(line.Substring(10),out count)||count<0)throw new InvalidDataException("Invalid warning footer");result.warnings.Add("7-Zip warning count="+count+"; inspect raw header (research RAR may use .zip suffix)");footer=true;continue;}
                if(footer)throw new InvalidDataException("Data after archive footer");int eq=line.IndexOf(" = ",StringComparison.Ordinal);if(eq<0)throw new InvalidDataException("Malformed inventory row");string k=line.Substring(0,eq);if(row.ContainsKey(k))throw new InvalidDataException("Ambiguous inventory key");row.Add(k,line.Substring(eq+3));}flush();}
        }
    }
}
