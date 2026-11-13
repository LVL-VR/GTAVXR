using System;
using System.IO;
using System.Linq;
using System.Text;
using System.Diagnostics;
using System.Xml;
using System.Xml.Linq;
using System.Text.RegularExpressions;
using System.Runtime.InteropServices;
using System.Collections.Generic;
using GtavxrSuite.Content;

namespace GtavxrSuite {
    // Content mods the player imports (World of Variety, WoV Plus, V4EVER, any .oiv or loose DLC pack) and the content
    // profile the Suite builds from them when Play is pressed (Content/ContentBuilder: merged onto the player's own 3889
    // files, every merged file checked). One built profile at a time is in the game, always under the same id, so changing
    // an option only rewrites the files that changed.
    public sealed class ContentSources {
        public const string ProfileId="suite";
        // 2: gameconfig pools limited to what GTAVXR's EXE takes (ContentBuilder.ClampPools); a profile built by 1 is rebuilt once.
        // 3: merged .ymt whose root GTA reads only as binary PSO are served only to versions listing it in pso_xml.
        public const string Engine="content-engine-3";
        // GTA's game heap for a session that serves a content profile: World of Variety Plus pools do not fit the source's
        // 474 MB (Suite session 20261103T050457, "Error allocating ... in heap 0"). The script host v2 EXE reads it
        // (core/game_heap.h, 474..2000 MB); older EXEs ignore the argument.
        public const string HeapArg="-gtavxrHeapMB=1536";
        readonly Env env;readonly Library lib;readonly Blobs blobs;
        public ContentSources(Env e){env=e;lib=new Library(e);blobs=new Blobs(e);}

        [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)]static extern bool CreateHardLinkW(string link,string target,IntPtr sa);
        // A second name for a stored file (same disk); false = not possible here (the caller copies instead).
        public static bool HardLink(string target,string link){return CreateHardLinkW(link,target,IntPtr.Zero);}

