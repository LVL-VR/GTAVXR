using System;
using System.IO;
using System.Linq;
using System.Collections.Generic;
using System.Security.Cryptography;
using System.Text;
using System.Web.Script.Serialization;
using System.Runtime.InteropServices;
using System.ComponentModel;

namespace GtavxrSuite {
    public static class Json {
        public static string Encode(object o) { return new JavaScriptSerializer { MaxJsonLength=16*1024*1024, RecursionLimit=64 }.Serialize(o); }
        public static T Read<T>(string p) {
            SafeFs.NoLinks(p);
            if (new FileInfo(p).Length>16*1024*1024) throw new InvalidDataException("JSON exceeds limit");
            string text=File.ReadAllText(p,new UTF8Encoding(false,true));StrictJson.Validate(text);
            return new JavaScriptSerializer { MaxJsonLength=16*1024*1024, RecursionLimit=64 }.Deserialize<T>(text);
        }
        public static void New(string p, object o) {
            SafeFs.NoLinks(p);
            byte[] bytes=new UTF8Encoding(false,true).GetBytes(Encode(o));
            using(var f=new FileStream(p,FileMode.CreateNew,FileAccess.Write,FileShare.None)) { f.Write(bytes,0,bytes.Length); f.Flush(true); }
        }
        public static void Atomic(string p, object o) {
            SafeFs.NoLinks(p); string t=p+"."+Guid.NewGuid().ToString("N")+".tmp";
            New(t,o);
            if(File.Exists(p)) File.Replace(t,p,null); else File.Move(t,p);
        }
        // Cross-process completion: visibility means the writer has flushed AND closed it.
        // Unlike Atomic, a prior receipt is never overwritten, including a racing publisher.
        public static void PublishNew(string p,object o) {
            SafeFs.NoLinks(p);if(File.Exists(p))throw new IOException("Completion receipt already exists");
            string temp=p+"."+Guid.NewGuid().ToString("N")+".tmp";
            New(temp,o);File.Move(temp,p);
        }
    }
    public static class SafeFs {
        [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)]private static extern uint GetFileAttributesW(string path);
        [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)]private static extern Microsoft.Win32.SafeHandles.SafeFileHandle CreateFileW(string path,uint access,uint share,IntPtr security,uint disposition,uint flags,IntPtr template);
        [DllImport("kernel32.dll",SetLastError=true)]private static extern bool SetFileInformationByHandle(Microsoft.Win32.SafeHandles.SafeFileHandle h,int infoClass,ref uint flags,uint size);
        // Delete one name of a file. Attributes belong to the file, not the name: clearing read-only to delete a hard link
        // would unprotect the stored copy it shares (Blobs). FileDispositionInfoEx (Windows 10 1809+) deletes a read-only
        // file as it is; elsewhere (no hard links there either) the attribute is cleared first.
        public static void DeleteName(string path) {
            string q=path.Length>=248&&!path.StartsWith(@"\\",StringComparison.Ordinal)?@"\\?\"+path:path;
            using(var h=CreateFileW(q,0x00010000,7,IntPtr.Zero,3,0x00200000,IntPtr.Zero)){   // DELETE, share all, existing, the link itself
                uint flags=0x1|0x2|0x10;   // delete | POSIX semantics | ignore read-only
                if(!h.IsInvalid&&SetFileInformationByHandle(h,21,ref flags,4))return;
            }
            if(!File.Exists(path))return;
            File.SetAttributes(path,FileAttributes.Normal);File.Delete(path);
        }
        public static string Hash(string p) { using(var s=File.OpenRead(p)) using(var h=SHA256.Create()) return BitConverter.ToString(h.ComputeHash(s)).Replace("-",""); }
        public static bool IsHash(string h) { return h!=null && h.Length==64 && h.All(c=>Uri.IsHexDigit(c)); }
        public static string Root(string p) {
            string r=CanonicalRoot(p);NoLinks(r);return r;
        }
        private static string CanonicalRoot(string p) {
            if(String.IsNullOrWhiteSpace(p) || !Path.IsPathRooted(p)) throw new InvalidDataException("Absolute root required");
            string r=Path.GetFullPath(p).TrimEnd(Path.DirectorySeparatorChar,Path.AltDirectorySeparatorChar);
            if(r.Length<=3 || r.StartsWith(@"\\",StringComparison.Ordinal) || r.IndexOf(':',2)>=0) throw new InvalidDataException("Unsafe root");
            return r;
        }
        public static string Relative(string r) {
            if(String.IsNullOrWhiteSpace(r)||r.Length>220||Path.IsPathRooted(r)) throw new InvalidDataException("Unsafe relative path");
            r=r.Replace('\\','/');
            foreach(string part in r.Split('/')) {
                if(part.Length==0||part=="."||part==".."||part.EndsWith(".")||part.EndsWith(" ")||part.Any(c=>c<32||"<>:\"|?*".Contains(c))) throw new InvalidDataException("Unsafe path segment");
                string stem=part.Split('.')[0].ToUpperInvariant();
                if(new[]{"CON","PRN","AUX","NUL","CLOCK$"}.Contains(stem) || (stem.Length==4 && (stem.StartsWith("COM")||stem.StartsWith("LPT")) && Char.IsDigit(stem[3]))) throw new InvalidDataException("Device path rejected");
            }
            return r;
        }
        public static string Under(string root,string rel) {
            string r=CanonicalRoot(root), p=Path.GetFullPath(Path.Combine(r,Relative(rel).Replace('/',Path.DirectorySeparatorChar)));
            if(!p.StartsWith(r+Path.DirectorySeparatorChar,StringComparison.OrdinalIgnoreCase)) throw new InvalidDataException("Path escapes root");
            NoLinks(p); return p;
        }
        public static void NoLinks(string path) {
            string p=Path.GetFullPath(path);
            while(!String.IsNullOrEmpty(p)) {
                // Avoid exceptions for every not-yet-created ancestor while retaining dangling-link rejection.
                string query=p.Length>=248&&!p.StartsWith(@"\\",StringComparison.Ordinal)?@"\\?\"+p:p;
                uint attributes=GetFileAttributesW(query);
                if(attributes==UInt32.MaxValue){int error=Marshal.GetLastWin32Error();if(error!=2&&error!=3)throw new Win32Exception(error,"Cannot establish path safety: "+p);}
                else if((attributes&(uint)FileAttributes.ReparsePoint)!=0)throw new InvalidDataException("Reparse point rejected: "+p);
                p=Path.GetDirectoryName(p);
            }
        }
        public static void AssertFile(string p,string hash,long bytes) {
            NoLinks(p);
            if(!IsHash(hash)||!File.Exists(p)||new FileInfo(p).Length!=bytes||!String.Equals(Hash(p),hash,StringComparison.OrdinalIgnoreCase)) throw new InvalidDataException("File identity mismatch: "+p);
        }
        public static void CopyNew(string source,string dest,string hash,long bytes) {
            AssertFile(source,hash,bytes); NoLinks(dest); Directory.CreateDirectory(Path.GetDirectoryName(dest));
            using(var s=File.OpenRead(source)) using(var d=new FileStream(dest,FileMode.CreateNew,FileAccess.Write,FileShare.None)){s.CopyTo(d);d.Flush(true);}
            AssertFile(dest,hash,bytes);
        }
        public static void ReplaceFrom(string source,string dest,string hash,long bytes,long ticks) {
            NoLinks(dest); Directory.CreateDirectory(Path.GetDirectoryName(dest));
            string tmp=Path.Combine(Path.GetDirectoryName(dest),".suite-"+Guid.NewGuid().ToString("N")+".tmp");
            CopyNew(source,tmp,hash,bytes);
            if(ticks>0) File.SetLastWriteTimeUtc(tmp,new DateTime(ticks,DateTimeKind.Utc));
            if(File.Exists(dest)) File.Replace(tmp,dest,null); else File.Move(tmp,dest);
            AssertFile(dest,hash,bytes);
        }
        // The operation must fit on the disk, leaving 1 GiB so Windows never runs completely out. (The project's 90 GiB
        // floor is a rule for AI agents only, never for the Suite.)
        public const long Reserve=1L<<30;
        public static void Budget(string root,long needed) {
            var drive=new DriveInfo(Path.GetPathRoot(Root(root)));
            if(needed<0 || needed>drive.AvailableFreeSpace-Reserve) throw new IOException("NO_SPACE");
        }
    }
    public class Payload { public string path {get;set;} public string sha256 {get;set;} public long bytes {get;set;} public string before {get;set;} }
    public class Package {
        public string schema {get;set;} public string id {get;set;} public string channel {get;set;}
        public List<Payload> files {get;set;}
        public static Package Load(string dir) {
            var p=Json.Read<Package>(SafeFs.Under(dir,"package.json"));
            if(p.schema!="gtavxr-suite-package-v1"||String.IsNullOrWhiteSpace(p.id)||p.id.Length>100||p.files==null||p.files.Count==0||p.files.Count>20000) throw new InvalidDataException("Invalid/empty package");
            var seen=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach(var f in p.files) {
                f.path=SafeFs.Relative(f.path);
                if(!seen.Add(f.path)||f.path.StartsWith(".suite",StringComparison.OrdinalIgnoreCase)||f.bytes<0||!SafeFs.IsHash(f.sha256)||(f.before!=null&&!SafeFs.IsHash(f.before))) throw new InvalidDataException("Invalid/duplicate payload");
                SafeFs.AssertFile(SafeFs.Under(dir,"payload/"+f.path),f.sha256,f.bytes);
            }
            return p;
        }
    }
    public class Change {
        public string path {get;set;} public string after {get;set;} public long afterBytes {get;set;}
        public string before {get;set;} public long beforeBytes {get;set;} public long beforeTicks {get;set;}
        public string source {get;set;} public bool repairWasMissing {get;set;}
    }
    public class Journal {
        public string schema {get;set;} public string id {get;set;} public string package {get;set;}
        public string target {get;set;} public string phase {get;set;} public long createdTicks {get;set;} public List<Change> changes {get;set;}
    }
    public class InstallPreview {
        public string schema {get;set;}public string status {get;set;}public string package {get;set;}public string package_sha256 {get;set;}public string target {get;set;}
        public List<Change> changes {get;set;}public bool target_modified {get;set;}public bool launch_admitted {get;set;}
    }
    public sealed class Store : IDisposable {
        public readonly string Root;
        private FileStream guard;
        public Store(string root) {
            Root=SafeFs.Root(root);SafeFs.Budget(Root,65536);Directory.CreateDirectory(Root);
            guard=new FileStream(SafeFs.Under(Root,"suite.lock"),FileMode.OpenOrCreate,FileAccess.ReadWrite,FileShare.None);
        }
        public void Dispose(){if(guard!=null){guard.Dispose();guard=null;}}
        private string JournalPath(string id){return SafeFs.Under(Root,"transactions/"+SafeFs.Relative(id)+"/journal.json");}
        private Journal PlanInstall(string packageDir,string target) {
            var p=Package.Load(packageDir); target=SafeFs.Root(target);
            if(target.Equals(Root,StringComparison.OrdinalIgnoreCase)||target.StartsWith(Root+"\\",StringComparison.OrdinalIgnoreCase)||Root.StartsWith(target+"\\",StringComparison.OrdinalIgnoreCase)) throw new InvalidDataException("Store and install root must be separate");
            foreach(var old in Transactions()) {
                if(old.phase!="installed"&&old.phase!="restored") throw new InvalidOperationException("Recovery required before installation");
            }
            long size=0; var j=new Journal { schema="gtavxr-suite-journal-v1", id=Guid.NewGuid().ToString("N"),package=p.id,target=target,phase="preparing",createdTicks=DateTime.UtcNow.Ticks,changes=new List<Change>() };
            foreach(var f in p.files) {
                string dest=SafeFs.Under(target,f.path); bool exists=File.Exists(dest);
                if(Directory.Exists(dest)) throw new InvalidDataException("Payload collides with directory");
                string before=exists?SafeFs.Hash(dest):null;
                if(!String.Equals(before,f.before,StringComparison.OrdinalIgnoreCase)) throw new InvalidDataException("Unexpected original; no overwrite authorized: "+f.path);
                long oldBytes=exists?new FileInfo(dest).Length:0;
                size=checked(size+f.bytes*2+oldBytes*2);
                j.changes.Add(new Change {path=f.path,after=f.sha256.ToUpperInvariant(),afterBytes=f.bytes,before=before,beforeBytes=oldBytes,beforeTicks=exists?File.GetLastWriteTimeUtc(dest).Ticks:0,source="payload/"+f.path});
            }
            SafeFs.Budget(Root,size); SafeFs.Budget(target,size);
            return j;
        }
        public InstallPreview InspectInstall(string packageDir,string target){
            string manifest=SafeFs.Under(packageDir,"package.json"),hash=SafeFs.Hash(manifest);var j=PlanInstall(packageDir,target);
            if(SafeFs.Hash(manifest)!=hash)throw new InvalidDataException("Package changed during installation preview");
            return new InstallPreview{schema="gtavxr-suite-install-preview-v1",status="READY_FOR_TRANSACTION_ONLY_NOT_RUNTIME_ADMISSION",package=j.package,package_sha256=hash,target=j.target,changes=j.changes};
        }
        public string Install(string packageDir,string target,Action<string,int> checkpoint,Action<string> recorded=null) {
            // SAME preflight as preview, always recomputed. A saved preview cannot bypass fresh checks.
            var j=PlanInstall(packageDir,target);
            string jp=JournalPath(j.id), dir=Path.GetDirectoryName(jp); Directory.CreateDirectory(dir);
            Json.New(jp,j);
            // Bind the durable transaction ID before copying/staging or mutating destination bytes.
            // Only the ID escapes: callers cannot mutate the in-memory recovery plan.
            if(recorded!=null)recorded(j.id);
            // Copy all incoming files and originals before the first destination mutation.
            foreach(var c in j.changes) {
                SafeFs.CopyNew(SafeFs.Under(packageDir,c.source),SafeFs.Under(dir,c.source),c.after,c.afterBytes);
                if(c.before!=null) SafeFs.CopyNew(SafeFs.Under(target,c.path),SafeFs.Under(dir,"before/"+c.path),c.before,c.beforeBytes);
                if(checkpoint!=null)checkpoint("staging",j.changes.IndexOf(c));
            }
            j.phase="prepared";Json.Atomic(jp,j);if(checkpoint!=null)checkpoint("prepared",-1);
            j.phase="installing";Json.Atomic(jp,j);
            for(int i=0;i<j.changes.Count;i++) {
                var c=j.changes[i]; AssertCurrent(target,c,c.before);
                SafeFs.ReplaceFrom(SafeFs.Under(dir,c.source),SafeFs.Under(target,c.path),c.after,c.afterBytes,0);
                if(checkpoint!=null)checkpoint("file",i);
            }
            j.phase="installed";Json.Atomic(jp,j); return j.id;
        }
        private static void AssertCurrent(string target,Change c,string expected) {
            string dest=SafeFs.Under(target,c.path);
            if(Directory.Exists(dest)|| (expected==null?File.Exists(dest):!File.Exists(dest)||!String.Equals(SafeFs.Hash(dest),expected,StringComparison.OrdinalIgnoreCase))) throw new InvalidDataException("Foreign modification blocks operation: "+c.path);
        }
        private Journal LoadJournal(string jp) {
            SafeFs.NoLinks(jp); var j=Json.Read<Journal>(jp);
            if(j.schema!="gtavxr-suite-journal-v1" || j.createdTicks<=0||j.changes==null||j.changes.Count==0||j.changes.Count>20000||!new[]{"preparing","prepared","installing","installed","restoring","restored"}.Contains(j.phase)) throw new InvalidDataException("Journal invalid");
            if(j.id.Length!=32||!j.id.All(Uri.IsHexDigit)||!String.Equals(JournalPath(j.id),jp,StringComparison.OrdinalIgnoreCase))throw new InvalidDataException("Journal identity invalid");
            j.target=SafeFs.Root(j.target);var seen=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach(var c in j.changes) if(!seen.Add(SafeFs.Relative(c.path))||!SafeFs.IsHash(c.after)||(c.before!=null&&!SafeFs.IsHash(c.before))||c.source!="payload/"+c.path||c.afterBytes<0||c.beforeBytes<0)throw new InvalidDataException("Journal changes invalid");
            return j;
        }
        public Journal[] Transactions() {
            string t=SafeFs.Under(Root,"transactions"); if(!Directory.Exists(t)) return new Journal[0];
            SafeFs.NoLinks(t);
            return Directory.GetDirectories(t).Select(d=>LoadJournal(SafeFs.Under(d,"journal.json"))).ToArray();
        }
        public string RepairMissing(string id) {return RepairMissing(id,null);}
        public string RepairMissing(string id,Action<string,int> checkpoint) {
            string jp=JournalPath(id);var j=LoadJournal(jp);if(j.phase!="installed")throw new InvalidOperationException("Only installed transactions can be repaired");
            foreach(var other in Transactions().Where(o=>o.id!=j.id&&o.phase!="restored"&&o.createdTicks>=j.createdTicks&&String.Equals(o.target,j.target,StringComparison.OrdinalIgnoreCase)))
                if(other.changes.Any(o=>j.changes.Any(c=>String.Equals(c.path,o.path,StringComparison.OrdinalIgnoreCase))))throw new InvalidOperationException("Repair newest overlapping installation first");
            var missing=new List<Change>();
            foreach(var c in j.changes) {
                string dest=SafeFs.Under(j.target,c.path);
                if(File.Exists(dest)){AssertCurrent(j.target,c,c.after);continue;}
                if(Directory.Exists(dest))throw new InvalidDataException("Directory blocks repair");
                SafeFs.AssertFile(SafeFs.Under(Path.GetDirectoryName(jp),c.source),c.after,c.afterBytes);
                if(c.before!=null)SafeFs.AssertFile(SafeFs.Under(Path.GetDirectoryName(jp),"before/"+c.path),c.before,c.beforeBytes);
                c.repairWasMissing=true;missing.Add(c);
            }
            if(missing.Count==0)return "NO_MISSING_FILES";
            SafeFs.Budget(j.target,checked(missing.Sum(f=>f.afterBytes)*2));j.phase="installing";Json.Atomic(jp,j);
            if(checkpoint!=null)checkpoint("repair-prepared",-1);
            foreach(var c in missing){AssertCurrent(j.target,c,null);SafeFs.ReplaceFrom(SafeFs.Under(Path.GetDirectoryName(jp),c.source),SafeFs.Under(j.target,c.path),c.after,c.afterBytes,0);if(checkpoint!=null)checkpoint("repair-file",missing.IndexOf(c));}
            foreach(var c in j.changes)c.repairWasMissing=false;j.phase="installed";Json.Atomic(jp,j);return id;
        }
        public Journal PreflightRestore(string id) {
            string jp=JournalPath(id),dir=Path.GetDirectoryName(jp);var j=LoadJournal(jp);
            if(j.phase=="restored")return j;
            foreach(var other in Transactions().Where(o=>o.id!=j.id&&o.phase!="restored"&&o.createdTicks>=j.createdTicks&&String.Equals(o.target,j.target,StringComparison.OrdinalIgnoreCase)))
                if(other.changes.Any(o=>j.changes.Any(c=>String.Equals(c.path,o.path,StringComparison.OrdinalIgnoreCase))))throw new InvalidOperationException("Restore overlapping updates newest first");
            // Validate the entire plan before restoring any file. Never overwrite unknown bytes.
            foreach(var c in j.changes) {
                string dest=SafeFs.Under(j.target,c.path);string h=File.Exists(dest)?SafeFs.Hash(dest):null;
                if(Directory.Exists(dest))throw new InvalidDataException("Foreign directory blocks restoration: "+c.path);
                if(!String.Equals(h,c.after,StringComparison.OrdinalIgnoreCase)&&!String.Equals(h,c.before,StringComparison.OrdinalIgnoreCase)&&!(h==null&&c.repairWasMissing))throw new InvalidDataException("Foreign change: "+c.path);
                if(c.before!=null&&!String.Equals(h,c.before,StringComparison.OrdinalIgnoreCase))SafeFs.AssertFile(SafeFs.Under(dir,"before/"+c.path),c.before,c.beforeBytes);
            }
            return j;
        }
        public void Restore(string id,Action<string,int> checkpoint) {
            var j=PreflightRestore(id);if(j.phase=="restored")return;
            string jp=JournalPath(id),dir=Path.GetDirectoryName(jp);
            j.phase="restoring";Json.Atomic(jp,j);
            for(int i=j.changes.Count-1;i>=0;i--) {
                var c=j.changes[i];string dest=SafeFs.Under(j.target,c.path);string h=File.Exists(dest)?SafeFs.Hash(dest):null;
                if(Directory.Exists(dest))throw new InvalidDataException("Concurrent foreign directory");
                if(!String.Equals(h,c.after,StringComparison.OrdinalIgnoreCase)&&!String.Equals(h,c.before,StringComparison.OrdinalIgnoreCase)&&!(h==null&&c.repairWasMissing))throw new InvalidDataException("Concurrent foreign change");
                if(c.before!=null&&!String.Equals(h,c.before,StringComparison.OrdinalIgnoreCase))SafeFs.ReplaceFrom(SafeFs.Under(dir,"before/"+c.path),dest,c.before,c.beforeBytes,c.beforeTicks);
                else if(c.before!=null)File.SetLastWriteTimeUtc(dest,new DateTime(c.beforeTicks,DateTimeKind.Utc));
                else if(File.Exists(dest)) {AssertCurrent(j.target,c,c.after);File.Delete(dest);}
                if(checkpoint!=null)checkpoint("restore",i);
            }
            j.phase="restored";Json.Atomic(jp,j);
        }
    }
}
