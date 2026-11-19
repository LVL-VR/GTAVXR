using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;

namespace GtavxrSuite {
    internal static class CompleteRemoval {
        static readonly string[] RootDirectories={"blobs","content-build","content-src","dlss","embedded","game","mods",
            "profiles","quarantine","recycle","runtime","scratch","sessions","reports","versions","ofxr"};
        static readonly string[] RootFiles={"settings.json","embedded-install.lock",ReleaseMigration.JournalName,ReleaseMigration.HandoverName};

        internal static List<string> Libraries(Env env,DataLocation location) {
            var roots=(env.ManagedLocation?location.KnownLibraries():new[]{env.DataRoot})
                .Select(SafeFs.Root).Where(root=>Present(root)||Equal(root,env.DataRoot)).ToList();
            foreach(string root in roots){
                if(roots.Any(other=>!Equal(root,other)&&Within(root,other))||
                   Within(env.GameDir,root)||Within(env.UserDataDir,root)||
                   Within(root,env.GameDir)||Within(root,env.UserDataDir)||
                   Within(root,Path.GetFullPath(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile))))
                    throw new InvalidDataException("Unsafe Suite data path: "+root);
                if(!Equal(root,env.DataRoot)&&!File.Exists(SafeFs.Under(root,"settings.json")))
                    throw new InvalidDataException("Could not verify the previous Suite library: "+root);
                var settings=Path.Combine(root,"settings.json");
                if(File.Exists(settings)&&Json.Read<SuiteSettings>(settings).schema!="gtavxr-suite2-settings-v1")
                    throw new InvalidDataException("Invalid Suite library: "+root);
                foreach(string item in Directory.EnumerateFileSystemEntries(root)){
                    SafeFs.NoLinks(item);
                    string name=Path.GetFileName(item);
                    var recognized=Directory.Exists(item)?RootDirectories:RootFiles;
                    if(!recognized.Contains(name,StringComparer.OrdinalIgnoreCase))
                        throw new InvalidDataException("Unexpected file in Suite data. Move it out and retry: "+item);
                    if(Directory.Exists(item))CheckTree(item);
                }
            }
            return roots;
        }

        static bool Present(string path) {
            try { return (File.GetAttributes(path)&FileAttributes.Directory)!=0; }
            catch(FileNotFoundException) { return false; }
            catch(DirectoryNotFoundException) {
                if(new DriveInfo(Path.GetPathRoot(path)).IsReady)return false;
                throw new IOException("Reconnect the drive before removing: "+path);
            }
        }

        static bool Equal(string first,string second) {
            return String.Equals(first,second,StringComparison.OrdinalIgnoreCase);
        }

        static bool Within(string root,string path) {
            return !String.IsNullOrEmpty(root)&&!String.IsNullOrEmpty(path)&&
                (Equal(root,path)||path.StartsWith(root.TrimEnd('\\')+"\\",StringComparison.OrdinalIgnoreCase));
        }

        internal static List<string> KnownLaunchers() {
            var launchers=new List<string>{Program.SelfPath};
            var installed=SelfInstall.InstalledExe();
            if(!Equal(installed,Program.SelfPath)&&File.Exists(installed)){
                var running=new FileInfo(Program.SelfPath);
                if(!SelfInstall.IsPreviousLauncher(installed))
                    SafeFs.AssertFile(installed,SafeFs.Hash(running.FullName),running.Length);
                launchers.Add(installed);
            }
            return launchers;
        }

        internal static void RemoveData(Env env,DataLocation location,List<string> confirmed) {
            var roots=Libraries(env,location);
            if(!roots.SequenceEqual(confirmed,StringComparer.OrdinalIgnoreCase))throw new IOException("Suite locations changed. Review them again.");
            var scopes=new List<GameScope>();
            try{
                foreach(string root in roots)scopes.Add(GameScope.Acquire(root));
                RemoveDataOwned(env,location,roots,scopes);
            }finally{
                for(int index=scopes.Count-1;index>=0;index--)scopes[index].Dispose();
            }
        }