        // ---------- import ----------
        // The content mods inside an archive (.zip / .rar / .7z through Windows tar), an .oiv, a loose DLC pack (<pack>/dlc.rpf)
        // or a folder holding any of them. The player's own files are only read (a pack is copied, never moved). Empty = none found.
        public List<ModManifest> Import(string path,Action<int,int> progress){
            SafeFs.NoLinks(path);
            string scratch=env.Dir("scratch/"+Ids.New());
            try{
                bool folder=Directory.Exists(path);
                var inputs=folder?Directory.GetFiles(path,"*",SearchOption.AllDirectories).ToList():new List<string>{path};
                SafeFs.Budget(env.DataRoot,inputs.Sum(f=>new FileInfo(f).Length)*(folder?2:4));
                var oivs=new List<string>();var packs=new List<KeyValuePair<string,string>>();
                Action<string,bool> take=(f,own)=>{   // own = a file in our scratch (may be moved)
                    SafeFs.NoLinks(f);
                    if(f.EndsWith(".oiv",StringComparison.OrdinalIgnoreCase)){oivs.Add(f);return;}
                    if(!String.Equals(Path.GetFileName(f),"dlc.rpf",StringComparison.OrdinalIgnoreCase))return;
                    string pack=Path.GetFileName(Path.GetDirectoryName(f));
                    if(!System.Text.RegularExpressions.Regex.IsMatch(pack??"","^[A-Za-z0-9_]{1,40}$"))return;
                    if(!own){string copy=Path.Combine(scratch,"packs",pack,"dlc.rpf");Directory.CreateDirectory(Path.GetDirectoryName(copy));File.Copy(f,copy);File.SetAttributes(copy,FileAttributes.Normal);f=copy;}
                    packs.Add(new KeyValuePair<string,string>(pack,f));
                };
                foreach(string f in inputs){
                    bool archive=Regex.IsMatch(f,@"\.(zip|rar|7z|tar|gz|tgz|xz)$",RegexOptions.IgnoreCase);
                    if(!archive){take(f,false);continue;}
                    string root=Path.Combine(scratch,"archive"+oivs.Count+"-"+packs.Count+"-"+Ids.New().Substring(0,6));Directory.CreateDirectory(root);
                    Untar(f,root);
                    foreach(string g in Directory.GetFiles(root,"*",SearchOption.AllDirectories))take(g,true);
                }
                var made=new List<ModManifest>();int i=0,n=oivs.Count+packs.Count;
                foreach(string oiv in oivs){
                    string dir=Path.Combine(scratch,"oiv"+(i++));Directory.CreateDirectory(dir);
                    Untar(oiv,dir);
                    if(!File.Exists(Path.Combine(dir,"assembly.xml")))continue;
                    var m=FromOiv(dir);if(m!=null)made.Add(m);
                    if(progress!=null)progress(i,n);
                }
                foreach(var p in packs){var m=FromPack(p.Key,p.Value);if(m!=null)made.Add(m);if(progress!=null)progress(++i,n);}
                return made;
            }finally{Wipe(scratch);}
        }
        // Scratch folders go away even when an archive marked its files read-only.
        internal static void Wipe(string dir){try{if(Directory.Exists(dir)){foreach(string f in Directory.GetFiles(dir,"*",SearchOption.AllDirectories))File.SetAttributes(f,FileAttributes.Normal);Directory.Delete(dir,true);}}catch(Exception){}}
        internal static void Untar(string archive,string dest){
            string tar=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.System),"tar.exe");
            if(!File.Exists(tar))throw new FileNotFoundException("Windows tar.exe not found");
            // bsdtar keeps every member inside dest (no absolute paths, no ..) unless -P is given.
            var psi=new ProcessStartInfo(tar,"-x -f "+ProcessText.Quote(archive)+" -C "+ProcessText.Quote(dest)){UseShellExecute=false,CreateNoWindow=true,RedirectStandardError=true,RedirectStandardOutput=true};
            using(var p=Process.Start(psi)){string err=p.StandardError.ReadToEnd();p.StandardOutput.ReadToEnd();p.WaitForExit();if(p.ExitCode!=0)throw new InvalidDataException("The archive could not be opened: "+err.Trim());}
            foreach(string d in Directory.GetDirectories(dest,"*",SearchOption.AllDirectories))SafeFs.NoLinks(d);
        }
        ModManifest FromOiv(string dir){
            XDocument doc;
            using(var r=XmlReader.Create(Path.Combine(dir,"assembly.xml"),new XmlReaderSettings{DtdProcessing=DtdProcessing.Prohibit,XmlResolver=null}))doc=XDocument.Load(r);
            if(doc.Root==null||doc.Root.Name.LocalName!="package")throw new InvalidDataException("Not an OIV package");
            var meta=doc.Root.Element("metadata");
            string name=meta==null?null:(string)meta.Element("name");name=(name??Path.GetFileName(dir)).Trim();
            var ver=meta==null?null:meta.Element("version");
            string version=ver==null?"":((string)ver.Element("major")??"")+"."+((string)ver.Element("minor")??"");
            var author=meta==null||meta.Element("author")==null?null:(string)meta.Element("author").Element("displayName");
            var src=ModSource.FromOiv("check",name,dir);
            foreach(var op in src.Ops)if(!File.Exists(op.Source))throw new InvalidDataException("The OIV lists a file it does not contain: "+op.Source.Substring(dir.Length+1));
            if(src.Ops.Count==0)return null;
            var cat=ContentCatalog.MatchOiv(name);
            return Store(dir,new ContentSourceInfo{format="oiv",catalog=cat==null?null:cat.id,option_of=cat==null?null:cat.option_of,author=author},cat!=null?cat.name:name,version,cat);
        }
        ModManifest FromPack(string pack,string file){
            string dir=Path.Combine(Path.GetDirectoryName(Path.GetDirectoryName(file)),".pack-"+Ids.New().Substring(0,8));
            string rel=Path.Combine(dir,"dlcpacks",pack);Directory.CreateDirectory(rel);File.Move(file,Path.Combine(rel,"dlc.rpf"));
            var cat=ContentCatalog.MatchPack(pack);
            return Store(dir,new ContentSourceInfo{format="pack",pack=pack,catalog=cat==null?null:cat.id},cat!=null?cat.name:pack,"",cat);
        }
        // An OpenIV folder (the game files a script mod ships: <archive>.rpf/<path>/<nested>.rpf/<file>, as OpenIV's mods folder
        // holds them) becomes a content source in the OIV layout: an assembly.xml with the nested archives + content/. The player's
        // files are only read. A file outside every archive is refused (no loose game files). Not a card: optionOf is its script mod.
        public ModManifest FromOpenIv(string folder,string name,string version,string optionOf){
            string root=SafeFs.Root(folder);SafeFs.NoLinks(root);
            string dir=env.Dir("scratch/"+Ids.New());
            try{
                string content=Path.Combine(dir,"content");Directory.CreateDirectory(content);
                var top=new XElement("content");var archives=new Dictionary<string,XElement>(StringComparer.OrdinalIgnoreCase);int i=0;
                foreach(string f in Directory.GetFiles(root,"*",SearchOption.AllDirectories).OrderBy(x=>x,StringComparer.OrdinalIgnoreCase)){
                    SafeFs.NoLinks(f);
                    var parts=f.Substring(root.TrimEnd('\\').Length+1).Replace('\\','/').Split('/');
                    int last=-1;for(int k=0;k<parts.Length-1;k++)if(parts[k].EndsWith(".rpf",StringComparison.OrdinalIgnoreCase))last=k;
                    if(last<0)throw new InvalidDataException("OPENIV_LOOSE_FILE: "+String.Join("/",parts));
                    // each .rpf segment opens the next archive level ("x64h.rpf" then "levels/gta5/interiors/v_int_40.rpf")
                    XElement node=top;string key="";int start=0;
                    for(int k=0;k<=last;k++){
                        if(!parts[k].EndsWith(".rpf",StringComparison.OrdinalIgnoreCase))continue;
                        string p=String.Join("/",parts,start,k-start+1);key+=(key.Length>0?"|":"")+p;start=k+1;
                        XElement a;if(!archives.TryGetValue(key,out a)){a=new XElement("archive",new XAttribute("path",p),new XAttribute("createIfNotExist","False"),new XAttribute("type","RPF7"));node.Add(a);archives[key]=a;}
                        node=a;
                    }
                    string src=(i++)+"_"+parts[parts.Length-1];
                    File.Copy(f,Path.Combine(content,src));File.SetAttributes(Path.Combine(content,src),FileAttributes.Normal);
                    node.Add(new XElement("add",new XAttribute("source",src),String.Join("/",parts,last+1,parts.Length-last-1)));
                }
                if(i==0)return null;
                var ver=(version??"").Split('.');
                var doc=new XDocument(new XElement("package",new XAttribute("version","2.2"),new XAttribute("target","Five"),
                    new XElement("metadata",new XElement("name",name),new XElement("version",new XElement("major",ver[0].Length>0?ver[0]:"1"),new XElement("minor",ver.Length>1?ver[1]:"0"))),top));
                doc.Save(Path.Combine(dir,"assembly.xml"));
                var check=ModSource.FromOiv("check",name,dir);
                if(check.Ops.Count!=i||check.Ops.Any(op=>!File.Exists(op.Source)))throw new InvalidDataException("OpenIV folder layout");
                return Store(dir,new ContentSourceInfo{format="oiv",option_of=optionOf},name,version??"",null);
            }finally{Wipe(dir);}
        }
        // Files move into the store (same disk: no second copy), the manifest lists them by their path inside the mod.
        ModManifest Store(string dir,ContentSourceInfo info,string name,string version,CatMod cat){
            var files=new List<FileRef>();
            foreach(string f in Directory.GetFiles(dir,"*",SearchOption.AllDirectories).OrderBy(x=>x,StringComparer.OrdinalIgnoreCase)){
                string rel=SafeFs.Relative(f.Substring(dir.Length+1).Replace('\\','/'));
                string sha=SafeFs.Hash(f);long bytes=new FileInfo(f).Length;
                string dest=blobs.PathOf(sha);
                if(!File.Exists(dest)){Directory.CreateDirectory(Path.GetDirectoryName(dest));File.SetAttributes(f,FileAttributes.Normal);File.Move(f,dest);File.SetAttributes(dest,FileAttributes.ReadOnly);}
                files.Add(new FileRef{path=rel,sha256=sha,bytes=bytes});
            }
            string family="content-src-"+(info.catalog??new string(name.ToLowerInvariant().Where(Char.IsLetterOrDigit).ToArray()));
            var same=lib.Mods().FirstOrDefault(m=>m.kind=="content-source"&&m.family==family&&m.files!=null&&m.files.Count==files.Count&&m.files.Zip(files,(a,b)=>a.path==b.path&&a.sha256==b.sha256).All(x=>x));
            if(same!=null)return same;
            var mod=new ModManifest{id="src-"+Ids.New().Substring(0,12),name=name,version=version,category=cat!=null&&cat.category!=null?cat.category:"map",added_utc=DateTime.UtcNow.ToString("o"),
                loadable=true,kind="content-source",files=files,args=new List<string>(),family=family,source=info};
            lib.SaveMod(mod);return mod;
        }

        // ---------- the old project-built packs ----------
        // Build 166 shipped World of Variety as three fixed packs made by the project (content-wov / -wov_solo / -wov_v4ever:
        // nothing inside could be turned off). Once the player's own mods are in the library each profile gets the same mods
        // as separate cards (Personalizar, V4EVER on its own) and the fixed pack leaves the library. A pack whose mods are not
        // all imported yet stays until they are.
        public List<string> ReplaceProjectPacks(){
            var done=new List<string>();var all=lib.Mods();
            Func<string,ModManifest> card=c=>all.Where(m=>m.kind=="content-source"&&m.source!=null&&m.source.catalog==c).OrderByDescending(m=>m.added_utc).FirstOrDefault();
            foreach(var old in all.Where(m=>m.kind=="content-native"&&m.built_key==null&&m.family!=null&&m.family.StartsWith("content-",StringComparison.Ordinal)).ToList()){
                var cats=new List<string>{"wov"};
                if(old.name.IndexOf("Plus",StringComparison.OrdinalIgnoreCase)>=0)cats.Add("wovplus");
                if(old.name.IndexOf("V4EVER",StringComparison.OrdinalIgnoreCase)>=0)cats.Add("v4ever");
                var cards=cats.Select(card).ToList();if(cards.Any(c=>c==null))continue;
                foreach(var p in lib.Profiles().Where(x=>x.mods!=null&&x.mods.Contains(old.id))){
                    int at=p.mods.IndexOf(old.id);p.mods.RemoveAt(at);
                    foreach(var c in cards)if(!p.mods.Contains(c.id))p.mods.Insert(at++,c.id);
                    lib.SaveProfile(p);
                }
                lib.DeleteMod(old.id);done.Add(old.name);
            }
            return done;
        }

        // ---------- sources for the builder ----------
        // The mod's files under their own paths, as hard links to the store (rebuilt if anything is missing).
        public string Tree(ModManifest m){
            string dir=SafeFs.Under(env.Dir("content-src"),m.id);
            bool ok=Directory.Exists(dir)&&m.files.All(f=>{string p=SafeFs.Under(dir,f.path);return File.Exists(p)&&new FileInfo(p).Length==f.bytes;});
            if(ok)return dir;
            if(Directory.Exists(dir)){foreach(string f in Directory.GetFiles(dir,"*",SearchOption.AllDirectories))SafeFs.DeleteName(f);Directory.Delete(dir,true);}
            foreach(var f in m.files){
                string dest=SafeFs.Under(dir,f.path);Directory.CreateDirectory(Path.GetDirectoryName(dest));
                if(!blobs.Has(f.sha256,f.bytes))throw new InvalidDataException("A stored file of "+m.name+" is missing: "+f.path+". Repair the library.");
                if(!HardLink(blobs.PathOf(f.sha256),dest))File.Copy(blobs.PathOf(f.sha256),dest);
            }
            return dir;
        }
        public ModSource Source(ModManifest m){
            string root=Tree(m);
            if(m.source!=null&&m.source.format=="pack")return ModSource.FromPack(m.id,m.name,m.source.pack,Path.Combine(root,"dlcpacks",m.source.pack,"dlc.rpf"));
            return ModSource.FromOiv(m.id,m.name,root);
        }

        // ---------- plan + build at Play ----------
        public ContentPlan Plan(Profile p,List<ModManifest> srcs){
            var all=lib.Mods();
            // Merge order: a mod that extends another (WoV Plus on World of Variety) right after it, whatever order the profile lists.
            srcs=new List<ModManifest>(srcs);
            foreach(var m in srcs.ToList()){
                var c=m.source==null?null:ContentCatalog.Mod(m.source.catalog);if(c==null||c.requires_mods==null)continue;
                srcs.Remove(m);
                int after=srcs.FindLastIndex(x=>x.source!=null&&c.requires_mods.Contains(x.source.catalog));
                srcs.Insert(after<0?srcs.Count:after+1,m);
            }
            return ContentCatalog.Plan(srcs.Select(m=>new KeyValuePair<string,string>(m.id,m.source==null?null:m.source.catalog)).ToList(),p.Option,
                c=>{var o=all.Where(x=>x.kind=="content-source"&&x.source!=null&&x.source.catalog==c).OrderByDescending(x=>x.added_utc).FirstOrDefault();return o==null?null:o.id;});
        }
        public string Key(ContentPlan plan,VersionManifest v){
            var sb=new StringBuilder(Engine).Append('\n').Append(plan.Canon).Append('\n');
            foreach(var kv in (v.retail??new Dictionary<string,string>()).OrderBy(x=>x.Key,StringComparer.Ordinal))sb.Append(kv.Key).Append('=').Append(kv.Value).Append('\n');
            foreach(string root in (v.pso_xml??new List<string>()).OrderBy(x=>x,StringComparer.Ordinal))sb.Append("pso_xml ").Append(root).Append('\n');
            foreach(string id in plan.Sources){var m=lib.Mod(id);foreach(var f in m.files)sb.Append(id).Append(' ').Append(f.path).Append(' ').Append(f.sha256).Append('\n');}
            using(var h=System.Security.Cryptography.SHA256.Create())return BitConverter.ToString(h.ComputeHash(Encoding.UTF8.GetBytes(sb.ToString()))).Replace("-","");
        }
        // The profile's content-source mods become one content-native mod (made once per distinct choice, then reused).
        public ModManifest Build(Profile p,VersionManifest v,List<ModManifest> srcs,Action<int,int> progress){
            var plan=Plan(p,srcs);if(plan.Sources.Count==0)return null;
            string key=Key(plan,v);string id="content-built-"+key.Substring(0,16).ToLowerInvariant();
            var have=lib.Mod(id);
            if(have!=null&&have.built_key==key&&have.files.All(f=>blobs.Has(f.sha256,f.bytes)))return have;
            string outDir=SafeFs.Under(env.Dir("content-build"),key.Substring(0,16)+"-"+Ids.New().Substring(0,8));
            try{
                var sources=plan.Sources.Select(x=>Source(lib.Mod(x))).ToList();
                plan.Options.PsoXmlRoots=new HashSet<string>(v.pso_xml??new List<string>(),StringComparer.Ordinal);
                // The builder reports names, merge, resources; Play shows one bar.
                string[] steps={"names","merge","resources"};
                var r=ContentBuilder.Build(env.GameDir,sources,plan.Options,ProfileId,outDir,(what,d,t)=>{int i=Array.IndexOf(steps,what);if(progress!=null&&i>=0&&t>0)progress(i*100+d*100/t,steps.Length*100);});
                // Files that are a source file as it is (packs, adopted resources) reuse that stored file.
                var bySource=new Dictionary<string,FileRef>(StringComparer.OrdinalIgnoreCase);
                foreach(string sid in plan.Sources){var m=lib.Mod(sid);string root=Tree(m);foreach(var f in m.files)bySource[Path.GetFullPath(SafeFs.Under(root,f.path))]=f;}
                var files=new List<FileRef>();string baseRel="gtavxr_content/"+ProfileId+"/";
                foreach(var f in r.Files){
                    string rel=baseRel+r.Mounts[f.Mount]+"/"+f.Relative;FileRef src;
                    if(bySource.TryGetValue(Path.GetFullPath(f.Source),out src))files.Add(new FileRef{path=rel,sha256=src.sha256,bytes=src.bytes});
                    else files.Add(blobs.PutFile(f.Source,rel));
                }
                files.Add(blobs.PutFile(Path.Combine(outDir,"profile","manifest.txt"),baseRel+"manifest.txt"));
                string reports=env.Dir("content-build/reports");
                File.Copy(Path.Combine(outDir,"build-report.json"),SafeFs.Under(reports,id+".json"),true);
                var names=plan.Sources.Select(x=>lib.Mod(x)).Where(x=>x.source==null||x.source.option_of==null).Select(x=>x.name).ToList();
                var mod=new ModManifest{id=id,name=String.Join(" + ",names),version=key.Substring(0,8),category="map",added_utc=DateTime.UtcNow.ToString("o"),loadable=true,
                    kind="content-native",files=files,args=new List<string>{"-gtavxrContent="+ProfileId},family="content-built",built_key=key};
                lib.SaveMod(mod);
                Prune(id);
                return mod;
            }finally{Wipe(outDir);}
        }
        // Built profiles kept: the newest few and the one installed in the game (a test flight may go back to it).
        void Prune(string keep){
            GameState st=null;try{st=new GameLayer(env).State();}catch(Exception){}
            var used=new HashSet<string>(st==null||st.mods==null?new List<string>():st.mods);used.Add(keep);
            foreach(var m in lib.Mods().Where(x=>x.built_key!=null).OrderByDescending(x=>x.added_utc).Skip(4))if(!used.Contains(m.id))lib.DeleteMod(m.id);
        }

        // ---------- what the profile stages besides (PlaySession) ----------
        // WoV's variety option: the multipliers in the profile's GTA settings.xml. "Game" leaves the game's own value (the
        // graphics slider only goes up to 1.0, so a value above it was written by this option and goes back to 1.0).
        public static string ApplySettings(string xml,Dictionary<string,string> values){
            foreach(string tag in new[]{"PedVarietyMultiplier","VehicleVarietyMultiplier"}){
                string v;bool set=values.TryGetValue(tag,out v);
                var re=new System.Text.RegularExpressions.Regex("(<"+tag+" value=\")([^\"]*)(\")");
                xml=re.Replace(xml,m=>{
                    double now;Double.TryParse(m.Groups[2].Value,System.Globalization.NumberStyles.Float,System.Globalization.CultureInfo.InvariantCulture,out now);
                    string nv=set?v:(now>1.0?"1.000000":m.Groups[2].Value);
                    return m.Groups[1].Value+nv+m.Groups[3].Value;
                });
            }
            return xml;
        }
    }

    // [VR] keys of a profile's vr-settings.ini the Suite sets as starting values (the VR menu keeps changing them in game).
    public static class VrIni {
        public static string Get(string text,string key){
            bool vr=false;
            foreach(var l in Lines(text)){var t=l.Trim();if(t.StartsWith("[",StringComparison.Ordinal)){vr=t.Equals("[VR]",StringComparison.OrdinalIgnoreCase);continue;}
                if(vr&&t.StartsWith(key+"=",StringComparison.OrdinalIgnoreCase))return t.Substring(key.Length+1).Trim();}
            return null;
        }
        public static string Set(string text,string key,string value){
            var lines=Lines(text);int vr=lines.FindIndex(l=>l.Trim().Equals("[VR]",StringComparison.OrdinalIgnoreCase));
            if(vr<0){lines.Insert(0,"[VR]");vr=0;}
            int end=lines.FindIndex(vr+1,l=>l.Trim().StartsWith("[",StringComparison.Ordinal));if(end<0)end=lines.Count;
            int at=lines.FindIndex(vr+1,end-vr-1,l=>l.Trim().StartsWith(key+"=",StringComparison.OrdinalIgnoreCase));
            if(at>=0)lines[at]=key+"="+value;else{int ins=end;while(ins>vr+1&&lines[ins-1].Trim().Length==0)ins--;lines.Insert(ins,key+"="+value);}
            return String.Join("\r\n",lines)+"\r\n";
        }
        static List<string> Lines(string text){var l=(text??"").Replace("\r\n","\n").Split('\n').ToList();while(l.Count>0&&l[l.Count-1].Length==0)l.RemoveAt(l.Count-1);return l;}
    }
}
