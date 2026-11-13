using System;
using System.IO;
using System.Threading;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;

namespace GtavxrSuite {
    // Serializes cooperating Suite processes, not arbitrary launchers. No stale file deletion,
    // game admission, process termination or automatic recovery is authorized by this guard.
    public sealed class GameScope : IDisposable {
        [StructLayout(LayoutKind.Sequential)] private struct FileInfo {
            public uint attributes,creationLow,creationHigh,accessLow,accessHigh,writeLow,writeHigh;
            public uint volumeSerial,sizeHigh,sizeLow,links,indexHigh,indexLow;
        }
        [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)]
        private static extern SafeFileHandle CreateFileW(string path,uint access,uint share,IntPtr security,uint disposition,uint flags,IntPtr template);
        [DllImport("kernel32.dll",SetLastError=true)] private static extern bool GetFileInformationByHandle(SafeFileHandle handle,out FileInfo info);
        private static readonly object gate=new object();
        private static readonly HashSet<string> local=new HashSet<string>(StringComparer.Ordinal);
        private readonly Mutex mutex;private readonly SafeFileHandle directory;private readonly int ownerThread;
        private bool disposed;
        public string Name {get;private set;}
        public bool HadAbandonedOwner {get;private set;}
        private GameScope(string name,Mutex handle,SafeFileHandle folder,bool abandoned){Name=name;mutex=handle;directory=folder;HadAbandonedOwner=abandoned;ownerThread=Thread.CurrentThread.ManagedThreadId;}
        private static SafeFileHandle OpenDirectory(string root){
            root=SafeFs.Root(root);if(!Directory.Exists(root))throw new DirectoryNotFoundException("Existing game directory required");
            // No delete sharing: the selected directory cannot be renamed/replaced while owned.
            string path=root.Length>=248?@"\\?\"+root:root;
            var handle=CreateFileW(path,0x80,3,IntPtr.Zero,3,0x02000000,IntPtr.Zero);
            if(handle.IsInvalid){int error=Marshal.GetLastWin32Error();handle.Dispose();throw new System.ComponentModel.Win32Exception(error,"Cannot establish game directory ownership");}
            return handle;
        }
        private static string Identity(SafeFileHandle handle){
            FileInfo info;if(!GetFileInformationByHandle(handle,out info))throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());
            if((info.attributes&0x10)==0||(info.indexHigh==0&&info.indexLow==0))throw new IOException("Stable directory identity unavailable");
            // Physical file identity makes case, dot segments and 8.3 aliases the SAME scope.
            // A collision on another volume is conservatively busy, never a second owner.
            return @"Global\GTAVXR_Suite_Game_"+info.volumeSerial.ToString("X8")+"_"+info.indexHigh.ToString("X8")+info.indexLow.ToString("X8");
        }
        public static string NameFor(string root){using(var folder=OpenDirectory(root))return Identity(folder);}
        public static GameScope Acquire(string root){
            SafeFileHandle folder=OpenDirectory(root);Mutex handle=null;string name=null;bool registered=false,owned=false,abandoned=false;
            try{
                name=Identity(folder);
                lock(gate){if(!local.Add(name))throw new IOException("This process already owns or is acquiring this game");registered=true;}
                handle=new Mutex(false,name);
                try{owned=handle.WaitOne(0);}catch(AbandonedMutexException){owned=true;abandoned=true;}
                if(!owned)throw new IOException("Another Suite instance owns this game; no changes permitted");
                return new GameScope(name,handle,folder,abandoned);
            }catch{
                if(owned)handle.ReleaseMutex();if(handle!=null)handle.Dispose();folder.Dispose();
                if(registered)lock(gate)local.Remove(name);throw;
            }
        }
        public void AssertOwner(string root){
            if(disposed||Thread.CurrentThread.ManagedThreadId!=ownerThread)throw new InvalidOperationException("Game scope must be used by its owning thread");
            if(NameFor(root)!=Name)throw new InvalidDataException("Game scope does not belong to the selected directory");
        }
        public void Dispose(){
            if(disposed)return;
            if(Thread.CurrentThread.ManagedThreadId!=ownerThread)throw new InvalidOperationException("Only the owning thread may release the game scope");
            // Do not remove the process-local registration until the kernel ownership is released.
            mutex.ReleaseMutex();disposed=true;mutex.Dispose();directory.Dispose();lock(gate)local.Remove(Name);
        }
    }
}
