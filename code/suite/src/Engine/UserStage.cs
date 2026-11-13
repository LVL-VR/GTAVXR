using System;
using System.IO;
using System.Linq;
using System.Xml;
using System.Collections.Generic;

namespace GtavxrSuite {
    // Moves whole Rockstar user folders (saves, settings.xml, profiles) between places, with hashes.
    public static class UserTree {
        public static List<SaveFileRef> Snapshot(string dir){
            var list=new List<SaveFileRef>();
            if(!Directory.Exists(dir))return list;
            SafeFs.NoLinks(dir);
            foreach(string f in Directory.GetFiles(dir,"*",SearchOption.AllDirectories).OrderBy(x=>x,StringComparer.OrdinalIgnoreCase)){
                if((File.GetAttributes(f)&FileAttributes.ReparsePoint)!=0)throw new InvalidDataException("Link inside the user folder: "+f);
                string rel=f.Substring(dir.TrimEnd('\\').Length+1).Replace('\\','/');
                list.Add(new SaveFileRef{relative=SafeFs.Relative(rel),sha256=SafeFs.Hash(f),bytes=new FileInfo(f).Length});
            }
            return list;
        }
        // Copy every listed file from src into an EMPTY or absent dest, verifying each.
        public static void CopyInto(string src,List<SaveFileRef> files,string dest){
            Directory.CreateDirectory(dest);
            foreach(var f in files){
                string target=SafeFs.Under(dest,f.relative);
                if(File.Exists(target)){File.SetAttributes(target,FileAttributes.Normal);File.Delete(target);}
                SafeFs.CopyNew(SafeFs.Under(src,f.relative),target,f.sha256,f.bytes);
            }
        }
        // Make dest contain exactly the listed files from src (extra files removed, then empty folders).
        public static void Mirror(string src,List<SaveFileRef> files,string dest){
            Directory.CreateDirectory(dest);SafeFs.NoLinks(dest);
            var keep=new HashSet<string>(files.Select(f=>f.relative),StringComparer.OrdinalIgnoreCase);
            foreach(string f in Directory.GetFiles(dest,"*",SearchOption.AllDirectories)){
                string rel=f.Substring(dest.TrimEnd('\\').Length+1).Replace('\\','/');
                if(!keep.Contains(rel)){File.SetAttributes(f,FileAttributes.Normal);File.Delete(f);}
            }
            foreach(var f in files){
                string target=SafeFs.Under(dest,f.relative);
                if(File.Exists(target)&&new FileInfo(target).Length==f.bytes&&String.Equals(SafeFs.Hash(target),f.sha256,StringComparison.OrdinalIgnoreCase))continue;
                SafeFs.ReplaceFrom(SafeFs.Under(src,f.relative),target,f.sha256,f.bytes,0);
            }
            foreach(string d in Directory.GetDirectories(dest,"*",SearchOption.AllDirectories).OrderByDescending(x=>x.Length))
                if(!Directory.EnumerateFileSystemEntries(d).Any())Directory.Delete(d);
        }
        public static void Verify(string dir,List<SaveFileRef> files){
            foreach(var f in files)SafeFs.AssertFile(SafeFs.Under(dir,f.relative),f.sha256,f.bytes);
        }
    }

    // GTAVXR plays windowed 1920x1080 with Stereo=0 on the monitor; the headset gets its own eyes.
    // Same contract as the launcher's Convert-PlayDisplay: exactly these four fields, nothing else.
    public static class Display {
        public static string Convert(string xml){
            var settings=new XmlReaderSettings{DtdProcessing=DtdProcessing.Prohibit,XmlResolver=null};
            var doc=new XmlDocument{PreserveWhitespace=true,XmlResolver=null};
            using(var r=XmlReader.Create(new StringReader(xml),settings))doc.Load(r);
            var wanted=new Dictionary<string,int>{{"ScreenWidth",1920},{"ScreenHeight",1080},{"Windowed",1},{"Stereo",0}};
            foreach(var w in wanted){
                var nodes=doc.SelectNodes("/Settings/video/"+w.Key);
                if(nodes==null||nodes.Count!=1||!(nodes[0] is XmlElement)||!((XmlElement)nodes[0]).HasAttribute("value"))throw new InvalidDataException("Missing/duplicate display field: "+w.Key);
                var e=(XmlElement)nodes[0];int value;
                if(!Int32.TryParse(e.GetAttribute("value"),out value))throw new InvalidDataException("Invalid display value: "+w.Key);
                if(((w.Key=="ScreenWidth"||w.Key=="ScreenHeight")&&value<=0)||(w.Key=="Windowed"&&(value<0||value>2))||(w.Key=="Stereo"&&(value<0||value>1)))throw new InvalidDataException("Unsupported display value: "+w.Key);
                e.SetAttribute("value",w.Value.ToString(System.Globalization.CultureInfo.InvariantCulture));
            }
            return doc.OuterXml;
        }
    }
}
