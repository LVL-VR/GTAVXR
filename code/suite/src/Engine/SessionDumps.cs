using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace GtavxrSuite {
    internal static class SessionDumps {
        internal const string Name="gtavxr-crash.dmp";
        private const int Retained=3;

        internal static bool Prepare(string sessions,string currentId,string native){
            try {
                string current=SafeFs.Under(sessions,currentId+"/native");
                if(!String.Equals(native,current,StringComparison.OrdinalIgnoreCase))return false;
                SafeFs.NoLinks(current);
                if(!Directory.Exists(current)||File.Exists(SafeFs.Under(current,Name)))return false;
                var closed=new List<string>();
                foreach(string dir in Directory.EnumerateDirectories(sessions).OrderByDescending(Path.GetFileName,StringComparer.Ordinal)){
                    string id=Path.GetFileName(dir);
                    if(id==currentId)continue;
                    string file=SafeFs.Under(dir,"native/"+Name);
                    if(!File.Exists(file))continue;
                    SafeFs.NoLinks(file);
                    string journal=SafeFs.Under(dir,"session.json");
                    var record=Json.Read<SessionRecord>(journal);
                    if(record==null||record.schema!="gtavxr-suite2-session-v1"||record.id!=id||record.phase!="CLOSED")
                        return false;
                    closed.Add(file);
                }
                foreach(string file in closed.Skip(Retained-1))SafeFs.DeleteName(file);
                return true;
            }catch(InvalidDataException){return false;}
             catch(IOException){return false;}
             catch(UnauthorizedAccessException){return false;}
             catch(ArgumentException){return false;}
             catch(InvalidOperationException){return false;}
             catch(System.ComponentModel.Win32Exception){return false;}
        }
    }
}
