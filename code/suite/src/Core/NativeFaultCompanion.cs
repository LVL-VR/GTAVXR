using System;
using System.IO;
using System.Diagnostics;
using System.Text;

namespace GtavxrSuite {
    public class FaultMonitorStart {public string status {get;set;}public ChildIdentity collector {get;set;}public ChildIdentity target {get;set;}}
    // The ONLY admitted collectors are the behavior-qualified versions, not whatever
    // an unrelated bridge build emits. Validation does not execute it or qualify GTA.
    // Hash: frozen BC5B778F (older versions; refuses its dump below 90 GiB free, an agent rule).
    // Hash166: the same collector with the player rule (dump room + 1 GiB reserve), self-test PASS
    // (GTAVXR evidence/native-openxr-20260926/legacy3889-166-native/fault-monitor-v1).
    public static class FaultMonitorPolicy {
        public const string Hash="BC5B778FE8A386AD6234D20071CD227059DA5BBA5B556EE5DA2D8CB159336D84";
        public const string Hash166="4BE694A9DD41A823D8BDF61CF3EB6DB765DC1EE4666C372280D2DD8D1E9A157F";
        public static bool Admitted(string sha){return sha==Hash||sha==Hash166;}
        public static object Inspect(string path){
            SafeFs.NoLinks(path);string sha=File.Exists(path)?SafeFs.Hash(path):null;if(!Admitted(sha))sha=Hash; // unknown file: AssertFile reports the mismatch
            SafeFs.AssertFile(path,sha,28160);return new{status="EXACT_PREVIOUSLY_BEHAVIOR_QUALIFIED_COLLECTOR",sha256=sha,bytes=28160,executed=false,gta_launched=false};}
        internal static void Recover(string session,string target,string expectedCollector){
            string start=SafeFs.Under(session,"fault-monitor-start.json"),exit=SafeFs.Under(session,"fault-monitor-exit.json");
            ProcessOwnership.AssertClosed(null,target);
            if(File.Exists(exit)){
                var proof=Json.Read<System.Collections.Generic.Dictionary<string,object>>(exit);
                if((string)proof["status"]!="CLOSED"||!(bool)proof["target_closed"])throw new IOException("Collector close receipt is unresolved");
                // Do not trust a close receipt while its recorded collector is live.
                if(File.Exists(start)){var original=Json.Read<FaultMonitorStart>(start);if(original.collector!=null){if(expectedCollector==null||original.collector.executable!=expectedCollector||!Admitted(original.collector.sha256))throw new InvalidDataException("Collector close identity mismatch");ProcessOwnership.AssertClosed(original.collector,expectedCollector);}}return;
            }
            if(!File.Exists(start)){
                // A crash between spawning the collector and publishing its identity
                // is not proof of absence. Never restore or kill an unknown process.
                if(Directory.Exists(SafeFs.Under(session,"fault-capture"))){
                    if(expectedCollector==null)throw new IOException("Collector may have started without published identity; expected image is unknown");
                    // No identity means NO kill authority. Recovery can still finish
                    // once the target AND every possible collector owner are closed.
                    ProcessOwnership.AssertClosed(null,expectedCollector);
                    Json.PublishNew(exit,new{status="CLOSED",target_closed=true,recovered=true,identity_gap=true,no_possible_collector_owner=true});
                }
                return;
            }
            var record=Json.Read<FaultMonitorStart>(start);
            if(record.status=="TARGET_ALREADY_CLOSED"&&record.collector==null)return;
            if(record.status!="STARTED"||record.collector==null||record.target==null||record.target.executable!=target||!Admitted(record.collector.sha256)||expectedCollector==null||record.collector.executable!=expectedCollector)throw new InvalidDataException("Collector recovery identity mismatch");
            Inspect(expectedCollector);
            if(ProcessOwnership.IsSameProcessAlive(record.collector)){
                using(var monitor=Process.GetProcessById(record.collector.pid)){
                    if(!monitor.WaitForExit(10000)){
                        // Recheck this handle's identity immediately before killing ONLY
                        // the owned debugger after its target is confirmed closed.
                        if(monitor.StartTime.ToUniversalTime().Ticks!=record.collector.creation_utc_ticks||!ProcessOwnership.IsSameProcessAlive(record.collector))throw new IOException("Collector changed during recovery");
                        monitor.Kill();if(!monitor.WaitForExit(5000))throw new IOException("Collector closure remains pending");
                    }
                }
            }
            Json.PublishNew(exit,new{status="CLOSED",collector=record.collector,target_closed=true,recovered=true});
        }
    }
    internal sealed class NativeFaultCompanion : IProcessCompanion,IDisposable {
        private readonly string source,session;private Process monitor;private ChildIdentity identity;
        private bool started,closed;private readonly object textGate=new object();private readonly StringBuilder stdout=new StringBuilder(),stderr=new StringBuilder();
        private string startError;
        public NativeFaultCompanion(string executable,string ownedSession){
            source=SafeFs.Root(executable);FaultMonitorPolicy.Inspect(source);session=SafeFs.Root(ownedSession);
            if(!Directory.Exists(session))throw new InvalidDataException("Owned collector session required");
            foreach(string name in new[]{"fault-monitor-start.json","fault-monitor-start-failed.json","fault-monitor-exit.json","fault-monitor-text.json","fault-capture"})
                if(File.Exists(SafeFs.Under(session,name))||Directory.Exists(SafeFs.Under(session,name)))throw new InvalidDataException("Fresh owned collector session required");
            SafeFs.Budget(session,2L*1024*1024*1024); // room for a crash dump; without it Play goes on with no collector (caller catches)
        }
        private void Append(StringBuilder text,string line){if(line==null)return;lock(textGate){string addition=(line.Length>4096?line.Substring(0,4096):line)+Environment.NewLine;int remaining=65536-text.Length;if(remaining>0)text.Append(addition,0,Math.Min(remaining,addition.Length));}}
        private void Record(string name,object value){Json.PublishNew(SafeFs.Under(session,name),value);}
        public void AfterStart(Process child,string executable){
            if(started)throw new InvalidOperationException("Collector start may be attempted only once");started=true;
            try{
                if(child==null||child.HasExited){Record("fault-monitor-start.json",new{status="TARGET_ALREADY_CLOSED",startup_fault_coverage_guaranteed=false});return;}
                FaultMonitorPolicy.Inspect(source);var target=ProcessOwnership.Capture(child,executable);
                string capture=SafeFs.Under(session,"fault-capture");Directory.CreateDirectory(capture);
                var start=new ProcessStartInfo(source,child.Id.ToString(System.Globalization.CultureInfo.InvariantCulture)+" "+ProcessText.Quote(executable)+" "+ProcessText.Quote(capture)){UseShellExecute=false,CreateNoWindow=true,WindowStyle=ProcessWindowStyle.Hidden,WorkingDirectory=Path.GetDirectoryName(source),RedirectStandardOutput=true,RedirectStandardError=true};
                monitor=new Process{StartInfo=start};monitor.OutputDataReceived+=(s,e)=>Append(stdout,e.Data);monitor.ErrorDataReceived+=(s,e)=>Append(stderr,e.Data);
                if(!monitor.Start())throw new IOException("Collector did not start");identity=ProcessOwnership.Capture(monitor,source);monitor.BeginOutputReadLine();monitor.BeginErrorReadLine();
                Record("fault-monitor-start.json",new{status="STARTED",collector=identity,target=target,kill_target_on_exit=false,memory_writes=false,registry_changes=false,startup_fault_coverage_guaranteed=false});
            }catch(Exception error){
                // As in frozen146: optional diagnostic attach failure must not terminate gameplay.
                startError=error.GetType().Name+": "+error.Message;
                try{Record("fault-monitor-start-failed.json",new{error=startError,target_not_terminated=true});}catch(IOException){}
            }
        }
        public void AfterExit(Process child){
            if(closed)return;if(child!=null&&!child.HasExited)throw new IOException("Cannot close the collector while its target remains active");
            bool forced=false;
            if(monitor!=null){
                if(!monitor.WaitForExit(10000)){
                    if(identity==null||!ProcessOwnership.IsSameProcessAlive(identity))throw new IOException("Collector identity unresolved; no forced close or restoration");
                    // ONLY this exact owned collector, AFTER target exit. Never the game/other processes.
                    monitor.Kill();if(!monitor.WaitForExit(5000))throw new IOException("Owned collector did not close; recovery pending");forced=true;
                }
                monitor.WaitForExit(); // Complete asynchronous bounded text readers.
            }
            Record("fault-monitor-exit.json",new{status="CLOSED",collector=identity,exit_code=monitor==null?(int?)null:monitor.ExitCode,forced_owned_collector=forced,target_closed=true,start_error=startError,dump_present=File.Exists(SafeFs.Under(session,"fault-capture/native-fault.dmp")),cause_not_inferred_from_exit=true});
            lock(textGate){Record("fault-monitor-text.json",new{stdout=stdout.ToString(),stderr=stderr.ToString(),bounded_chars_per_stream=65536});}
            closed=true;
        }
        public void Dispose(){if(monitor!=null){monitor.Dispose();monitor=null;}} // No implicit kill/restore on disposal.
    }
}
