using System;
using System.Diagnostics;
using System.Text;

namespace GtavxrSuite {
    // Optional process that lives beside the game (the fault collector). Start after the game, close after it exits.
    public interface IProcessCompanion {
        void AfterStart(Process child,string executable);
        void AfterExit(Process child);
    }
    public static class Ids {
        // Store identities: letters, digits, '-' and '_' only, so they are safe as folder names.
        public static void Check(string s) {
            if(String.IsNullOrEmpty(s)||s.Length>80)throw new System.IO.InvalidDataException("Invalid identity");
            foreach(char c in s)if(!(Char.IsLetterOrDigit(c)||c=='-'||c=='_'))throw new System.IO.InvalidDataException("Invalid identity");
        }
        public static string New(){return Guid.NewGuid().ToString("N");}
    }
    public static class ProcessText {
        // Windows CommandLineToArgvW quoting (spaces, quotes, trailing backslashes).
        public static string Quote(string s) {
            if(s==null||s.IndexOf('\0')>=0||s.IndexOf('\r')>=0||s.IndexOf('\n')>=0)throw new ArgumentException("Invalid command argument");
            if(s.Length>0&&s.IndexOfAny(new[]{' ','\t','"'})<0)return s;
            var b=new StringBuilder("\"");int slashes=0;
            foreach(char c in s){
                if(c=='\\'){slashes++;continue;}
                if(c=='"')b.Append('\\',slashes*2+1);else b.Append('\\',slashes);
                b.Append(c);slashes=0;
            }
            b.Append('\\',slashes*2);b.Append('"');return b.ToString();
        }
        public static string Join(System.Collections.Generic.IEnumerable<string> args) {
            var b=new StringBuilder();
            foreach(string a in args){if(b.Length>0)b.Append(' ');b.Append(Quote(a));}
            return b.ToString();
        }
    }
}
