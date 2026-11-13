using System;
using System.IO;
using System.Text;
using Microsoft.Win32;

namespace GtavxrSuite {
    static class PlatformCompatibility {
        // The integrated game's ANSI session path has a fixed 400-byte native buffer.
        internal static string SessionPathProblem(string root) {
            if(!PlaySession.AnsiPath(root)) return "DATA_PATH_NOT_ANSI";
            string example=Path.Combine(root,"sessions","20000101T000000-000000000000","native","activation-0001");
            if(Encoding.Default.GetByteCount(example)>=400) return "DATA_PATH_TOO_LONG";
            try { SafeFs.NoLinks(root); }
            catch(InvalidDataException) { return "DATA_PATH_NOT_LOCAL"; }
            if(Directory.Exists(root)&&(File.GetAttributes(root)&FileAttributes.Offline)!=0) return "DATA_PATH_NOT_LOCAL";
            return null;
        }

        internal static string ScriptHostProblem(int release,Func<string,bool> systemHas) {
            if(release<528040) return "HOST_NET48_REQUIRED";
            foreach(string dependency in new[]{"mscoree.dll","msvcp140.dll","vcruntime140.dll","ucrtbase.dll"})
                if(!systemHas(dependency)) return "HOST_SYSTEM_DEPENDENCY · "+dependency;
            return null;
        }

        internal static string ScriptHostProblem() {
            int release=0;
            using(var key=Registry.LocalMachine.OpenSubKey(@"SOFTWARE\Microsoft\NET Framework Setup\NDP\v4\Full")) {
                if(key!=null&&key.GetValue("Release") is int) release=(int)key.GetValue("Release");
            }
            return ScriptHostProblem(release,file=>File.Exists(Path.Combine(Environment.SystemDirectory,file)));
        }
    }
}
