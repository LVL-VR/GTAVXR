using System;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Collections.Generic;
using GtavxrSuite.Content;

namespace GtavxrSuite {
    // Script mods (ScriptHookVDotNet: scripts/*.dll) for versions with the GTAVXR script host (work/SCRIPT_HOST_PLAN.md). The archive
    // is only read: its scripts/ files go to the store as the mod's script_files (placed in the game's scripts/ folder only while a
    // script host session runs, PlaySession.StageScripts), the game files it ships in an OpenIV folder become its companion content
    // source (ContentSources.FromOpenIv), merged into the content profile like any content mod. The ScriptHookVDotNet API a .dll uses
    // is read from its assembly references (bytes only: nothing is loaded or run).
    public sealed class ScriptMods {
        readonly Env env;readonly Library lib;readonly Blobs blobs;
        public ScriptMods(Env e){env=e;lib=new Library(e);blobs=new Blobs(e);}
        // A script host's own files are never a mod's (GTAVXR brings its own ScriptHookVDotNet build and loader).
        static readonly Regex HostFile=new Regex(@"^(ScriptHookVDotNet\d*\.(dll|asi|ini|xml|pdb|log)|ScriptHookV\.dll|dinput8\.dll)$",RegexOptions.IgnoreCase);

        // 2.4.4: an .asi mod's readme / licence / link files at its top level are not game files
        static readonly Regex Doc=new Regex(@"^(read ?me|l[eé]eme|changelog|change ?log|licen[cs]e|install(ation)?|instructions?|credits)[^/]*\.(txt|md|pdf|html?|rtf|docx?)$|\.url$",RegexOptions.IgnoreCase);
        // 2.4.4: folders beside an .asi that are not the mod's own files for the game folder: its ScriptHookVDotNet part (scripts/),
        // its game files for OpenIV, and the game's own folders (the Suite never writes into them through an .asi mod)
        static readonly Regex NotAsiFolder=new Regex(@"^(scripts|openiv|mods|update|x64|x64[a-z]|common|platform)$",RegexOptions.IgnoreCase);

