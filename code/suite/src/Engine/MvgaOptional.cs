using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Text.RegularExpressions;
using System.Web.Script.Serialization;
using System.Xml;

namespace GtavxrSuite {
    static class MvgaOptional {
        sealed class Recipe {
            public string assembly { get; set; }
            public List<Job> jobs { get; set; }
        }

        sealed class Job : FileRef {
            public string member { get; set; }
            public string base_path { get; set; }
            public string input_sha256 { get; set; }
            public int input_bytes { get; set; }
            public string relative { get; set; }
        }

        internal static List<FileRef> TryImport(Env env, VersionManifest version, string path) {
            if(!File.Exists(path) || !path.EndsWith(".zip",StringComparison.OrdinalIgnoreCase)) return null;
            Recipe recipe;
            using(var resource=Assembly.GetExecutingAssembly().GetManifestResourceStream("GtavxrSuite.mvga-native-2-65.json")) {
                if(resource==null) return null;
                using(var reader=new StreamReader(resource)) recipe=new JavaScriptSerializer().Deserialize<Recipe>(reader.ReadToEnd());
            }
            SafeFs.NoLinks(path);
            if(new FileInfo(path).Length>256L*1024*1024) return null;
            var blobs=new Blobs(env);
            var files=new List<FileRef>();
            using(var outer=ZipFile.OpenRead(path)) {
                var candidates=outer.Entries.Where(entry=>String.Equals(entry.Name,Path.GetFileName(recipe.assembly),
                                                                       StringComparison.OrdinalIgnoreCase)).ToList();
                if(candidates.Count==0) return null;
                if(candidates.Count!=1 || outer.Entries.Count>4096) throw new InvalidDataException("MVGA_ARCHIVE_AMBIGUOUS");
                SafeFs.Relative(candidates[0].FullName);
                if(version==null) throw new InvalidDataException("INSTALL_REQUIRED");
                string scratch=env.Dir("scratch/mvga-"+Ids.New());
                var staged=new Dictionary<string,string>();
                try {
                    using(var oiv=new MemoryStream(ReadMember(outer,candidates[0].FullName,96*1024*1024,null)))
                    using(var inner=new ZipArchive(oiv,ZipArchiveMode.Read)) {
                        if(inner.Entries.Count>4096) throw new InvalidDataException("MVGA_ARCHIVE_AMBIGUOUS");
                        foreach(var job in recipe.jobs) {
                            if(job.base_path!=null) {
                                var file=version.files.SingleOrDefault(f=>f.path==job.base_path);
                                if(file==null || file.sha256!=job.sha256 || file.bytes!=job.bytes)
                                    throw new InvalidDataException("MVGA_BASE:"+job.base_path);
                                blobs.Verify(file.sha256,file.bytes);
                            } else {
                                var raw=ReadMember(inner,job.member,32*1024*1024,job.input_bytes);
                                using(var stream=new MemoryStream(raw))
                                    if(Hash(stream)!=job.input_sha256) throw new InvalidDataException("MVGA_INPUT:"+job.member);
                                string temp=SafeFs.Under(scratch,files.Count+".bin");
                                File.WriteAllBytes(temp,RepairXml(job.relative,raw));
                                SafeFs.AssertFile(temp,job.sha256,job.bytes);
                                staged.Add(job.path,temp);
                            }
                            files.Add(new FileRef { path=job.path, sha256=job.sha256, bytes=job.bytes });
                        }
                    }
                    foreach(var file in files.Where(file=>staged.ContainsKey(file.path)))
                        blobs.Put(staged[file.path],file.sha256,file.bytes);
                } finally { Directory.Delete(scratch,true); }
            }
            return files;
        }

        static string Hash(Stream stream) {
            using(var sha=System.Security.Cryptography.SHA256.Create())
                return BitConverter.ToString(sha.ComputeHash(stream)).Replace("-","");
        }

        static byte[] ReadMember(ZipArchive archive,string name,int max,int? length) {
            var matches=archive.Entries.Where(e=>String.Equals(e.FullName,name,StringComparison.OrdinalIgnoreCase)).ToList();
            if(matches.Count!=1 || matches[0].FullName!=name || matches[0].Length<=0 || matches[0].Length>max ||
               (length.HasValue && matches[0].Length!=length.Value)) throw new InvalidDataException("MVGA_MEMBER:"+name);
            var bytes=new byte[(int)matches[0].Length];
            using(var stream=matches[0].Open()) {
                for(int offset=0;offset<bytes.Length;) {
                    int read=stream.Read(bytes,offset,bytes.Length-offset);
                    if(read==0) throw new InvalidDataException("MVGA_MEMBER:"+name);
                    offset+=read;
                }
                if(stream.ReadByte()!=-1) throw new InvalidDataException("MVGA_MEMBER:"+name);
            }
            return bytes;
        }

        static byte[] RepairXml(string path,byte[] raw) {
            var utf8=new UTF8Encoding(false,true);
            if(path=="data/hbaosettings.xml") {
                string text=utf8.GetString(raw);
                int end=text.TrimEnd('\r','\n',' ','\t').Length;
                if(end<2 || text.Substring(end-2,2)!=">>") throw new InvalidDataException("MVGA_HBAO_XML");
                raw=utf8.GetBytes(text.Remove(end-1,1));
            } else if(path=="data/levels/gta5/weather.xml") {
                string text=utf8.GetString(raw);
                text=CloudName(text,"FOGGY","FOGGYclouds","MEDIUMclouds");
                text=CloudName(text,"RAIN","RAINclouds","HEAVYclouds");
                text=CloudName(text,"BLIZZARD","BLIZZARDclouds","SNOWclouds");
                text=CloudName(text,"SNOWLIGHT","SNOWclouds2","Default");
                raw=utf8.GetBytes(CloudName(text,"XMAS","XMASclouds","SNOWclouds"));
            }
            if(path.EndsWith(".xml",StringComparison.Ordinal)) {
                using(var stream=new MemoryStream(raw))
                using(var reader=XmlReader.Create(stream,new XmlReaderSettings { DtdProcessing=DtdProcessing.Prohibit, XmlResolver=null }))
                    while(reader.Read()) { }
            }
            return raw;
        }

        static string CloudName(string text,string weather,string oldName,string newName) {
            var pattern=new Regex(@"(<Item>\s*<Name>"+weather+@"</Name>.*?<CloudSettingsName>)"+oldName+
                                  @"(</CloudSettingsName>)",RegexOptions.Singleline,TimeSpan.FromSeconds(2));
            if(pattern.Matches(text).Count!=1) throw new InvalidDataException("MVGA_WEATHER_XML:"+weather);
            return pattern.Replace(text,match=>match.Groups[1].Value+newName+match.Groups[2].Value);
        }
    }
}
