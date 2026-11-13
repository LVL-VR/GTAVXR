using System;
using System.IO;
using System.Diagnostics;
using System.Linq;

namespace GtavxrSuite {
    public class ChildIdentity {
        public int pid {get;set;}public long creation_utc_ticks {get;set;}public string executable {get;set;}public string sha256 {get;set;}
    }
    // No process is killed here. A PID by itself is never authority to restore files or control a process.
    public static class ProcessOwnership {
        internal static void AssertImage(string actual,string expected,bool closed){
            if(closed)return;
            if(String.IsNullOrEmpty(actual))throw new IOException("Process image is not yet available or is closing; ownership remains unresolved");
            if(!String.Equals(actual,expected,StringComparison.OrdinalIgnoreCase))throw new InvalidDataException("Live process image differs from the owned path");
        }
        internal static void AssertNoNativeGame(){
            foreach(string name in new[]{"GTA5","GTA5_BE","PlayGTAV","GTAVXR_SourceBaseline","game_win64_bankrelease","game_win64_release"}){
                var candidates=Process.GetProcessesByName(name);try{foreach(var process in candidates)if(!process.HasExited)throw new IOException("Close the active game before this operation: "+name);}finally{foreach(var process in candidates)process.Dispose();}
            }
        }
        public static ChildIdentity Capture(Process child,string executable){
            executable=SafeFs.Root(executable);SafeFs.NoLinks(executable);
            var result=new ChildIdentity{pid=child.Id,creation_utc_ticks=child.StartTime.ToUniversalTime().Ticks,executable=executable,sha256=SafeFs.Hash(executable)};
            ReadImage(()=>{child.Refresh();var module=child.MainModule;return module==null?null:module.FileName;},()=>child.HasExited,executable,15000,250);
            return result;
        }
        // Right after the start Windows may not let the module list be read yet (Win32Exception "only part of a ReadProcessMemory
        // request was completed": Suite 2.3.5 session 20261103T032803 lost the game it had just started). While the child lives the
        // read is retried for a bounded time; a different image fails at once, an unreadable one only after the wait.
        internal static void ReadImage(Func<string> read,Func<bool> exited,string executable,int timeoutMs,int stepMs){
            var until=DateTime.UtcNow.AddMilliseconds(timeoutMs);
            while(true){
                Exception last;
                try{if(exited())return;AssertImage(read(),executable,exited());return;}
                catch(InvalidOperationException e){last=e;}
                catch(System.ComponentModel.Win32Exception e){last=e;}
                catch(IOException e){last=e;}
                catch(NullReferenceException e){last=e;} // Framework ProcessModule.FileName can race a fast exit.
                if(exited())return;
                if(DateTime.UtcNow>=until)throw new IOException("The game's process could not be read: "+last.Message,last);
                System.Threading.Thread.Sleep(stepMs);
            }
        }
        public static bool IsSameProcessAlive(ChildIdentity identity){
            if(identity==null||identity.pid<=0||identity.creation_utc_ticks<=0||!SafeFs.IsHash(identity.sha256))throw new InvalidDataException("Complete process identity required");
            SafeFs.Root(identity.executable);Process process;
            try{process=Process.GetProcessById(identity.pid);}catch(ArgumentException){return false;}
            using(process){
                try{
                    if(process.HasExited||process.StartTime.ToUniversalTime().Ticks!=identity.creation_utc_ticks)return false;
                    var module=process.MainModule;if(process.HasExited)return false;AssertImage(module==null?null:module.FileName,identity.executable,false);
                    return true;
                }catch(InvalidOperationException){if(process.HasExited)return false;throw;}
                 catch(NullReferenceException){if(process.HasExited)return false;throw;}
                // Access denied/unknown process state is deliberately NOT translated into 'closed'.
            }
        }
        public static void AssertClosed(ChildIdentity identity,string expected){
            expected=SafeFs.Root(expected);
            if(identity!=null){
                if(!String.Equals(expected,identity.executable,StringComparison.OrdinalIgnoreCase))throw new InvalidDataException("Session/child image mismatch");
                if(IsSameProcessAlive(identity))throw new IOException("The exact child is still running; recovery must wait");
            }
            // Covers parent death between Process.Start and identity publication, and an unknown relaunch.
            // Conservatively refuse even a same-name process at another path. No CIM dependency or stale-lock deletion.
            var candidates=Process.GetProcessesByName(Path.GetFileNameWithoutExtension(expected));
            try{foreach(var process in candidates)if(!process.HasExited)throw new IOException("A possible image owner is active; no restoration");}
            finally{foreach(var process in candidates)process.Dispose();}
        }
        public static Journal PreflightRestore(Store store,string transaction,ChildIdentity identity,string expected){
            AssertClosed(identity,expected);var journal=store.PreflightRestore(transaction);
            if(!expected.Equals(SafeFs.Under(journal.target,Path.GetFileName(expected)),StringComparison.OrdinalIgnoreCase))throw new InvalidDataException("Child is not installed directly in this transaction target");
            var owned=journal.changes.Where(row=>row.path.Equals(Path.GetFileName(expected),StringComparison.OrdinalIgnoreCase)).ToArray();
            if(owned.Length!=1||(identity!=null&&owned[0].after!=identity.sha256))throw new InvalidDataException("Child image is not this journal's exact payload");
            // Check EVERY present payload before any mutation, not only the executable after restoring other files.
            foreach(var row in journal.changes){string file=SafeFs.Under(journal.target,row.path);if(File.Exists(file))using(var handle=new FileStream(file,FileMode.Open,FileAccess.Read,FileShare.None)){} }
            return journal;
        }
    }
}
