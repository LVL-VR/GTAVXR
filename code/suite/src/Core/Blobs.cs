using System;
using System.IO;

namespace GtavxrSuite {
    public class FileRef { public string path {get;set;} public string sha256 {get;set;} public long bytes {get;set;} }

    // Content-addressed file store. Versions and mods keep each distinct file once,
    // so 125, 155 and later builds share their identical game data.
    public sealed class Blobs {
        private readonly string root;
        public Blobs(Env env){root=env.Dir("blobs");}
        public string PathOf(string sha){
            if(!SafeFs.IsHash(sha))throw new InvalidDataException("Blob hash required");
            sha=sha.ToUpperInvariant();
            return SafeFs.Under(root,sha.Substring(0,2)+"/"+sha);
        }
        public bool Has(string sha,long bytes){string p=PathOf(sha);return File.Exists(p)&&new FileInfo(p).Length==bytes;}
        // Copy a verified source into the store (no-op if present). Returns the stored path.
        public string Put(string source,string sha,long bytes){
            string dest=PathOf(sha);
            if(File.Exists(dest)){if(new FileInfo(dest).Length!=bytes)throw new InvalidDataException("Stored blob has the wrong size: "+sha);return dest;}
            SafeFs.AssertFile(source,sha,bytes);
            string tmp=dest+"."+Ids.New()+".tmp";
            SafeFs.CopyNew(source,tmp,sha,bytes);
            try{File.Move(tmp,dest);}catch(IOException){if(!File.Exists(dest))throw;File.Delete(tmp);}
            File.SetAttributes(dest,FileAttributes.ReadOnly);
            return dest;
        }
        public FileRef PutFile(string source,string relative){
            string sha=SafeFs.Hash(source);long bytes=new FileInfo(source).Length;Put(source,sha,bytes);
            return new FileRef{path=SafeFs.Relative(relative),sha256=sha,bytes=bytes};
        }
        // Full verification is explicit (Repair), never on every Play.
        public void Verify(string sha,long bytes){SafeFs.AssertFile(PathOf(sha),sha,bytes);}
        // Content profile files (gtavxr_content/): a second name for the stored file on the same disk instead of a copy
        // (World of Variety / V4EVER packs are hundreds of MB to GB; the game only reads them). Copy when a link is not possible.
        public void Link(FileRef f,string dest){
            SafeFs.NoLinks(dest);Directory.CreateDirectory(Path.GetDirectoryName(dest));
            string tmp=Path.Combine(Path.GetDirectoryName(dest),".gtavxr-"+Ids.New()+".tmp");
            if(!ContentSources.HardLink(PathOf(f.sha256),tmp)){Materialize(f,dest,true);return;}
            if(new FileInfo(tmp).Length!=f.bytes){File.Delete(tmp);throw new InvalidDataException("Stored file is damaged: "+f.path);}
            if(File.Exists(dest))SafeFs.DeleteName(dest);
            File.Move(tmp,dest);
        }
        // Where a GTAVXR file goes into the game: content profile files are links, everything else a checked copy.
        public void Place(FileRef f,string dest){if(f.path.StartsWith("gtavxr_content/",StringComparison.OrdinalIgnoreCase))Link(f,dest);else Materialize(f,dest);}
        // Write a blob to a destination through a temp file + atomic move, then check it.
        public void Materialize(FileRef f,string dest){Materialize(f,dest,false);}
        private void Materialize(FileRef f,string dest,bool protect){
            SafeFs.NoLinks(dest);Directory.CreateDirectory(Path.GetDirectoryName(dest));
            string tmp=Path.Combine(Path.GetDirectoryName(dest),".gtavxr-"+Ids.New()+".tmp");
            using(var s=File.OpenRead(PathOf(f.sha256)))using(var d=new FileStream(tmp,FileMode.CreateNew,FileAccess.Write,FileShare.None)){s.CopyTo(d);d.Flush(true);}
            if(new FileInfo(tmp).Length!=f.bytes||!String.Equals(SafeFs.Hash(tmp),f.sha256,StringComparison.OrdinalIgnoreCase)){File.Delete(tmp);throw new InvalidDataException("Stored file is damaged: "+f.path);}
            File.SetAttributes(tmp,protect?FileAttributes.ReadOnly:FileAttributes.Normal);
            if(protect&&File.Exists(dest))SafeFs.DeleteName(dest);
            if(File.Exists(dest)){File.SetAttributes(dest,FileAttributes.Normal);File.Replace(tmp,dest,null);}else File.Move(tmp,dest);
        }
    }
}