        static void RemoveDataOwned(Env env,DataLocation location,List<string> roots,List<GameScope> scopes) {
            var libraries=roots.Select(root=>Equal(root,env.DataRoot)?env:Env.Product(root)).ToArray();
            foreach(var library in libraries){
                var session=new PlaySession(library);
                if(session.GameRunning())throw new InvalidOperationException("GAME_RUNNING");
                if(new ReleaseMigration(library).Pending)
                    throw new InvalidOperationException("REMOVE_UPDATE_PENDING");
                if(session.Unfinished().Count!=0)throw new InvalidOperationException("RECOVERY_REQUIRED");
                CheckRetainedPlayerFiles(library);
            }
            var owned=libraries.Select(library=>new{library,layer=new GameLayer(library)})
                .Select(item=>new{item.library,item.layer,state=item.layer.State(),pending=item.layer.PendingOperation()})
                .Where(item=>item.state!=null||item.pending!=null).ToList();
            if(owned.Any(item=>item.pending!=null&&(item.pending.version!=null||item.pending.adds.Count!=0||
               item.pending.keeps.Count!=0||item.pending.quarantine!=null||
               item.state!=null&&!Equal(item.pending.game_dir,item.state.game_dir))))
                throw new InvalidOperationException("RECOVERY_REQUIRED");
            var gameIdentities=new HashSet<string>(StringComparer.Ordinal);
            foreach(string path in owned.Select(item=>SafeFs.Root(item.pending==null?item.state.game_dir:item.pending.game_dir))
                     .Distinct(StringComparer.OrdinalIgnoreCase)){
                if(!new DriveInfo(Path.GetPathRoot(path)).IsReady)throw new IOException("Reconnect the GTA drive: "+path);
                if(Directory.Exists(path)&&gameIdentities.Add(GameScope.NameFor(path)))scopes.Add(GameScope.Acquire(path));
            }
            foreach(var item in owned){
                string game=SafeFs.Root(item.pending==null?item.state.game_dir:item.pending.game_dir);
                if(!new DriveInfo(Path.GetPathRoot(game)).IsReady)throw new IOException("Reconnect the GTA drive: "+game);
                foreach(var file in item.pending==null?item.state.files:item.pending.removes){
                    string target=SafeFs.Under(game,file.path);
                    if(File.Exists(target))SafeFs.AssertFile(target,file.sha256,file.bytes);
                }
            }
            if(libraries.Any(library=>new PlaySession(library).GameRunning()))
                throw new InvalidOperationException("GAME_RUNNING");
            foreach(var item in owned){
                if(item.pending!=null)item.layer.Recover(null);
                else{
                    item.library.GameDir=item.state.game_dir;
                    item.layer.Uninstall(null);
                }
            }
            // Leave the launcher until all destinations have been processed. An interrupted deletion can be retried there.
            for(int index=0;index<roots.Count;index++)scopes[index].ReleaseDirectoryHandle();
            foreach(string root in roots.Where(root=>!Equal(root,env.DataRoot)))DeleteDirectory(root,true);
            DeleteDirectory(env.DataRoot,true);
            if(env.ManagedLocation&&File.Exists(location.Pointer))SafeFs.DeleteName(location.Pointer);
        }

        static void CheckRetainedPlayerFiles(Env env) {
            foreach(string directory in Directory.EnumerateDirectories(env.Dir("sessions"))){
                foreach(string folder in new[]{"aside","asi-aside"}){
                    string retained=SafeFs.Under(directory,folder);
                    if(Directory.Exists(retained)&&Directory.EnumerateFiles(retained,"*",SearchOption.AllDirectories).Any())
                        throw new IOException("Move the original game files kept here to a safe location before removing GTAVXR: "+retained);
                }
            }
            string quarantine=SafeFs.Under(env.DataRoot,"quarantine");
            if(Directory.Exists(quarantine)&&Directory.EnumerateFiles(quarantine,"*",SearchOption.AllDirectories).Any())
                throw new IOException("Review the files GTAVXR set aside and move the ones you want to keep before removal: "+quarantine);
        }

        static void CheckTree(string directory) {
            foreach(string item in Directory.EnumerateFileSystemEntries(directory)){
                SafeFs.NoLinks(item);
                if(Directory.Exists(item))CheckTree(item);
            }
        }

        static void DeleteDirectory(string directory,bool keepSettingsLast) {
            SafeFs.NoLinks(directory);
            foreach(string child in Directory.GetDirectories(directory))DeleteDirectory(child,false);
            foreach(string file in Directory.GetFiles(directory).OrderBy(p=>keepSettingsLast&&Path.GetFileName(p)=="settings.json"?1:0)){
                SafeFs.NoLinks(file);
                SafeFs.DeleteName(file);
                if(File.Exists(file))throw new IOException("Could not remove: "+file);
            }
            Directory.Delete(directory);
        }

