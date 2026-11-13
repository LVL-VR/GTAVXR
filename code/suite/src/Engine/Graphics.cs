using System;
using System.Linq;
using System.Collections.Generic;

namespace GtavxrSuite {
    // The graphics choice of one profile, kept in that profile's vr-settings.ini [Graphics] (the file the EXE reads):
    //   upscale = off | quality | balanced | performance | ultra   (DLSS Super Resolution level; read when the game starts)
    //   neural = 0 | 1, neural_intensity = 0..100                  (DLSS neural; the VR menu also changes them live)
    //   preset = native | performance | neural | custom             (the Suite's own key; the EXE ignores it)
    // The Suite writes these keys only when the player changes them here, so what the VR menu saved during play stays.
    // Edits go through Ini (ScriptMods.cs): one key in one section, the file's own line ends and other lines kept.
    public sealed class GraphicsChoice {
        public string preset {get;set;}
        public string upscale {get;set;}
        public bool neural {get;set;}
        public int intensity {get;set;}
        public bool NeedsDlss(){return upscale!="off"||neural;}
    }
    public static class Graphics {
        public const string Section="Graphics";
        public static readonly string[] Presets={"native","performance","neural","custom"};
        public static readonly string[] Levels={"quality","balanced","performance","ultra"};
        public const string DefaultLevel="quality";
        public const int DefaultIntensity=50;

        public static GraphicsChoice Read(string ini){
            string up=(Ini.Get(ini,Section,"upscale")??"off").Trim().ToLowerInvariant();if(!Levels.Contains(up))up="off";
            int n;bool nr=Int32.TryParse(Ini.Get(ini,Section,"neural"),out n)&&n!=0;
            int i;if(!Int32.TryParse(Ini.Get(ini,Section,"neural_intensity"),out i))i=DefaultIntensity;i=Math.Max(0,Math.Min(100,i));
            var c=new GraphicsChoice{upscale=up,neural=nr,intensity=i};
            string p=(Ini.Get(ini,Section,"preset")??"").Trim().ToLowerInvariant();
            // a stored preset whose keys were changed afterwards (VR menu, by hand) reads as Personalizado
            c.preset=Presets.Contains(p)&&Matches(p,c)?p:(up=="off"&&!nr?"native":"custom");
            return c;
        }
        static bool Matches(string preset,GraphicsChoice c){
            if(preset=="native")return c.upscale=="off"&&!c.neural;
            if(preset=="performance")return c.upscale!="off"&&!c.neural;
            if(preset=="neural")return c.upscale!="off"&&c.neural;
            return true;
        }
        // The keys of `preset` (level / neural / intensity only where the preset leaves them to the player).
        public static GraphicsChoice For(string preset,GraphicsChoice current){
            string level=current!=null&&current.upscale!="off"?current.upscale:DefaultLevel;
            int intensity=current!=null?current.intensity:DefaultIntensity;
            switch(preset){
                case "native":return new GraphicsChoice{preset="native",upscale="off",neural=false,intensity=intensity};
                case "performance":return new GraphicsChoice{preset="performance",upscale=level,neural=false,intensity=intensity};
                case "neural":return new GraphicsChoice{preset="neural",upscale=level,neural=true,intensity=intensity};
                case "custom":return new GraphicsChoice{preset="custom",upscale=current==null?"off":current.upscale,neural=current!=null&&current.neural,intensity=intensity};
            }
            throw new ArgumentException("preset");
        }
        public static string Write(string ini,GraphicsChoice c){
            if(!Presets.Contains(c.preset)||(c.upscale!="off"&&!Levels.Contains(c.upscale))||c.intensity<0||c.intensity>100)throw new ArgumentException("graphics");
            ini=Ini.Set(ini,Section,"preset",c.preset);
            ini=Ini.Set(ini,Section,"upscale",c.upscale);
            ini=Ini.Set(ini,Section,"neural",c.neural?"1":"0");
            return Ini.Set(ini,Section,"neural_intensity",c.intensity.ToString());
        }
    }
}
