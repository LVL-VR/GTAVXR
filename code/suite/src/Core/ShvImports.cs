using System;
using System.IO;
using System.Linq;
using System.Text;
using System.Collections.Generic;

namespace GtavxrSuite {
    // 2.4.4 (user 2026-11-04 "adelante con los .asi y los mods shvdn que llaman a scripthookv.dll"): mods made for the store
    // ScriptHookV name its DLL inside their own files. GTAVXR's script host is GTAVXRHost.dll (user 2026-11-01: our host is not
    // called ScriptHookV), so the Suite's copy of each mod file says GTAVXRHost.dll instead; the player's original is never
    // touched. Two places hold the name:
    //  - a native .asi / .dll: its import and delay-import tables (and a bound-import entry, if any);
    //  - a .NET assembly (ScriptHookVDotNet scripts calling ScriptHookV.dll directly through DllImport): its ModuleRef rows.
    // The new name is shorter, so it is written in place and padded with zero bytes; nothing else moves. Only bytes are read:
    // nothing is loaded or run.
    // 2.4.5 (user 2026-11-04 "si opcion a"): a third place, the name as a whole text the mod looks the DLL up with while it runs
    // (GetModuleHandle / LoadLibrary("ScriptHookV.dll")): a zero-terminated ASCII or UTF-16 string that is exactly
    // "ScriptHookV.dll" or "ScriptHookV" (any case), and in a .NET assembly a user string (ldstr) that is exactly one of them.
    // "ScriptHookV" inside a longer text (a message, a path) is left as it is and still counted in runtime_mentions.
    public sealed class ShvRename {
        public bool pe {get;set;}               // a PE file at all
        public bool managed {get;set;}          // has a .NET metadata header
        public int native_imports {get;set;}    // import / delay-import / bound-import names renamed
        public int module_refs {get;set;}       // .NET ModuleRef names renamed
        public int name_strings {get;set;}      // 2.4.5: whole "ScriptHookV.dll" / "ScriptHookV" texts renamed (by-name lookups)
        public bool strong_named {get;set;}     // .NET strong-name signature (no longer matches the renamed copy)
        public int runtime_mentions {get;set;}  // "ScriptHookV" still in a longer text after renaming (not changed)
        public string error {get;set;}
        public bool Renamed{get{return native_imports+module_refs+name_strings>0;}}
    }
    public static class ShvImports {
        public const string HostDll="GTAVXRHost.dll";

        // A renamed copy of the file (the same array when nothing names ScriptHookV) and what was found.
        public static byte[] Rename(byte[] input,out ShvRename result){
            result=new ShvRename();var b=(byte[])input.Clone();
            try{
                var pe=Pe.Parse(b);if(pe==null)return input;
                result.pe=true;
                result.native_imports=RenameNative(b,pe);
                int mdFrom=-1,mdTo=-1;
                if(pe.Dir(14).Value!=0){
                    result.managed=true;int refs,users;bool signed;
                    RenameModuleRefs(b,pe,out refs,out signed,out users,out mdFrom,out mdTo);
                    result.module_refs=refs;result.strong_named=signed;result.name_strings=users;
                }
                // whole texts outside the .NET metadata (its #Strings heap holds type / member names, never touched)
                result.name_strings+=RenameStrings(b,mdFrom,mdTo);
            }catch(Exception e){result.error=e.GetType().Name+": "+e.Message;result.native_imports=result.module_refs=result.name_strings=0;return input;}
            result.runtime_mentions=Mentions(b);
            return result.Renamed?b:input;
        }
        public static bool IsShvName(string s){
            return s!=null&&(s.Equals("ScriptHookV.dll",StringComparison.OrdinalIgnoreCase)||s.Equals("ScriptHookV",StringComparison.OrdinalIgnoreCase));
        }
        static string NewName(string old){return old.EndsWith(".dll",StringComparison.OrdinalIgnoreCase)?HostDll:"GTAVXRHost";}
        // Writes the new name over the old one (both zero-terminated ASCII; the new one is never longer).
        static void Overwrite(byte[] b,int at,string old){
            byte[] n=Encoding.ASCII.GetBytes(NewName(old));
            if(n.Length>old.Length)throw new InvalidDataException("name longer");
            for(int i=0;i<old.Length;i++)b[at+i]=i<n.Length?n[i]:(byte)0;
        }
        static string Ascii(byte[] b,int at,int max=260){
            if(at<0||at>=b.Length)throw new InvalidDataException("string outside the file");
            int end=at;while(end<b.Length&&end-at<max&&b[end]!=0)end++;
            if(end>=b.Length||b[end]!=0)throw new InvalidDataException("unterminated string");
            return Encoding.ASCII.GetString(b,at,end-at);
        }

