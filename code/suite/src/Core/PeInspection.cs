using System;
using System.IO;
using System.Text;
using System.Collections.Generic;

namespace GtavxrSuite {
    [Serializable]
    sealed class PeImport {
        public string Library;
        public string Name;
        public int Ordinal;
        public bool Delayed;
        public string Suffix;
    }

    // Inspect the tables through their raw file ranges. The DLL is never mapped or loaded.
    sealed class PeInspection : IDisposable {
        readonly BinaryReader reader;
        readonly uint[] directories=new uint[16];
        readonly List<uint[]> sections=new List<uint[]>();
        readonly ulong imageBase;
        readonly bool wide;
        public readonly ushort Machine;
        public readonly bool Managed;
        public readonly bool IlOnly;
        public readonly bool Requires32Bit;
        public readonly bool Authenticode;

        public PeInspection(string path) {
            reader=new BinaryReader(File.OpenRead(path));
            try {
                if(U16(0)!=0x5a4d) throw new InvalidDataException("PE_SIGNATURE");
                long pe=U32(60);
                if(U32(pe)!=0x4550) throw new InvalidDataException("PE_SIGNATURE");
                Machine=U16(pe+4);
                int count=U16(pe+6), optionalBytes=U16(pe+20);
                long optional=pe+24;
                ushort magic=U16(optional);
                if(magic!=0x10b&&magic!=0x20b) throw new InvalidDataException("PE_OPTIONAL_HEADER");
                wide=magic==0x20b;
                int offset=wide?112:96;
                if(count==0||count>96||optionalBytes<offset) throw new InvalidDataException("PE_HEADER_BOUNDS");
                imageBase=wide?U64(optional+24):U32(optional+28);
                uint dirs=U32(optional+offset-4);
                if(dirs>(optionalBytes-offset)/8) throw new InvalidDataException("PE_DIRECTORY_BOUNDS");
                for(int i=0;i<directories.Length&&i<dirs;i++) directories[i]=U32(optional+offset+i*8);
                for(int i=0;i<count;i++) {
                    long section=optional+optionalBytes+i*40L;
                    uint rva=U32(section+12), size=U32(section+16), raw=U32(section+20);
                    if(size!=0) At(raw,size);
                    sections.Add(new[]{rva,size,raw});
                }
                Authenticode=directories[4]!=0;
                Managed=directories[14]!=0;
                if(Managed){
                    uint flags=U32(Map(directories[14],20)+16);
                    IlOnly=(flags&1)!=0;
                    Requires32Bit=(flags&2)!=0&&(flags&0x20000)==0;
                }
            } catch { reader.Dispose(); throw; }
        }

        public HashSet<string> ExportNames() {
            var names=new HashSet<string>(StringComparer.Ordinal);
            ReadExports(names,null);
            return names;
        }

        public HashSet<int> ExportOrdinals() {
            var ordinals=new HashSet<int>();
            ReadExports(null,ordinals);
            return ordinals;
        }

        void ReadExports(HashSet<string> names,HashSet<int> ordinals) {
            if(directories[0]==0) return;
            long export=Map(directories[0],40);
            uint first=U32(export+16), count=U32(export+20), named=U32(export+24);
            if(count>65536||named>65536) throw new InvalidDataException("PE_EXPORT_LIMIT");
            long addresses=count==0?0:Map(U32(export+28),count*4);
            if(ordinals!=null) {
                for(uint i=0;i<count;i++)
                    if(U32(addresses+i*4L)!=0&&(ulong)first+i<=65535) ordinals.Add((int)(first+i));
            }
            if(names!=null&&named!=0) {
                long entries=Map(U32(export+32),named*4), indexes=Map(U32(export+36),named*2);
                for(uint i=0;i<named;i++) {
                    int index=U16(indexes+i*2L);
                    if(index>=count) throw new InvalidDataException("PE_EXPORT_INDEX");
                    if(U32(addresses+index*4L)!=0) names.Add(Name(U32(entries+i*4L)));
                }
            }
        }

        public List<PeImport> Imports() {
            var imports=new List<PeImport>();
            ReadImports(imports,false);
            ReadImports(imports,true);
            return imports;
        }

        void ReadImports(List<PeImport> imports,bool delay) {
            uint rva=directories[delay?13:1];
            if(rva==0) return;
            int stride=delay?32:20, pointer=wide?8:4;
            for(int i=0;i<4096;i++) {
                long entry=Map(checked(rva+(uint)(i*stride)),(uint)stride);
                uint reference=U32(entry+(delay?4:12));
                if(reference==0) return;
                bool addresses=delay&&(U32(entry)&1)==0;
                string library=Name(ToRva(reference,addresses));
                uint thunk=U32(entry+(delay?16:0));
                if(thunk==0) thunk=U32(entry+(delay?12:16));
                uint table=ToRva(thunk,addresses);
                bool terminated=false;
                for(int n=0;n<65536&&imports.Count<65536;n++) {
                    long item=Map(checked(table+(uint)(n*pointer)),(uint)pointer);
                    ulong value=wide?U64(item):U32(item);
                    if(value==0){terminated=true;break;}
                    bool ordinal=(value&(wide?1UL<<63:1UL<<31))!=0;
                    imports.Add(new PeImport{Library=library,Delayed=delay,Ordinal=ordinal?(int)(value&0xffff):0,
                                            Name=ordinal?null:Name(checked(ToRva(value,addresses)+2))});
                }
                if(!terminated) throw new InvalidDataException("PE_IMPORT_LIMIT");
            }
            throw new InvalidDataException("PE_IMPORT_LIMIT");
        }

        uint ToRva(ulong value,bool address) {
            if(address&&value<imageBase) throw new InvalidDataException("PE_IMPORT_ADDRESS");
            return checked((uint)(address?value-imageBase:value));
        }

        string Name(uint rva) {
            var text=new StringBuilder();
            for(uint i=0;i<512;i++) {
                At(Map(checked(rva+i),1),1);
                byte next=reader.ReadByte();
                if(next==0) return text.ToString();
                text.Append((char)next);
            }
            throw new InvalidDataException("PE_STRING_LIMIT");
        }

        long Map(uint rva,uint size) {
            foreach(var section in sections) {
                if(rva>=section[0]&&(ulong)(rva-section[0])+size<=section[1])
                    return section[2]+(long)(rva-section[0]);
            }
            throw new InvalidDataException("PE_RVA_BOUNDS");
        }

        void At(long offset,long size) {
            var stream=reader.BaseStream;
            if(offset<0||size<0||offset>stream.Length-size) throw new InvalidDataException("PE_FILE_BOUNDS");
            stream.Position=offset;
        }

        ushort U16(long offset){At(offset,2);return reader.ReadUInt16();}
        uint U32(long offset){At(offset,4);return reader.ReadUInt32();}
        ulong U64(long offset){At(offset,8);return reader.ReadUInt64();}
        public void Dispose(){reader.Dispose();}
    }
}