        internal static void StartLauncherRemoval() {
            string exe=Program.SelfPath;
            SafeFs.NoLinks(exe);
            var current=new FileInfo(exe);
            string hash=SafeFs.Hash(exe);
            string stage=Path.Combine(Path.GetTempPath(),"gtavxr-remove-"+Ids.New());
            SafeFs.NoLinks(stage);
            Directory.CreateDirectory(stage);
            string helper=SafeFs.Under(stage,"GTAVXR.exe");
            SafeFs.CopyNew(exe,helper,hash,current.Length);
            using(var process=Process.GetCurrentProcess()){
                string arguments=ProcessText.Join(new[]{"--complete-launcher-removal",exe,process.Id.ToString(),
                    process.StartTime.ToUniversalTime().Ticks.ToString()});
                Process.Start(new ProcessStartInfo(helper,arguments){UseShellExecute=false,CreateNoWindow=true});
            }
        }

        internal static int FinishLauncherRemoval(string target,int pid,long ticks) {
            string stage=Path.GetDirectoryName(Program.SelfPath);
            string stageName=Path.GetFileName(stage);
            if(!stageName.StartsWith("gtavxr-remove-",StringComparison.Ordinal)||!Equal(Path.GetFileName(Program.SelfPath),"GTAVXR.exe")||
               !Equal(Path.GetDirectoryName(stage),Path.GetFullPath(Path.GetTempPath()).TrimEnd('\\')))
                throw new InvalidDataException("Invalid removal helper location.");
            var helper=new FileInfo(Program.SelfPath);
            string hash=SafeFs.Hash(helper.FullName);
            target=Path.GetFullPath(target);
            try {
                SafeFs.AssertFile(target,hash,helper.Length);
                try {
                    using(var process=Process.GetProcessById(pid)){
                        if(process.StartTime.ToUniversalTime().Ticks==ticks&&
                           (!Equal(process.MainModule.FileName,target)||!process.WaitForExit(60000)))
                            throw new IOException("The GTAVXR launcher is still running: "+target);
                    }
                }catch(ArgumentException){}
                RemoveLauncherFiles(target,hash,helper.Length,SelfInstall.InstalledExe(),
                    new[]{Environment.GetFolderPath(Environment.SpecialFolder.Programs),
                          Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory)});
                return 0;
            }catch(Exception error){
                bool spanish=System.Globalization.CultureInfo.CurrentUICulture.TwoLetterISOLanguageName=="es";
                string detail=spanish?"Queda un paso de limpieza. Si el launcher sigue disponible, vuelve a abrirlo y reintenta.":
                    "Cleanup is incomplete. If the launcher is still available, reopen it and retry.";
                System.Windows.MessageBox.Show(detail+"\n\n"+error.Message,"GTAVXR");
                return 1;
            }finally{StartHelperCleanup(stage,helper.FullName);}
        }

        internal static void RemoveLauncherFiles(string target,string hash,long bytes,string installed,IEnumerable<string> shortcutDirs) {
            // Remove the launcher that owns the current UI last, so a locked file can be retried from it.
            SafeFs.AssertFile(target,hash,bytes);
            if(!Equal(target,installed)&&File.Exists(installed)){
                if(!SelfInstall.IsPreviousLauncher(installed))SafeFs.AssertFile(installed,hash,bytes);
                SafeFs.DeleteName(installed);
                if(File.Exists(installed))throw new IOException("Could not remove: "+installed);
            }
            SelfInstall.RemoveOwnShortcuts(installed,shortcutDirs);
            SafeFs.AssertFile(target,hash,bytes);
            SafeFs.DeleteName(target);
            if(File.Exists(target))throw new IOException("Could not remove: "+target);
            string installedDirectory=Path.GetDirectoryName(installed);
            if(Directory.Exists(installedDirectory)&&!Directory.EnumerateFileSystemEntries(installedDirectory).Any())
                Directory.Delete(installedDirectory);
        }

        internal static void StartHelperCleanup(string stage,string helper) {
            const string cleanup="$helper=$env:GTAVXR_REMOVE_HELPER; $dir=$env:GTAVXR_REMOVE_DIR; "+
                "for($i=0;$i -lt 40 -and [IO.File]::Exists($helper);$i++){ "+
                "try{[IO.File]::Delete($helper)}catch{Start-Sleep -Milliseconds 250}}; "+
                "if(![IO.File]::Exists($helper)){[IO.Directory]::Delete($dir)}";
            var info=new ProcessStartInfo(Path.Combine(Environment.SystemDirectory,@"WindowsPowerShell\v1.0\powershell.exe"),
                ProcessText.Join(new[]{"-NoProfile","-NonInteractive","-Command",cleanup})){
                UseShellExecute=false,CreateNoWindow=true};
            info.EnvironmentVariables["GTAVXR_REMOVE_HELPER"]=helper;
            info.EnvironmentVariables["GTAVXR_REMOVE_DIR"]=stage;
            using(var process=Process.Start(info)){
                if(process==null)throw new IOException("Could not start launcher cleanup: "+stage);
            }
        }
    }
}
