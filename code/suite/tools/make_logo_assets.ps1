param([Parameter(Mandatory=$true)][string]$Kit,[Parameter(Mandatory=$true)][string]$OutDir,[string]$GateRect='446,370,270,166')
# Suite pictures from the user's brand kit (GTAVXR_Visor_by_LVL_Brand_Kit_v1, extracted): the logo, the app icon and one
# gate picture per colour variant (the airplane window of the boarding pass, chosen per profile as "Imagen de perfil").
# Every variant is the same 1254 px canvas, so one rectangle cuts the same view of Los Santos through the visor from each.
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
Add-Type -AssemblyName PresentationCore,WindowsBase
if(Test-Path -LiteralPath $OutDir){throw 'OutDir must be fresh'}
[void][IO.Directory]::CreateDirectory($OutDir)
$code=@'
using System;
using System.IO;
using System.Text;
using System.Windows;
using System.Windows.Media;
using System.Windows.Media.Imaging;
public static class LogoKit {
    // Gate pictures: Suite id -> kit file (Original is the untreated art on the same transparent canvas).
    static readonly string[,] Gates={
        {"original","01_LOGO_PRINCIPAL/GTAVXR_Visor_Transparente_Canvas_Original.png"},
        {"midnight","02_VARIANTES/GTAVXR_Visor_Alternativa_Midnight_Transparente.png"},
        {"amber","02_VARIANTES/GTAVXR_Visor_Alternativa_Amber_Transparente.png"},
        {"noir","02_VARIANTES/GTAVXR_Visor_Alternativa_Noir_Transparente.png"},
        {"arctic","02_VARIANTES/GTAVXR_Visor_Alternativa_Arctic_Transparente.png"}};
    static readonly int[] IconSizes={16,32,48,64,128,256};
    public static string Run(string kit,string outDir,string gateRect){
        var sb=new StringBuilder();
        // The logo: the kit's trimmed transparent logo, scaled to 900 px wide (the Suite shows it at most 300 px tall).
        var logo=Load(Path.Combine(kit,"01_LOGO_PRINCIPAL/GTAVXR_Visor_Transparente_Recortado.png"));
        Save(Scale(logo,900),Path.Combine(outDir,"logo.png"));
        sb.AppendFormat("logo {0}x{1} -> 900 wide\n",logo.PixelWidth,logo.PixelHeight);
        // Gate pictures: the downtown towers above the letters, between the visor frame and the top of the X (the version
        // number is drawn over the window's lower left, so the cut keeps the letters out). Porthole aspect 206x128.
        var r=Array.ConvertAll(gateRect.Split(','),Int32.Parse);
        for(int i=0;i<Gates.GetLength(0);i++){
            var src=Load(Path.Combine(kit,Gates[i,1]));
            if(src.PixelWidth!=1254||src.PixelHeight!=1254)throw new InvalidDataException(Gates[i,1]+" is not the 1254 px canvas");
            var cut=new CroppedBitmap(src,new Int32Rect(r[0],r[1],r[2],r[3]));
            Save(Scale(cut,400),Path.Combine(outDir,"gate-"+Gates[i,0]+".png"));
            sb.AppendFormat("gate-{0} {1}\n",Gates[i,0],gateRect);
        }
        // App icon: the kit's transparent icons, drawn for each size, stored as PNG entries (Vista+ .ico).
        var frames=new byte[IconSizes.Length][];
        for(int i=0;i<IconSizes.Length;i++){
            string p=Path.Combine(kit,"04_ICONOS/GTAVXR_Icono_Transparente_"+IconSizes[i]+".png");
            var b=Load(p);if(b.PixelWidth!=IconSizes[i]||b.PixelHeight!=IconSizes[i])throw new InvalidDataException(p+" has the wrong size");
            frames[i]=File.ReadAllBytes(p);
        }
        using(var f=new FileStream(Path.Combine(outDir,"gtavxr.ico"),FileMode.CreateNew,FileAccess.Write))using(var w=new BinaryWriter(f)){
            w.Write((short)0);w.Write((short)1);w.Write((short)IconSizes.Length);
            int offset=6+16*IconSizes.Length;
            for(int i=0;i<IconSizes.Length;i++){
                w.Write((byte)(IconSizes[i]>=256?0:IconSizes[i]));w.Write((byte)(IconSizes[i]>=256?0:IconSizes[i]));w.Write((byte)0);w.Write((byte)0);
                w.Write((short)1);w.Write((short)32);w.Write(frames[i].Length);w.Write(offset);offset+=frames[i].Length;
            }
            foreach(var b in frames)w.Write(b);
        }
        sb.AppendFormat("icon {0}\n",String.Join(",",IconSizes));
        return sb.ToString();
    }
    static BitmapSource Load(string path){
        using(var fs=File.OpenRead(path)){var d=BitmapDecoder.Create(fs,BitmapCreateOptions.PreservePixelFormat,BitmapCacheOption.OnLoad);return new FormatConvertedBitmap(d.Frames[0],PixelFormats.Pbgra32,null,0);}
    }
    // High-quality (Fant) downscale to a target width.
    static BitmapSource Scale(BitmapSource src,int width){
        if(src.PixelWidth<=width)return src;
        int height=(int)Math.Round(src.PixelHeight*(double)width/src.PixelWidth);
        var dv=new DrawingVisual();RenderOptions.SetBitmapScalingMode(dv,BitmapScalingMode.Fant);
        using(var dc=dv.RenderOpen())dc.DrawImage(src,new Rect(0,0,width,height));
        var bmp=new RenderTargetBitmap(width,height,96,96,PixelFormats.Pbgra32);bmp.Render(dv);return bmp;
    }
    static void Save(BitmapSource b,string path){var e=new PngBitmapEncoder();e.Frames.Add(BitmapFrame.Create(b));using(var f=new FileStream(path,FileMode.CreateNew))e.Save(f);}
}
'@
Add-Type -TypeDefinition $code -ReferencedAssemblies PresentationCore,WindowsBase,System.Xaml
[LogoKit]::Run($Kit,$OutDir,$GateRect)