        // The script mod inside a .zip / .rar / .7z, a folder or a single .asi; null = it holds no scripts/ folder with a .dll and no
        // .asi. 2.4.4: ScriptHookV .asi mods too (user 2026-11-04 "adelante con los .asi y los mods shvdn que llaman a
        // scripthookv.dll"): the folder that holds the shallowest .asi is the mod's game-folder part; every .asi / .dll in it and
        // every ScriptHookVDotNet script is stored as a copy that imports GTAVXRHost.dll instead of ScriptHookV.dll (ShvImports).
        public ModManifest Import(string path){
            SafeFs.NoLinks(path);
            bool folder=Directory.Exists(path);
            bool singleAsi=!folder&&path.EndsWith(".asi",StringComparison.OrdinalIgnoreCase)&&File.Exists(path);
            if(!folder&&!singleAsi&&!Regex.IsMatch(path,@"\.(zip|rar|7z)$",RegexOptions.IgnoreCase))return null;
            string scratch=env.Dir("scratch/"+Ids.New());
            try{
                string root=path;
                if(singleAsi){root=Path.Combine(scratch,"single");Directory.CreateDirectory(root);File.Copy(path,Path.Combine(root,Path.GetFileName(path)));}
                else if(!folder){SafeFs.Budget(env.DataRoot,new FileInfo(path).Length*4);root=Path.Combine(scratch,"archive");Directory.CreateDirectory(root);ContentSources.Untar(path,root);}
                string scripts=Shallowest(root,"scripts",d=>Directory.GetFiles(d,"*.dll").Length>0);
                string asiRoot=ShallowestAsi(root,scripts);
                if(scripts==null&&asiRoot==null)return null;
                var files=new List<FileRef>();var dlls=new List<string>();string api=null;bool sources=false;var byName=new List<string>();
                if(scripts!=null)foreach(string f in Directory.GetFiles(scripts,"*",SearchOption.AllDirectories).OrderBy(x=>x,StringComparer.OrdinalIgnoreCase)){
                    SafeFs.NoLinks(f);
                    string rel=f.Substring(scripts.TrimEnd('\\').Length+1).Replace('\\','/');
                    if(!rel.Contains("/")&&HostFile.IsMatch(rel))continue;
                    string ext=Path.GetExtension(f).ToLowerInvariant();
                    if(ext==".cs"||ext==".vb")sources=true;
                    if(ext==".dll"){
                        byte[] bytes=File.ReadAllBytes(f);
                        if(!rel.Contains("/"))dlls.Add(rel);api=Stronger(api,Api(bytes));
                        // a script that calls ScriptHookV.dll directly (DllImport): its copy calls GTAVXRHost.dll
                        files.Add(Renamed(f,bytes,rel,"scripts/"+rel,byName));continue;
                    }
                    files.Add(blobs.PutFile(f,rel));
                }
                var asiFiles=new List<FileRef>();bool asiWithoutShv=false;
                if(asiRoot!=null)foreach(string f in Directory.GetFiles(asiRoot,"*",SearchOption.AllDirectories).OrderBy(x=>x,StringComparer.OrdinalIgnoreCase)){
                    SafeFs.NoLinks(f);
                    string rel=f.Substring(asiRoot.TrimEnd('\\').Length+1).Replace('\\','/');
                    int slash=rel.IndexOf('/');
                    if(slash>0&&NotAsiFolder.IsMatch(rel.Substring(0,slash)))continue;
                    if(slash<0&&(HostFile.IsMatch(rel)||Doc.IsMatch(rel)||rel.EndsWith(".rpf",StringComparison.OrdinalIgnoreCase)))continue;
                    string ext=Path.GetExtension(f).ToLowerInvariant();
                    if(ext==".asi"||ext==".dll"){
                        byte[] bytes=File.ReadAllBytes(f);
                        ShvRename rn;ShvImports.Rename(bytes,out rn);
                        // an .asi that does not import ScriptHookV is a plugin for the store EXE (ASI loader): GTAVXR cannot run it
                        if(ext==".asi"&&!rn.Renamed)asiWithoutShv=true;
                        asiFiles.Add(Renamed(f,bytes,rel,rel,byName));continue;
                    }
                    asiFiles.Add(blobs.PutFile(f,rel));
                }
                bool hasAsi=asiFiles.Any(x=>x.path.EndsWith(".asi",StringComparison.OrdinalIgnoreCase));
                if(!hasAsi)asiFiles.Clear();
                if(files.Count==0&&!hasAsi)return null;
                var cat=ContentCatalog.MatchScript(dlls);
                string baseName=folder?Path.GetFileName(path.TrimEnd('\\')):Path.GetFileNameWithoutExtension(path);
                string name=cat!=null?cat.name:baseName.Replace('_',' ').Trim();
                var vm=Regex.Match(baseName,@"\d+(?:\.\d+)+");string version=vm.Success?vm.Value:"";
                string family="script-"+(cat!=null?cat.id:new string(name.ToLowerInvariant().Where(Char.IsLetterOrDigit).ToArray()));
                // 2.4.3: v3 mods load (the version must bring ScriptHookVDotNet 3: ModManifest.WorksWith)
                // 2.4.4: an .asi mod without ScriptHookVDotNet scripts is a script mod too
                string reason=api=="v1"?"SHVDN_V1_PENDING":sources?"SHVDN_SOURCE_PENDING":asiWithoutShv?"ASI_WITHOUT_SCRIPTHOOKV":api==null&&!hasAsi?"NO_SHVDN_SCRIPT":null;
                // Its game files: the OpenIV folder for the Legacy game (an Enhanced-only folder is not for this game).
                ModManifest companion=null;
                string openiv=Shallowest(root,"OpenIV",d=>true);
                if(openiv!=null){
                    string legacy=Directory.GetDirectories(openiv).FirstOrDefault(d=>String.Equals(Path.GetFileName(d),"Legacy",StringComparison.OrdinalIgnoreCase));
                    bool enhancedOnly=legacy==null&&Directory.GetDirectories(openiv).All(d=>String.Equals(Path.GetFileName(d),"Enhanced",StringComparison.OrdinalIgnoreCase))&&Directory.GetFiles(openiv).Length==0;
                    if(enhancedOnly)reason=reason??"LEGACY_FILES_MISSING";
                    else companion=new ContentSources(env).FromOpenIv(legacy??openiv,name+" · OpenIV",version,cat!=null?cat.id:family);
                }
                var same=lib.Mods().FirstOrDefault(m=>m.kind=="script"&&m.family==family&&SameFiles(m.script_files,files)&&SameFiles(m.asi_files,asiFiles)&&m.companion==(companion==null?null:companion.id));
                if(same!=null)return same;
                var mod=new ModManifest{id="scr-"+Ids.New().Substring(0,12),name=name,version=version,category=cat!=null&&cat.category!=null?cat.category:"other",added_utc=DateTime.UtcNow.ToString("o"),
                    loadable=reason==null,not_loadable_reason=reason,kind="script",files=new List<FileRef>(),script_files=files,script_api=api,args=new List<string>(),family=family,
                    asi_files=asiFiles.Count>0?asiFiles:null,shv_by_name=byName.Count>0?byName:null,
                    companion=companion==null?null:companion.id,source=new ContentSourceInfo{format="script",catalog=cat==null?null:cat.id}};
                lib.SaveMod(mod);
                return mod;
            }finally{ContentSources.Wipe(scratch);}
        }
        static bool SameFiles(List<FileRef> a,List<FileRef> b){
            a=a??new List<FileRef>();b=b??new List<FileRef>();
            return a.Count==b.Count&&a.Zip(b,(x,y)=>x.path==y.path&&x.sha256==y.sha256).All(x=>x);
        }
        // The store copy of an .asi / .dll: renamed when it names ScriptHookV.dll, the original bytes otherwise.
        FileRef Renamed(string file,byte[] bytes,string rel,string shown,List<string> byName){
            ShvRename rn;byte[] copy=ShvImports.Rename(bytes,out rn);
            if(rn.runtime_mentions>0)byName.Add(shown);
            if(!rn.Renamed)return blobs.PutFile(file,rel);
            using(var image=new PeInspection(file))
                if(rn.strong_named||image.Authenticode)return blobs.PutFile(file,rel);
            string tmp=Path.Combine(env.Dir("scratch"),Ids.New()+".bin");
            try{File.WriteAllBytes(tmp,copy);return blobs.PutFile(tmp,rel);}finally{if(File.Exists(tmp))File.Delete(tmp);}
        }
        // The folder nearest the top whose own files include an .asi (not inside the ScriptHookVDotNet scripts/ folder), or null.
        static string ShallowestAsi(string root,string scripts){
            var all=new List<string>{root};all.AddRange(Directory.GetDirectories(root,"*",SearchOption.AllDirectories));
            return all.Where(d=>(scripts==null||!(d.TrimEnd('\\')+"\\").StartsWith(scripts.TrimEnd('\\')+"\\",StringComparison.OrdinalIgnoreCase))&&
                    Directory.GetFiles(d,"*.asi").Any(f=>!HostFile.IsMatch(Path.GetFileName(f))))
                .OrderBy(d=>d.Count(c=>c=='\\')).ThenBy(d=>d,StringComparer.OrdinalIgnoreCase).FirstOrDefault();
        }
        // The folder with this name nearest the top (the archive root itself counts), or null.
        static string Shallowest(string root,string name,Func<string,bool> ok){
            var all=new List<string>{root};all.AddRange(Directory.GetDirectories(root,"*",SearchOption.AllDirectories));
            return all.Where(d=>String.Equals(Path.GetFileName(d.TrimEnd('\\')),name,StringComparison.OrdinalIgnoreCase)&&ok(d))
                .OrderBy(d=>d.Count(c=>c=='\\')).ThenBy(d=>d,StringComparer.OrdinalIgnoreCase).FirstOrDefault();
        }
        // Assembly reference names sit null-terminated in the metadata string heap.
        static string Api(byte[] dll){
            if(Has(dll,"ScriptHookVDotNet3"))return "v3";
            if(Has(dll,"ScriptHookVDotNet"))return "v1";
            if(Has(dll,"ScriptHookVDotNet2"))return "v2";
            return null;
        }
        static bool Has(byte[] d,string s){
            var n=Encoding.ASCII.GetBytes("\0"+s+"\0");
            for(int i=0;i+n.Length<=d.Length;i++){int k=0;while(k<n.Length&&d[i+k]==n[k])k++;if(k==n.Length)return true;}
            return false;
        }
        // One .dll the host cannot run makes the mod not loadable: v3 / v1 win over v2.
        static string Stronger(string a,string b){
            foreach(string x in new[]{"v3","v1","v2"})if(a==x||b==x)return x;
            return null;
        }

