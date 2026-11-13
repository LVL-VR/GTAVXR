using System;
using System.IO;
using System.Linq;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;

namespace GtavxrSuite {
    // ctime = NTFS change time (moves on every write, even when the visible date is put back); fid = volume + file index
    // (moves when the file is replaced by another one). 0 / null when unknown (older inventories, unreadable files).
    public class InventoryFile { public string path {get;set;} public long bytes {get;set;} public long mtime {get;set;} public long ctime {get;set;} public string fid {get;set;} }
    public class GameInventory {
        public string schema {get;set;}
        public string game_dir {get;set;}
        public List<InventoryFile> files {get;set;}
        public List<string> dirs {get;set;}
        public List<string> owned {get;set;}   // GTAVXR files in the game folder when the inventory was taken
    }
    public class FlightReport { public List<string> quarantined {get;set;} public List<string> changed {get;set;} public List<string> missing {get;set;} public string retail {get;set;} }

    // Test flight: before a test, the Suite writes down what the game folder holds; after it (or after a crash),
    // GTAVXR goes back to the version + mods installed before, new files that appeared in the game folder are moved
    // aside (quarantine, recoverable), and changed or missing game files are reported (the Suite has no copy of the game to fix them).
    public static class TestFlight {
        [StructLayout(LayoutKind.Sequential)] private struct BasicInfo { public long creation,access,write,change; public uint attributes,pad; }
        [StructLayout(LayoutKind.Sequential)] private struct HandleInfo {
            public uint attributes,creationLow,creationHigh,accessLow,accessHigh,writeLow,writeHigh;
            public uint volumeSerial,sizeHigh,sizeLow,links,indexHigh,indexLow;
        }
        [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)]
        private static extern SafeFileHandle CreateFileW(string path,uint access,uint share,IntPtr security,uint disposition,uint flags,IntPtr template);
        [DllImport("kernel32.dll",SetLastError=true)] private static extern bool GetFileInformationByHandleEx(SafeFileHandle handle,int infoClass,out BasicInfo info,uint size);
        [DllImport("kernel32.dll",SetLastError=true)] private static extern bool GetFileInformationByHandle(SafeFileHandle handle,out HandleInfo info);

        // Attribute-only open (no read, shares everything): the game's files are never locked or touched.
        static void Identity(string full,InventoryFile f){
            string path=full.Length>=248?@"\\?\"+full:full;
            using(var h=CreateFileW(path,0x80,7,IntPtr.Zero,3,0x02000000,IntPtr.Zero)){
                if(h.IsInvalid)return;
                BasicInfo b;if(GetFileInformationByHandleEx(h,0,out b,(uint)Marshal.SizeOf(typeof(BasicInfo))))f.ctime=b.change;
                HandleInfo i;if(GetFileInformationByHandle(h,out i)&&(i.indexHigh!=0||i.indexLow!=0))f.fid=i.volumeSerial.ToString("X8")+"-"+i.indexHigh.ToString("X8")+i.indexLow.ToString("X8");
            }
        }

        public static GameInventory Take(string game,GameState state){
            game=SafeFs.Root(game);
            var inv=new GameInventory{schema="gtavxr-suite2-inventory-v2",game_dir=game,files=new List<InventoryFile>(),dirs=new List<string>(),
                owned=state==null?new List<string>():state.files.Select(f=>f.path).ToList()};
            foreach(string f in Directory.EnumerateFiles(game,"*",SearchOption.AllDirectories)){
                var fi=new FileInfo(f);
                var row=new InventoryFile{path=Rel(game,f),bytes=fi.Length,mtime=fi.LastWriteTimeUtc.Ticks};
                Identity(f,row);inv.files.Add(row);
            }
            foreach(string d in Directory.EnumerateDirectories(game,"*",SearchOption.AllDirectories))inv.dirs.Add(Rel(game,d));
            return inv;
        }
        static string Rel(string root,string full){return full.Substring(root.TrimEnd('\\').Length+1).Replace('\\','/');}

        // Size or visible date always count; change time and identity only when both inventories know them (older v1 ones do not).
        public static bool Changed(InventoryFile was,InventoryFile now){
            if(was.bytes!=now.bytes||was.mtime!=now.mtime)return true;
            if(was.ctime!=0&&now.ctime!=0&&was.ctime!=now.ctime)return true;
            return was.fid!=null&&now.fid!=null&&!String.Equals(was.fid,now.fid,StringComparison.Ordinal);
        }

        // After GTAVXR is back to its previous state: move new foreign files aside, report changed and missing foreign files.
        public static FlightReport Settle(GameInventory before,GameState now,IEnumerable<string> verifiedPaths,string quarantine){
            string game=SafeFs.Root(before.game_dir);
            var after=Take(game,now);
            var ours=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach(string p in before.owned)ours.Add(p);
            if(now!=null)foreach(var f in now.files)ours.Add(f.path);
            foreach(string p in verifiedPaths??new string[0])ours.Add(p);
            var old=before.files.ToDictionary(f=>f.path,StringComparer.OrdinalIgnoreCase);
            var rep=new FlightReport{quarantined=new List<string>(),changed=new List<string>(),missing=new List<string>()};
            var seen=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach(var f in after.files){
                seen.Add(f.path);
                if(ours.Contains(f.path))continue;
                InventoryFile was;
                if(!old.TryGetValue(f.path,out was)){
                    string dest=SafeFs.Under(quarantine,f.path);
                    Directory.CreateDirectory(Path.GetDirectoryName(dest));
                    string src=SafeFs.Under(game,f.path);
                    File.SetAttributes(src,FileAttributes.Normal);File.Move(src,dest);
                    rep.quarantined.Add(f.path);
                }else if(Changed(was,f))rep.changed.Add(f.path);
            }
            // Game files that were there before the test and are gone now (the Suite has no copy to put back).
            foreach(var f in before.files)if(!seen.Contains(f.path)&&!ours.Contains(f.path))rep.missing.Add(f.path);
            // Folders that appeared during the test and are empty now go away (deepest first).
            var oldDirs=new HashSet<string>(before.dirs,StringComparer.OrdinalIgnoreCase);
            var ourDirs=new HashSet<string>(now==null||now.dirs==null?new List<string>():now.dirs,StringComparer.OrdinalIgnoreCase);
            foreach(string d in after.dirs.Where(x=>!oldDirs.Contains(x)&&!ourDirs.Contains(x)).OrderByDescending(x=>x.Length)){
                string full=SafeFs.Under(game,d);
                if(Directory.Exists(full)&&!Directory.EnumerateFileSystemEntries(full).Any())Directory.Delete(full);
            }
            return rep;
        }
    }
}