        static int RenameNative(byte[] b,Pe pe){
            int count=0;
            var imp=pe.Dir(1);
            if(imp.Key!=0){
                int at=pe.Offset(imp.Key);
                for(int i=0;;i++,at+=20){
                    if(at+20>b.Length)throw new InvalidDataException("import table outside the file");
                    if(Enumerable.Range(0,20).All(k=>b[at+k]==0))break;
                    uint nameRva=U32(b,at+12);if(nameRva==0)break;
                    int n=pe.Offset(nameRva);string name=Ascii(b,n);
                    if(IsShvName(name)){Overwrite(b,n,name);count++;}
                    if(i>4096)throw new InvalidDataException("import table too long");
                }
            }
            var delay=pe.Dir(13);
            if(delay.Key!=0){
                int at=pe.Offset(delay.Key);
                for(int i=0;;i++,at+=32){
                    if(at+32>b.Length)throw new InvalidDataException("delay import table outside the file");
                    if(Enumerable.Range(0,32).All(k=>b[at+k]==0))break;
                    uint attributes=U32(b,at),nameRef=U32(b,at+4);if(nameRef==0)break;
                    // attributes bit 0 clear = an old table that holds addresses, not RVAs
                    ulong rva=(attributes&1)!=0?nameRef:(ulong)nameRef-pe.ImageBase;
                    int n=pe.Offset((uint)rva);string name=Ascii(b,n);
                    if(IsShvName(name)){Overwrite(b,n,name);count++;}
                    if(i>4096)throw new InvalidDataException("delay import table too long");
                }
            }
            var bound=pe.Dir(11);
            if(bound.Key!=0){
                // bound imports sit at a file offset (not in a section); each entry: stamp, name offset (from the table start), forwarders
                int start=(int)bound.Key,at=start;
                for(int i=0;i<4096;i++){
                    if(at+8>b.Length)throw new InvalidDataException("bound import table outside the file");
                    uint stamp=U32(b,at);ushort nameOff=U16(b,at+4),forwarders=U16(b,at+6);
                    if(stamp==0&&nameOff==0&&forwarders==0)break;
                    string name=Ascii(b,start+nameOff);
                    if(IsShvName(name)){Overwrite(b,start+nameOff,name);count++;}
                    at+=8;
                }
            }
            return count;
        }

