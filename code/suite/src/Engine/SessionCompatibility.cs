using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;

namespace GtavxrSuite {
    public sealed class PlayOptions {
        public List<string> OmittedMods=new List<string>();
        public string GraphicsFallback;
    }

    public sealed class OptionalComponentException : InvalidOperationException {
        public readonly PlayOptions Retry;
        public OptionalComponentException(IEnumerable<string> details,PlayOptions retry)
            :base(String.Join(Environment.NewLine,details)){Retry=retry;}
    }

    sealed class SessionGraphics {
        public GraphicsChoice Choice;
        public DlssFolder Folder;
        public string Fallback;
        public string Problem;
    }

    public sealed partial class PlaySession {
        List<ModManifest> CompatibleMods(Profile profile,VersionManifest version,PlayOptions options,
                                         List<string> diagnostics,List<string> omitted) {
            var checks=new ModCompatibility(env,RuntimeRoot(version));
            var result=new List<ModManifest>();
            var companions=new List<ModManifest>();
            foreach(string id in profile.mods??new List<string>()){
                if(options.OmittedMods.Contains(id)||result.Any(item=>item.id==id)) continue;
                var mod=lib.Mod(id);
                string issue=mod==null?"MOD_MISSING":CheckMod(mod,version,checks);
                var companion=mod==null||mod.companion==null?null:lib.Mod(mod.companion);
                if(issue==null&&mod.companion!=null){
                    if(options.OmittedMods.Contains(mod.companion)||companion==null) issue="MOD_COMPANION_MISSING";
                    else issue=CheckMod(companion,version,checks);
                }
                if(issue!=null){
                    omitted.Add(id);
                    diagnostics.Add((mod==null?id:mod.name)+" · "+issue);
                    continue;
                }
                result.Add(mod);
                if(companion!=null) companions.Add(companion);
            }
            // Preserve the established content merge order: explicit selections first, then script companions.
            foreach(var companion in companions)
                if(!result.Any(item=>item.id==companion.id)) result.Add(companion);
            var content=result.Where(mod=>mod.kind=="content-source").ToList();
            if(content.Count!=0){
                var plan=new ContentSources(env).Plan(profile,content);
                foreach(string id in plan.Skipped){
                    var mod=result.Single(item=>item.id==id);
                    result.Remove(mod);
                    omitted.Add(id);
                    diagnostics.Add(mod.name+" · MOD_CONTENT_DEPENDENCY_MISSING");
                    foreach(var owner in result.Where(item=>item.companion==id).ToList()){
                        result.Remove(owner);
                        omitted.Add(owner.id);
                        diagnostics.Add(owner.name+" · MOD_COMPANION_MISSING · "+mod.name);
                    }
                }
            }
            var occupied=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);
            foreach(var file in version.files) occupied[file.path]=version.title??version.id;
            foreach(var mod in result.Where(item=>item.kind!="script"&&item.kind!="content-source").ToList()){
                var files=mod.files??new List<FileRef>();
                var collision=files.FirstOrDefault(file=>occupied.ContainsKey(file.path));
                if(collision!=null){
                    result.Remove(mod);
                    omitted.Add(mod.id);
                    diagnostics.Add(mod.name+" · MOD_FILE_COLLISION · "+collision.path+" · "+occupied[collision.path]);
                    continue;
                }
                foreach(var file in files) occupied.Add(file.path,mod.name);
            }
            return result;
        }

        string CheckMod(ModManifest mod,VersionManifest version,ModCompatibility checks){
            if(!mod.WorksWith(version)) return mod.not_loadable_reason??"MOD_VERSION_CONTRACT";
            foreach(var file in mod.files??new List<FileRef>()){
                try { blobs.Verify(file.sha256,file.bytes); }
                catch(InvalidDataException) { return "MOD_FILE_CHANGED · "+file.path; }
                catch(IOException) { return "MOD_FILE_UNREADABLE · "+file.path; }
            }
            if(mod.kind=="content-source"){
                try { new ContentSources(env).Source(mod); }
                catch(InvalidDataException error) { return "MOD_CONTENT_FORMAT · "+error.Message; }
                catch(IOException error) { return "MOD_CONTENT_UNREADABLE · "+error.Message; }
                catch(System.Xml.XmlException error) { return "MOD_CONTENT_FORMAT · "+error.Message; }
                catch(UnauthorizedAccessException error) { return "MOD_CONTENT_UNREADABLE · "+error.Message; }
            }
            if(mod.kind!="script") return null;
            return PlatformCompatibility.ScriptHostProblem()??checks.Check(mod);
        }

        SessionGraphics CheckGraphics(Profile profile,VersionManifest version,PlayOptions options) {
            if(version.neural==null) return null;
            string path=lib.VrSettingsFile(profile.id);
            string ini=profile.has_vr_settings&&File.Exists(path)?File.ReadAllText(path):
                (version.vr_settings_seed==null?"":File.ReadAllText(blobs.PathOf(version.vr_settings_seed.sha256)));
            var choice=Graphics.Read(ini);
            var result=new SessionGraphics{Choice=choice,Fallback=options.GraphicsFallback};
            if(result.Fallback!=null) result.Choice=Graphics.For(result.Fallback,choice);
            if(!result.Choice.NeedsDlss()) return result;
            var settings=env.LoadSettings();
            result.Folder=Dlss.Find(settings.dlss_dir,env.Dir("dlss"),env.DlssSearchRoots==null?null:env.DlssSearchRoots());
            if(Dlss.Usable(result.Folder)&&result.Folder.dir.Length>=512){
                result.Problem="DLSS_PATH_TOO_LONG";
                result.Fallback="native";
            }else if(!Dlss.Usable(result.Folder)){
                result.Problem=result.Folder==null?"DLSS_MODEL_MISSING":result.Folder.sr.error;
                result.Fallback="native";
            }else if(result.Choice.neural){
                if(!Dlss.NeuralUsable(result.Folder)) result.Problem=result.Folder.nr==null?"DLSS_NEURAL_MODEL_MISSING":result.Folder.nr.error;
                else if(Ofxr.NeuralEnabled(env.OfxrRoot,env.OfxrLayer)) result.Problem="OFXR_NEURAL_ALREADY_ACTIVE";
                if(result.Problem!=null) result.Fallback=result.Choice.upscale=="off"?"native":"performance";
            }
            return result;
        }

        void StageGraphics(SessionRecord record,SessionGraphics graphics) {
            if(graphics==null) return;
            record.graphics=graphics.Choice;
            if(Dlss.Usable(graphics.Folder)) record.dlss_dir=graphics.Folder.dir;
            if(graphics.Fallback!=null){
                string ini=File.Exists(record.vr_settings)?File.ReadAllText(record.vr_settings):"";
                string original=SafeFs.Under(SessionDir(record.id),"before-graphics-fallback.ini");
                File.WriteAllText(original,ini,new UTF8Encoding(false));
                File.WriteAllText(record.vr_settings,Graphics.Write(ini,graphics.Choice),new UTF8Encoding(false));
            }
            Save(record);
        }

        string RestoreGraphicsChoice(SessionRecord record,string effective) {
            string path=SafeFs.Under(SessionDir(record.id),"before-graphics-fallback.ini");
            if(!File.Exists(path)) return effective;
            string original=File.ReadAllText(path);
            if(effective==Graphics.Write(original,record.graphics)) return original;
            foreach(string key in new[]{"preset","upscale","neural","neural_intensity"})
                effective=Ini.Set(effective,Graphics.Section,key,Ini.Get(original,Graphics.Section,key));
            return effective;
        }
    }
}
