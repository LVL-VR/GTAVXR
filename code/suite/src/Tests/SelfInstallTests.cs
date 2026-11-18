using System;
using System.IO;
using System.Diagnostics;
using System.Collections.Generic;
using System.Threading;

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
            t("full removal retries a locked installed launcher and cleans both copies and shortcuts",()=>{
                string portable=Path.Combine(fresh,"portable.exe");
                File.Copy(Program.SelfPath,portable);
                SelfInstall.Install(app,start,desk);
                string hash=SafeFs.Hash(portable);
                long bytes=new FileInfo(portable).Length;
                using(File.Open(installed,FileMode.Open,FileAccess.Read,FileShare.Read)){
                    try{CompleteRemoval.RemoveLauncherFiles(portable,hash,bytes,installed,new[]{start,desk});return false;}
                    catch(IOException){
                        if(!File.Exists(portable)||!File.Exists(installed)||!File.Exists(Path.Combine(start,"GTAVXR.lnk")))return false;
                    }
                }
                CompleteRemoval.RemoveLauncherFiles(portable,hash,bytes,installed,new[]{start,desk});
                return !File.Exists(portable)&&!File.Exists(installed)&&
                       !File.Exists(Path.Combine(start,"GTAVXR.lnk"))&&!File.Exists(Path.Combine(desk,"GTAVXR.lnk"));
            });
            t("helper cleanup retries until its temporary executable can be deleted",()=>{
                string stage=Path.Combine(fresh,"gtavxr-remove-fixture");
                Directory.CreateDirectory(stage);
                string temporary=Path.Combine(stage,"GTAVXR.exe");
                File.Copy(Program.SelfPath,temporary);
                using(File.Open(temporary,FileMode.Open,FileAccess.Read,FileShare.Read)){
                    CompleteRemoval.StartHelperCleanup(stage,temporary);
                    Thread.Sleep(500);
                    if(!File.Exists(temporary))return false;
                }
                return SpinWait.SpinUntil(()=>!Directory.Exists(stage),12000);
            });
            t("session minidumps retain two prior captures and preserve session records",()=>{
                string sessions=SafeFs.Under(fresh,"session-dump-retention");
                string[] ids={"20261117T000000-old","20261117T010000-newer","20261117T020000-last","20261117T030000-current"};
                foreach(string id in ids){
                    string native=SafeFs.Under(sessions,id+"/native");
                    Directory.CreateDirectory(native);
                    if(id==ids[3])continue;
                    Json.New(SafeFs.Under(sessions,id+"/session.json"),new SessionRecord{
                        schema="gtavxr-suite2-session-v1",id=id,phase="CLOSED"});
                    File.WriteAllText(SafeFs.Under(native,SessionDumps.Name),id);
                }
                string current=SafeFs.Under(sessions,ids[3]+"/native");
                if(!SessionDumps.Prepare(sessions,ids[3],current)||
                   File.Exists(SafeFs.Under(sessions,ids[0]+"/native/"+SessionDumps.Name))||
                   !File.Exists(SafeFs.Under(sessions,ids[0]+"/session.json"))||
                   !File.Exists(SafeFs.Under(sessions,ids[1]+"/native/"+SessionDumps.Name))||
                   !File.Exists(SafeFs.Under(sessions,ids[2]+"/native/"+SessionDumps.Name)))return false;
                if(SessionDumps.Prepare(sessions,ids[3],SafeFs.Under(sessions,ids[1]+"/native")))return false;
                File.WriteAllText(SafeFs.Under(current,SessionDumps.Name),"existing");
                return !SessionDumps.Prepare(sessions,ids[3],current);
            });
            return new{status=failed==0?"PASS":"FAIL",failed,rows,real_start_menu_touched=false};
        }
    }
}