        // ECMA-335 II.24 / II.22: metadata root -> #~ stream -> table row sizes up to ModuleRef (0x1A) -> its Name column.
        static readonly int[] HasCustomAttribute={0x06,0x04,0x01,0x02,0x08,0x09,0x0A,0x00,0x0E,0x17,0x14,0x11,0x1A,0x1B,0x20,0x23,0x26,0x27,0x28,0x2A,0x2C,0x2B};
        internal static int[] ReferenceCounts(byte[] b){
            var pe=Pe.Parse(b);
            if(pe==null||pe.Dir(14).Key==0) throw new InvalidDataException("CLR_METADATA");
            int md=pe.Offset(U32(b,pe.Offset(pe.Dir(14).Key)+8));
            if(U32(b,md)!=0x424A5342) throw new InvalidDataException("CLR_METADATA");
            int at=checked(md+16+(int)U32(b,md+12));
            int streams=U16(b,at+2);at+=4;
            for(int i=0;i<streams;i++){
                string name=Ascii(b,at+8,32);
                if(name=="#~"||name=="#-"){
                    int table=checked(md+(int)U32(b,at));
                    ulong valid=U64(b,table+8);
                    int row=table+24;
                    var counts=new int[11];
                    for(int t=0;t<counts.Length;t++) if(((valid>>t)&1)!=0){
                        counts[t]=checked((int)U32(b,row));row+=4;
                    }
                    return new[]{counts[1],counts[10]};
                }
                at+=8+((name.Length+4)&~3);
            }
            throw new InvalidDataException("CLR_METADATA");
        }
        static void RenameModuleRefs(byte[] b,Pe pe,out int count,out bool signed,out int userStrings,out int mdFrom,out int mdTo){
            count=0;userStrings=0;
            int cli=pe.Offset(pe.Dir(14).Key);
            uint mdRva=U32(b,cli+8),mdSize=U32(b,cli+12);uint flags=U32(b,cli+16);signed=(flags&0x08)!=0;
            int md=pe.Offset(mdRva);mdFrom=md;mdTo=checked(md+(int)mdSize);
            if(U32(b,md)!=0x424A5342)throw new InvalidDataException("no metadata signature");
            int verLen=(int)U32(b,md+12);int at=md+16+verLen;
            int streams=U16(b,at+2);at+=4;
            int tables=-1,strings=-1,us=-1;uint stringsSize=0,usSize=0;
            for(int i=0;i<streams;i++){
                uint off=U32(b,at),streamSize=U32(b,at+4);string name=Ascii(b,at+8,32);
                if(name=="#~")tables=md+(int)off;else if(name=="#-")throw new InvalidDataException("uncompressed metadata (#-)");
                else if(name=="#Strings"){strings=md+(int)off;stringsSize=streamSize;}
                else if(name=="#US"){us=md+(int)off;usSize=streamSize;}
                at+=8+((name.Length+4)&~3);
            }
            if(us>=0)userStrings=RenameUserStrings(b,us,(int)usSize);
            if(tables<0||strings<0)throw new InvalidDataException("metadata streams missing");
            byte heap=b[tables+6];ulong valid=U64(b,tables+8);
            int s=(heap&1)!=0?4:2,g=(heap&2)!=0?4:2,bl=(heap&4)!=0?4:2;
            var rows=new uint[64];int p=tables+24;
            for(int t=0;t<64;t++)if(((valid>>t)&1)!=0){rows[t]=U32(b,p);p+=4;}
            if((heap&0x40)!=0)p+=4;
            Func<int,int> idx=t=>rows[t]<65536?2:4;
            Func<int[],int,int> coded=(ts,bits)=>{uint max=0;foreach(int t in ts)max=Math.Max(max,rows[t]);return max<(1u<<(16-bits))?2:4;};
            int resolution=coded(new[]{0x00,0x1A,0x23,0x01},2),typeDefOrRef=coded(new[]{0x02,0x01,0x1B},2),memberRefParent=coded(new[]{0x02,0x01,0x1A,0x06,0x1B},3);
            int hasConstant=coded(new[]{0x04,0x08,0x17},2),hasCa=coded(HasCustomAttribute,5),caType=coded(new[]{0x06,0x0A},3);
            int hasMarshal=coded(new[]{0x04,0x08},1),hasDecl=coded(new[]{0x02,0x06,0x20},2),hasSem=coded(new[]{0x14,0x17},1),methodDefOrRef=coded(new[]{0x06,0x0A},1);
            var size=new int[0x1B];
            size[0x00]=2+s+g+g+g;
            size[0x01]=resolution+s+s;
            size[0x02]=4+s+s+typeDefOrRef+idx(0x04)+idx(0x06);
            size[0x03]=idx(0x04);
            size[0x04]=2+s+bl;
            size[0x05]=idx(0x06);
            size[0x06]=4+2+2+s+bl+idx(0x08);
            size[0x07]=idx(0x08);
            size[0x08]=2+2+s;
            size[0x09]=idx(0x02)+typeDefOrRef;
            size[0x0A]=memberRefParent+s+bl;
            size[0x0B]=2+hasConstant+bl;
            size[0x0C]=hasCa+caType+bl;
            size[0x0D]=hasMarshal+bl;
            size[0x0E]=2+hasDecl+bl;
            size[0x0F]=2+4+idx(0x02);
            size[0x10]=4+idx(0x04);
            size[0x11]=bl;
            size[0x12]=idx(0x02)+idx(0x14);
            size[0x13]=idx(0x14);
            size[0x14]=2+s+typeDefOrRef;
            size[0x15]=idx(0x02)+idx(0x17);
            size[0x16]=idx(0x17);
            size[0x17]=2+s+bl;
            size[0x18]=2+idx(0x06)+hasSem;
            size[0x19]=idx(0x02)+methodDefOrRef+methodDefOrRef;
            size[0x1A]=s;
            for(int t=0;t<0x1A;t++)p+=checked((int)rows[t]*size[t]);
            var done=new HashSet<uint>();
            for(uint r=0;r<rows[0x1A];r++,p+=s){
                uint name=s==4?U32(b,p):U16(b,p);
                if(name>=stringsSize)throw new InvalidDataException("ModuleRef name outside #Strings");
                string text=Ascii(b,strings+(int)name);
                if(IsShvName(text)&&done.Add(name)){Overwrite(b,strings+(int)name,text);count++;}
            }
        }
        // 2.4.5: ECMA-335 II.24.2.4: #US holds blobs, each a compressed length, UTF-16 and one final byte. A blob that is exactly
        // "ScriptHookV.dll" / "ScriptHookV" gets the new name with its own shorter length, so string comparisons in the mod
        // match "GTAVXRHost.dll" exactly; the freed bytes become empty blobs (a single 0 each): the heap still reads blob by
        // blob and every ldstr token keeps its offset.
        static int RenameUserStrings(byte[] b,int us,int size){
            int n=0,end=checked(us+size),p=us;
            if(us<0||end>b.Length)throw new InvalidDataException("#US outside the file");
            while(p<end){
                int h=b[p],len,hdr;
                if((h&0x80)==0){len=h;hdr=1;}
                else if((h&0xC0)==0x80){if(p+1>=end)break;len=((h&0x3F)<<8)|b[p+1];hdr=2;}
                else if((h&0xE0)==0xC0){if(p+3>=end)break;len=((h&0x1F)<<24)|(b[p+1]<<16)|(b[p+2]<<8)|b[p+3];hdr=4;}
                else break;
                if(p+hdr+len>end)throw new InvalidDataException("#US blob outside the heap");
                if(hdr==1&&(len==2*15+1||len==2*11+1)){
                    string text=Encoding.Unicode.GetString(b,p+1,len-1);
                    if(IsShvName(text)){
                        byte[] nw=Encoding.Unicode.GetBytes(NewName(text));
                        b[p]=(byte)(nw.Length+1);
                        Array.Copy(nw,0,b,p+1,nw.Length);
                        b[p+1+nw.Length]=0;   // final byte 0: no character that needs special handling
                        for(int k=p+2+nw.Length;k<p+1+len;k++)b[k]=0;
                        n++;
                    }
                }
                p+=hdr+len;
            }
            return n;
        }
        // 2.4.5: whole zero-terminated texts "ScriptHookV.dll" / "ScriptHookV" (ASCII and UTF-16, any case) outside [skipFrom, skipTo)
        static int RenameStrings(byte[] b,int skipFrom,int skipTo){
            int n=0;
            foreach(string name in new[]{"ScriptHookV.dll","ScriptHookV"})
                n+=Whole(b,Encoding.ASCII.GetBytes(name),1,skipFrom,skipTo,name)+Whole(b,Encoding.Unicode.GetBytes(name),2,skipFrom,skipTo,name);
            return n;
        }
        // A string that is exactly pat: a zero code unit right before and right after it (part of a longer text is left alone).
        static int Whole(byte[] b,byte[] pat,int unit,int skipFrom,int skipTo,string old){
            byte[] nw=unit==1?Encoding.ASCII.GetBytes(NewName(old)):Encoding.Unicode.GetBytes(NewName(old));
            int n=0;
            for(int i=unit;i+pat.Length+unit<=b.Length;i++){
                if(i<skipTo&&i+pat.Length>skipFrom)continue;
                bool ok=true;
                for(int k=1;k<=unit&&ok;k++)ok=b[i-k]==0;
                for(int k=0;k<pat.Length&&ok;k++)ok=ToLower(b[i+k])==ToLower(pat[k]);
                for(int k=0;k<unit&&ok;k++)ok=b[i+pat.Length+k]==0;
                if(!ok)continue;
                for(int k=0;k<pat.Length;k++)b[i+k]=k<nw.Length?nw[k]:(byte)0;
                n++;i+=pat.Length-1;
            }
            return n;
        }
        // "ScriptHookV" left in the file (ASCII or UTF-16), not as part of "ScriptHookVDotNet": the mod may look it up by name.
        static int Mentions(byte[] b){
            int n=0;
            foreach(var pattern in new[]{Encoding.ASCII.GetBytes("ScriptHookV"),Encoding.Unicode.GetBytes("ScriptHookV")}){
                int unit=pattern.Length/11;
                for(int i=0;i+pattern.Length<=b.Length;i++){
                    int k=0;while(k<pattern.Length&&ToLower(b[i+k])==ToLower(pattern[k]))k++;
                    if(k<pattern.Length)continue;
                    int after=i+pattern.Length;
                    var next=Encoding.ASCII.GetBytes("DotNet");bool dotnet=true;
                    for(int j=0;j<next.Length;j++){int q=after+j*unit;if(q>=b.Length||ToLower(b[q])!=ToLower(next[j])){dotnet=false;break;}}
                    if(!dotnet)n++;
                    i=after-1;
                }
            }
            return n;
        }
        static byte ToLower(byte c){return c>=(byte)'A'&&c<=(byte)'Z'?(byte)(c+32):c;}
        static ushort U16(byte[] b,int at){if(at<0||at+2>b.Length)throw new InvalidDataException("read outside the file");return BitConverter.ToUInt16(b,at);}
        static uint U32(byte[] b,int at){if(at<0||at+4>b.Length)throw new InvalidDataException("read outside the file");return BitConverter.ToUInt32(b,at);}
        static ulong U64(byte[] b,int at){if(at<0||at+8>b.Length)throw new InvalidDataException("read outside the file");return BitConverter.ToUInt64(b,at);}

