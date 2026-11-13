using System;
using System.IO;
using System.Diagnostics;
using System.Collections.Generic;

namespace GtavxrSuite {
    // Install + local-channel update over owned folders only (no real Start menu, Desktop or Programs folder).
    public static class SelfInstallTests {
        public static object Run(string fresh){
            fresh=SafeFs.Root(fresh);if(Directory.Exists(fresh))throw new InvalidDataException("Fresh folder required");Directory.CreateDirectory(fresh);
            var rows=new List<object>();int failed=0;
            Action<string,Func<bool>> t=(name,body)=>{bool ok;string err=null;try{ok=body();}catch(Exception e){ok=false;err=e.Message;}if(!ok)failed++;rows.Add(new{name,pass=ok,error=err});};
            string app=Path.Combine(fresh,"app"),start=Path.Combine(fresh,"start"),desk=Path.Combine(fresh,"desk");Directory.CreateDirectory(start);Directory.CreateDirectory(desk);
            string installed=null;
            t("install copies this exe and creates both shortcuts",()=>{installed=SelfInstall.Install(app,start,desk);
                return SafeFs.Hash(installed)==SafeFs.Hash(Program.SelfPath)&&File.Exists(Path.Combine(start,"GTAVXR.lnk"))&&File.Exists(Path.Combine(desk,"GTAVXR.lnk"));});
            t("reinstall replaces in place",()=>SafeFs.Hash(SelfInstall.Install(app,null,null))==SafeFs.Hash(Program.SelfPath));
            var env=Env.ForFixture(Path.Combine(fresh,"w"));
            Directory.CreateDirectory(Path.Combine(env.DevRepo,"work"));File.WriteAllText(Path.Combine(env.DevRepo,"work","AGENT_LEASE.json"),"{}");
            t("no update without a channel",()=>SelfInstall.Available(env)==null);
            // Channel build = a copy of this exe with a different byte appended (a different identity, same program).
            string newer=Path.Combine(fresh,"newer.exe");File.Copy(Program.SelfPath,newer);using(var f=new FileStream(newer,FileMode.Append))f.WriteByte(0);
            t("publish a newer version to the channel",()=>{SelfInstall.Publish(env.DevRepo,newer,"9.9.9","test");var b=SelfInstall.Available(env);return b!=null&&b.version=="9.9.9";});
            t("an older or equal version is not offered",()=>{SelfInstall.Publish(env.DevRepo,newer,SuiteApp.Version,"same");return SelfInstall.Available(env)==null;});
            t("finish-update replaces the target after the old process exits",()=>{
                var dead=Process.Start(new ProcessStartInfo("cmd.exe","/c exit 0"){UseShellExecute=false,CreateNoWindow=true});dead.WaitForExit();int pid=dead.Id;dead.Dispose();
                var p=Process.Start(new ProcessStartInfo(newer,ProcessText.Join(new[]{"--finish-update",installed,pid.ToString(),"no-restart"})){UseShellExecute=false,CreateNoWindow=true});
                p.WaitForExit(60000);return p.ExitCode==0&&SafeFs.Hash(installed)==SafeFs.Hash(newer);});
            return new{status=failed==0?"PASS":"FAIL",failed,rows,real_start_menu_touched=false};
        }
    }
}