        // What goes into scripts/ for this profile: the mod's files, its settings file with the profile's options written in
        // (the catalog's ini keys; a value the catalog does not list falls back to the option's default).
        public List<FileRef> Placed(ModManifest m,Profile p){
            var list=new List<FileRef>(m.script_files??new List<FileRef>());
            var cat=m.source==null?null:ContentCatalog.Mod(m.source.catalog);
            if(cat==null||cat.ini_file==null)return list;
            int at=list.FindIndex(f=>String.Equals(f.path,cat.ini_file,StringComparison.OrdinalIgnoreCase));
            byte[] raw=at>=0?File.ReadAllBytes(blobs.PathOf(list[at].sha256)):new byte[0];
            bool bom=raw.Length>=3&&raw[0]==0xEF&&raw[1]==0xBB&&raw[2]==0xBF;
            string text=new UTF8Encoding(false).GetString(raw,bom?3:0,raw.Length-(bom?3:0));
            foreach(var st in ContentCatalog.States(cat,p.Option,"en")){
                var o=st.Option;if(o.ini==null||st.Value==null)continue;
                string v;
                if(o.type=="switch")v=st.Value=="on"?(o.ini_on??"1"):(o.ini_off??"0");
                else v=(o.values??new List<CatValue>()).Any(x=>x.id==st.Value)?st.Value:o.@default;
                if(v==null||v=="mod")continue;
                text=Ini.Set(text,cat.ini_section??"Settings",o.ini,v);
            }
            string tmp=Path.Combine(env.Dir("scratch"),Ids.New()+".ini");
            try{File.WriteAllBytes(tmp,new UTF8Encoding(bom).GetPreamble().Concat(new UTF8Encoding(false).GetBytes(text)).ToArray());
                var made=blobs.PutFile(tmp,at>=0?list[at].path:cat.ini_file);
                if(at>=0)list[at]=made;else list.Add(made);
            }finally{if(File.Exists(tmp))File.Delete(tmp);}
            return list;
        }
    }