        sealed class Pe {
            public ulong ImageBase;int dirs,dirCount;readonly List<int[]> sections=new List<int[]>();byte[] b;
            public static Pe Parse(byte[] b){
                if(b.Length<0x40||b[0]!='M'||b[1]!='Z')return null;
                int nt=(int)U32(b,0x3C);if(nt<0||nt+24>b.Length||U32(b,nt)!=0x00004550)return null;
                var pe=new Pe{b=b};
                int nsec=U16(b,nt+6),optSize=U16(b,nt+20),opt=nt+24;ushort magic=U16(b,opt);
                if(magic==0x20B){pe.ImageBase=U64(b,opt+24);pe.dirCount=(int)U32(b,opt+108);pe.dirs=opt+112;}
                else if(magic==0x10B){pe.ImageBase=U32(b,opt+28);pe.dirCount=(int)U32(b,opt+92);pe.dirs=opt+96;}
                else return null;
                int sec=opt+optSize;
                for(int i=0;i<nsec;i++,sec+=40){
                    if(sec+40>b.Length)throw new InvalidDataException("section table outside the file");
                    pe.sections.Add(new[]{(int)U32(b,sec+12),(int)Math.Max(U32(b,sec+8),U32(b,sec+16)),(int)U32(b,sec+20),(int)U32(b,sec+16)});
                }
                return pe;
            }
            public KeyValuePair<uint,uint> Dir(int i){return i<dirCount?new KeyValuePair<uint,uint>(U32(b,dirs+i*8),U32(b,dirs+i*8+4)):new KeyValuePair<uint,uint>(0,0);}
            public int Offset(uint rva){
                foreach(var s in sections)if(rva>=(uint)s[0]&&rva<(uint)s[0]+(uint)s[1]){
                    uint inSection=rva-(uint)s[0];if(inSection>=(uint)s[3])throw new InvalidDataException("address in a section's uninitialised part");
                    return checked(s[2]+(int)inSection);}
                throw new InvalidDataException("address outside every section");
            }
        }
    }
}
