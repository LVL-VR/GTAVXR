using System;
using System.Collections.Generic;
using System.Web.Script.Serialization;

namespace GtavxrSuite {
    // Validate standard JSON and duplicate keys before the framework maps into typed objects.
    // Case-insensitive duplicates are also refused because typed .NET property binding ignores case.
    internal sealed class StrictJson {
        private readonly string text;private int at;
        private StrictJson(string s){text=s;}
        public static void Validate(string s){if(s==null||s.Length>16*1024*1024)throw new FormatException("JSON size");var p=new StrictJson(s);p.Value(0);p.Space();if(p.at!=s.Length)throw new FormatException("Trailing JSON");}
        private void Space(){while(at<text.Length&&(text[at]==' '||text[at]=='\t'||text[at]=='\r'||text[at]=='\n'))at++;}
        private bool Take(char c){Space();if(at<text.Length&&text[at]==c){at++;return true;}return false;}
        private void Need(char c){if(!Take(c))throw new FormatException("Expected JSON "+c);}
        private string String(){Space();int start=at;if(at>=text.Length||text[at++]!='"')throw new FormatException("JSON string");
            bool ended=false;while(at<text.Length){char c=text[at++];if(c=='"'){ended=true;break;}if(c<32)throw new FormatException("Control in JSON string");if(c=='\\'){if(at>=text.Length)throw new FormatException("Truncated escape");char e=text[at++];if(e=='u'){for(int i=0;i<4;i++)if(at>=text.Length||!Uri.IsHexDigit(text[at++]))throw new FormatException("JSON unicode escape");}else if("\"\\/bfnrt".IndexOf(e)<0)throw new FormatException("Invalid escape");}}
            if(!ended)throw new FormatException("Unterminated string");return new JavaScriptSerializer().Deserialize<string>(text.Substring(start,at-start));}
        private void Value(int depth){Space();if(depth>64||at>=text.Length)throw new FormatException("JSON depth/end");char c=text[at];
            if(c=='{'){at++;var keys=new HashSet<string>(StringComparer.OrdinalIgnoreCase);if(Take('}'))return;do{if(!keys.Add(String()))throw new FormatException("Duplicate JSON key");Need(':');Value(depth+1);}while(Take(','));Need('}');return;}
            if(c=='['){at++;if(Take(']'))return;do{Value(depth+1);}while(Take(','));Need(']');return;}
            if(c=='"'){String();return;}
            foreach(string literal in new[]{"true","false","null"})if(at+literal.Length<=text.Length&&text.Substring(at,literal.Length)==literal){at+=literal.Length;return;}
            if(c=='-')at++;if(at>=text.Length)throw new FormatException("Number end");if(text[at]=='0')at++;else{if(text[at]<'1'||text[at]>'9')throw new FormatException("JSON value");while(at<text.Length&&text[at]>='0'&&text[at]<='9')at++;}
            if(at<text.Length&&text[at]=='.'){at++;Digits();}if(at<text.Length&&(text[at]=='e'||text[at]=='E')){at++;if(at<text.Length&&(text[at]=='+'||text[at]=='-'))at++;Digits();}
        }
        private void Digits(){int start=at;while(at<text.Length&&text[at]>='0'&&text[at]<='9')at++;if(at==start)throw new FormatException("JSON digits");}
    }
}