    // key=value settings files of script mods: one key set inside one section, the file's own line ends and other lines kept.
    public static class Ini {
        public static string Set(string text,string section,string key,string value){
            string eol=text.Contains("\r\n")?"\r\n":"\n";bool endsEol=text.EndsWith("\n",StringComparison.Ordinal);
            var lines=text.Length==0?new List<string>():text.Replace("\r\n","\n").Split('\n').ToList();
            if(endsEol)lines.RemoveAt(lines.Count-1);
            int sec=lines.FindIndex(l=>l.Trim().Equals("["+section+"]",StringComparison.OrdinalIgnoreCase));
            if(sec<0&&value==null)return text;
            if(sec<0){lines.Add("["+section+"]");sec=lines.Count-1;}
            int end=lines.FindIndex(sec+1,l=>l.Trim().StartsWith("[",StringComparison.Ordinal));if(end<0)end=lines.Count;
            int at=-1;
            for(int i=sec+1;i<end;i++){int eq=lines[i].IndexOf('=');if(eq>0&&lines[i].Substring(0,eq).Trim().Equals(key,StringComparison.OrdinalIgnoreCase)){at=i;break;}}
            if(value==null){if(at>=0)lines.RemoveAt(at);}
            else if(at>=0)lines[at]=key+"="+value;
            else{int ins=end;while(ins>sec+1&&lines[ins-1].Trim().Length==0)ins--;lines.Insert(ins,key+"="+value);}
            return String.Join(eol,lines)+(endsEol?eol:"");
        }
        public static string Get(string text,string section,string key){
            bool inside=false;
            foreach(var l in (text??"").Replace("\r\n","\n").Split('\n')){
                var t=l.Trim();if(t.StartsWith("[",StringComparison.Ordinal)){inside=t.Equals("["+section+"]",StringComparison.OrdinalIgnoreCase);continue;}
                int eq=t.IndexOf('=');if(inside&&eq>0&&t.Substring(0,eq).Trim().Equals(key,StringComparison.OrdinalIgnoreCase))return t.Substring(eq+1).Trim();
            }
            return null;
        }
    }
}
