using System;
using System.IO;
using System.Text;
using System.Diagnostics;
using System.Collections.Generic;

namespace GtavxrSuite {
    public class LeaseHolder { public string agent {get;set;} public string purpose {get;set;} public string acquired_utc {get;set;} }

    // Development host only: the repository's play-runtime lease (tools/agent_lease.ps1 protocol).
    // The Suite takes it as agent "suite" while the game runs, and never touches another holder's lease.
    public sealed class PlayLease : IDisposable {
        private readonly string leasePath;private string leaseHash;private bool released;
        public string Session {get;private set;}
        private PlayLease(string path,string session){leasePath=path;Session=session;}

        private static string Locks(string repo){string d=Path.Combine(repo,"local-cache","agent-locks");Directory.CreateDirectory(d);return d;}
        public static LeaseHolder Holder(string repo){
            string p=Path.Combine(Locks(repo),"play-runtime.lease");
            if(!File.Exists(p))return null;
            try{var d=Json.Read<Dictionary<string,object>>(p);return new LeaseHolder{agent=d.ContainsKey("agent")?d["agent"] as string:"?",purpose=d.ContainsKey("purpose")?d["purpose"] as string:"",acquired_utc=d.ContainsKey("acquired_utc")?d["acquired_utc"] as string:""};}
            catch(Exception){return new LeaseHolder{agent="?",purpose="",acquired_utc=""};}
        }
        public static PlayLease Acquire(string repo,string session,string purpose){
            string locks=Locks(repo),lease=Path.Combine(locks,"play-runtime.lease");
            using(new FileStream(Path.Combine(locks,"play-runtime.lock"),FileMode.OpenOrCreate,FileAccess.ReadWrite,FileShare.None)){
                var h=Holder(repo);
                if(h!=null)throw new LeaseBusyException(h);
                byte[] bytes;
                using(var self=Process.GetCurrentProcess())
                    bytes=Encoding.UTF8.GetBytes(Json.Encode(new{agent="suite",resource="play-runtime",purpose=purpose,acquired_utc=DateTime.UtcNow.ToString("o"),host_pid=self.Id,machine=Environment.MachineName,suite_session=session}));
                using(var w=new FileStream(lease,FileMode.CreateNew,FileAccess.Write,FileShare.None)){w.Write(bytes,0,bytes.Length);w.Flush(true);}
                var r=new PlayLease(lease,session);r.leaseHash=SafeFs.Hash(lease);return r;
            }
        }
        // Release only the exact lease this Suite session wrote.
        public static bool ReleaseOwn(string repo,string session){
            string locks=Locks(repo),lease=Path.Combine(locks,"play-runtime.lease");
            using(new FileStream(Path.Combine(locks,"play-runtime.lock"),FileMode.OpenOrCreate,FileAccess.ReadWrite,FileShare.None)){
                if(!File.Exists(lease))return false;
                var d=Json.Read<Dictionary<string,object>>(lease);
                if(!d.ContainsKey("agent")||(d["agent"] as string)!="suite"||!d.ContainsKey("suite_session")||(d["suite_session"] as string)!=session)return false;
                File.Delete(lease);return true;
            }
        }
        public void Dispose(){
            if(released)return;released=true;
            string repo=Path.GetDirectoryName(Path.GetDirectoryName(Path.GetDirectoryName(leasePath)));
            if(File.Exists(leasePath)&&SafeFs.Hash(leasePath)==leaseHash)ReleaseOwn(repo,Session);
        }
    }
    public class LeaseBusyException : IOException {
        public LeaseHolder Holder {get;private set;}
        public LeaseBusyException(LeaseHolder h):base("play-runtime is held by "+h.agent+": "+h.purpose){Holder=h;}
    }
}
