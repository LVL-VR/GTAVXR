using System;
using System.IO;
using System.Diagnostics;
using System.Linq;

namespace GtavxrSuite {
    public class MaintenanceRequest {public string schema {get;set;}public string store {get;set;}public string transaction {get;set;}public string action {get;set;}public string updatePackage {get;set;}public string packageHash {get;set;}public int parentPid {get;set;}public long parentTicks {get;set;}public string helperHash {get;set;}}
    public static class Maintenance {
        public static string Request(Store s,string id,string action) {
            if(action!="restore")throw new InvalidDataException("Unsupported deferred maintenance");return RequestInternal(s,id,action,null);
        }
        public static string RequestUpdate(Store s,string id,string package) {return RequestInternal(s,id,"update",package);}
        private static string RequestInternal(Store s,string id,string action,string package) {
            // Reject known conflicts BEFORE asking the running UI to exit. The helper revalidates after exit.
            var j=s.PreflightRestore(id);if(j.phase=="restored")throw new InvalidDataException("Already restored");
            if(!j.changes.Any(c=>String.Equals(SafeFs.Under(j.target,c.path),Program.SelfPath,StringComparison.OrdinalIgnoreCase)))throw new InvalidDataException("Deferred helper is only for its own installed executable");
            if(action=="update") {
                if(j.phase!="installed")throw new InvalidDataException("Recover the interrupted manager transaction before updating");
                var update=Package.Load(package);if(update.files.Count!=1||update.files[0].path!="GTAVXR.exe"||!String.Equals(update.files[0].before,SafeFs.Hash(Program.SelfPath),StringComparison.OrdinalIgnoreCase))throw new InvalidDataException("Update must target only this exact manager executable");
                if(System.Reflection.AssemblyName.GetAssemblyName(SafeFs.Under(package,"payload/GTAVXR.exe")).Name!="GTAVXR")throw new InvalidDataException("Not a Suite managed build");
            }
            string hash=SafeFs.Hash(Program.SelfPath);long bytes=new FileInfo(Program.SelfPath).Length;SafeFs.Budget(s.Root,bytes*3);
            string dir=SafeFs.Under(s.Root,"maintenance/"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(dir);
            string helper=SafeFs.Under(dir,"GTAVXR.exe");SafeFs.CopyNew(Program.SelfPath,helper,hash,bytes);
            using(var p=Process.GetCurrentProcess())Json.New(SafeFs.Under(dir,"request.json"),new MaintenanceRequest{schema="gtavxr-suite-maintenance-v1",store=s.Root,transaction=id,action=action,updatePackage=package,packageHash=package==null?null:SafeFs.Hash(SafeFs.Under(package,"package.json")),parentPid=p.Id,parentTicks=p.StartTime.ToUniversalTime().Ticks,helperHash=hash});return dir;
        }
        public static void Start(string dir) {
            string request=SafeFs.Under(dir,"request.json");var r=Json.Read<MaintenanceRequest>(request);string helper=SafeFs.Under(dir,"GTAVXR.exe");SafeFs.AssertFile(helper,r.helperHash,new FileInfo(helper).Length);
            var si=new ProcessStartInfo(helper,"--maintenance-helper "+ProcessText.Quote(request)+" "+SafeFs.Hash(request)){UseShellExecute=false,CreateNoWindow=true,WindowStyle=ProcessWindowStyle.Hidden};
            using(var p=Process.Start(si)){if(p==null)throw new IOException("Maintenance helper did not start");}
        }
        public static void Run(string request,string hash) {
            SafeFs.AssertFile(request,hash,new FileInfo(request).Length);var r=Json.Read<MaintenanceRequest>(request);
            if(r.schema!="gtavxr-suite-maintenance-v1"||!new[]{"restore","update"}.Contains(r.action)||r.parentPid<=0||r.parentTicks<=0||!String.Equals(SafeFs.Hash(Program.SelfPath),r.helperHash,StringComparison.OrdinalIgnoreCase))throw new InvalidDataException("Invalid maintenance request");
            string result=SafeFs.Under(Path.GetDirectoryName(request),"result.json");
            if(File.Exists(result)){var receipt=Json.Read<System.Collections.Generic.Dictionary<string,object>>(result);if((string)receipt["request_sha256"]==hash&&new[]{"RESTORED","UPDATED"}.Contains((string)receipt["status"]))return;throw new InvalidDataException("Prior maintenance result is not a success");}
            try {
            // Wait for that exact parent, never kill it or confuse a reused PID with the owner.
            try{using(var parent=Process.GetProcessById(r.parentPid)){if(parent.StartTime.ToUniversalTime().Ticks==r.parentTicks&&!parent.WaitForExit(60000))throw new IOException("Parent still active; maintenance not applied");}}catch(ArgumentException){}
            string transaction=r.transaction;
            using(var s=new Store(r.store)){
                if(r.action=="restore")s.Restore(r.transaction,null);
                else {SafeFs.AssertFile(SafeFs.Under(r.updatePackage,"package.json"),r.packageHash,new FileInfo(SafeFs.Under(r.updatePackage,"package.json")).Length);var target=s.Transactions().Single(t=>t.id==r.transaction).target;transaction=s.Install(r.updatePackage,target,null);}
            }
            // Release the store before publishing completion, so readers can immediately recover/use it.
            Json.PublishNew(result,new{status=r.action=="restore"?"RESTORED":"UPDATED",request_sha256=hash,transaction=transaction,gta_launched=false,gui_opened=false});
            } catch(Exception e){if(!File.Exists(result))Json.PublishNew(result,new{status="BLOCKED",request_sha256=hash,error=e.Message,transaction=r.transaction,gta_launched=false,gui_opened=false});throw;}
        }
    }
}
