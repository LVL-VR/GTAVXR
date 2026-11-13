using System;
using System.Collections.Generic;
using System.Text;

namespace GtavxrSuite.Content {
    // RAGE string hashes. atStringHash lower-cases (values), atLiteralStringHash does not (PSO member / struct names).
    public static class Joaat {
        public static uint Hash(string s){return Hash(s,true);}
        public static uint Hash(string s,bool lower){
            uint h=0;
            foreach(byte c in Encoding.UTF8.GetBytes(lower?s.ToLowerInvariant():s)){h+=c;h+=h<<10;h^=h>>6;}
            h+=h<<3;h^=h>>11;h+=h<<15;
            return h;
        }
        public static uint OfBytes(byte[] b){uint h=0;foreach(byte c in b){h+=c;h+=h<<10;h^=h>>6;}h+=h<<3;h^=h>>11;h+=h<<15;return h;}

        // A lower-case text whose joaat is the given hash, the way OpenIV writes 'hash_collision_<digits>' (port of
        // tools/mods/joaat_preimage.py): joaat's step and finalisation are invertible, meet in the middle over digits.
        const string Prefix="hash_collision_";
        static readonly uint Inv1025=Inverse(1025),Inv9=Inverse(9),Inv32769=Inverse(32769);
        static readonly Dictionary<uint,string> cache=new Dictionary<uint,string>();
        static uint Inverse(uint a){uint x=1;for(int i=0;i<5;i++)x*=2-a*x;return x;}  // Newton: a*x == 1 mod 2^32
        static uint Step(uint h,byte c){h+=c;h*=1025;return h^(h>>6);}
        static uint Unstep(uint h,byte c){uint x=h;for(int i=0;i<6;i++)x=h^(x>>6);return x*Inv1025-c;}
        static uint Unfinal(uint h){h*=Inv32769;h=h^(h>>11)^(h>>22);return h*Inv9;}
        static uint State(string s){uint h=0;foreach(byte c in Encoding.ASCII.GetBytes(s))h=Step(h,c);return h;}
        public static string Preimage(uint target){
            lock(cache){string hit;if(cache.TryGetValue(target,out hit))return hit;}
            const int half=5;
            foreach(int k in new[]{half,half+1}){
                uint start=State(Prefix);var fwd=new Dictionary<uint,string>();
                int limit=k==5?100000:1000000;
                for(int n=0;n<limit;n++){
                    string d=n.ToString().PadLeft(k,'0');uint h=start;
                    foreach(char c in d)h=Step(h,(byte)c);
                    if(!fwd.ContainsKey(h))fwd[h]=d;
                }
                uint goal=Unfinal(target);
                for(int n=0;n<100000;n++){
                    string d=n.ToString().PadLeft(half,'0');uint h=goal;
                    for(int i=d.Length-1;i>=0;i--)h=Unstep(h,(byte)d[i]);
                    string f;
                    if(fwd.TryGetValue(h,out f)){
                        string s=Prefix+f+d;
                        if(Hash(s,false)!=target)throw new InvalidOperationException("preimage check "+s);
                        lock(cache)cache[target]=s;
                        return s;
                    }
                }
            }
            throw new InvalidOperationException("no preimage for "+target.ToString("X8"));
        }
    }
}
