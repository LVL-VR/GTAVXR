using System;
using System.Collections.Generic;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Shapes;
using System.Windows.Media.Animation;
using System.Windows.Media.Imaging;

namespace GtavxrSuite {
    // Small builders so the views read like layout, not boilerplate.
    public static class U {
        public static TextBlock Text(string s,double size,Color c,bool sign=false,FontWeight? w=null){
            return new TextBlock{Text=s??"",FontSize=size,Foreground=Theme.B(c),FontFamily=sign?Theme.Sign_:Theme.Body,FontWeight=w??FontWeights.Normal,TextWrapping=TextWrapping.Wrap};
        }
        public static TextBlock Label(string s,Color c){
            var t=Text(s.ToUpperInvariant(),11,c,true,FontWeights.SemiBold);t.TextWrapping=TextWrapping.NoWrap;
            t.Margin=new Thickness(0,0,0,2);return Spaced(t,1.6);
        }
        static TextBlock Spaced(TextBlock t,double em){
            // WPF has no letter-spacing: widen the run with a thin space after each letter.
            string s=t.Text;var b=new System.Text.StringBuilder();foreach(char ch in s){b.Append(ch);if(ch!=' ')b.Append(' ');}t.Text=b.ToString();return t;
        }
        public static Button Btn(string text,string style,Action click){
            var b=new Button{Content=text,Style=(Style)Application.Current.Resources[style]};
            if(click!=null)b.Click+=(s,e)=>click();return b;
        }
        public static StackPanel Row(double gap,params UIElement[] items){
            var p=new StackPanel{Orientation=Orientation.Horizontal};
            for(int i=0;i<items.Length;i++){var fe=items[i] as FrameworkElement;if(fe!=null&&i>0)fe.Margin=new Thickness(gap,fe.Margin.Top,fe.Margin.Right,fe.Margin.Bottom);p.Children.Add(items[i]);}
            return p;
        }
        public static StackPanel Col(double gap,params UIElement[] items){
            var p=new StackPanel();
            for(int i=0;i<items.Length;i++){var fe=items[i] as FrameworkElement;if(fe!=null&&i>0)fe.Margin=new Thickness(fe.Margin.Left,gap,fe.Margin.Right,fe.Margin.Bottom);p.Children.Add(items[i]);}
            return p;
        }
        public static Border Box(UIElement child,Color bg,Color border,double radius=10,double pad=16){
            return new Border{Child=child,Background=Theme.B(bg),BorderBrush=Theme.B(border),BorderThickness=new Thickness(1),CornerRadius=new CornerRadius(radius),Padding=new Thickness(pad)};
        }
        public static Ellipse Dot(Color c,double d=8){return new Ellipse{Width=d,Height=d,Fill=Theme.B(c),VerticalAlignment=VerticalAlignment.Center};}
        public static Border Chip(string text,Color dot){
            var p=Row(7,Dot(dot,7),Text(text,12,Theme.Text));
            return new Border{Child=p,BorderBrush=Theme.B(Theme.Line),BorderThickness=new Thickness(1),CornerRadius=new CornerRadius(999),Padding=new Thickness(10,4,11,4),Margin=new Thickness(0,0,8,0)};
        }
        public static Border Pill(string text,Color fg,Color bg){
            var t=Text(text,12,fg,true,FontWeights.SemiBold);t.TextWrapping=TextWrapping.NoWrap;
            return new Border{Child=t,Background=Theme.B(bg),CornerRadius=new CornerRadius(999),Padding=new Thickness(9,2,9,3),HorizontalAlignment=HorizontalAlignment.Left};
        }
        public static FrameworkElement Glyph(double size,Color bg,Color fg){
            // A plane leaving: the terminal's departure pictogram.
            var path=new Path{Fill=Theme.B(fg),Stretch=Stretch.Uniform,Width=size*0.66,Height=size*0.66,
                Data=Geometry.Parse("M2,11.5 L8.5,9.9 L13,3 L15,3 L12.8,9.4 L17,8.4 L18,9.8 L13.1,12.2 L15,16 L13.2,16 L9.8,12.4 L4,14 Z")};
            return new Border{Width=size,Height=size,CornerRadius=new CornerRadius(size*0.18),Background=Theme.B(bg),Child=path};
        }
        public const string PlanePath="M2,11.5 L8.5,9.9 L13,3 L15,3 L12.8,9.4 L17,8.4 L18,9.8 L13.1,12.2 L15,16 L13.2,16 L9.8,12.4 L4,14 Z";
        public static Image Img(BitmapSource src,double height){
            var i=new Image{Source=src,Height=height,Stretch=Stretch.Uniform,SnapsToDevicePixels=true};
            RenderOptions.SetBitmapScalingMode(i,BitmapScalingMode.HighQuality);return i;
        }
        // An airplane window looking out on Los Santos (from the logo's visor), with a chrome rim.
        public static FrameworkElement Porthole(BitmapSource view,double w,double h){
            var img=new Image{Source=view,Stretch=Stretch.UniformToFill,HorizontalAlignment=HorizontalAlignment.Center,VerticalAlignment=VerticalAlignment.Center};
            RenderOptions.SetBitmapScalingMode(img,BitmapScalingMode.HighQuality);
            double r=Math.Min(w,h)*0.42;
            var glass=new Border{CornerRadius=new CornerRadius(r),ClipToBounds=true,Child=img,Background=Theme.B(Theme.SkyDeep)};
            glass.Clip=new RectangleGeometry(new Rect(0,0,w-8,h-8),r-4,r-4);
            // Reflection across the glass.
            var shine=new Border{CornerRadius=new CornerRadius(r-4),IsHitTestVisible=false,
                Background=new LinearGradientBrush(Color.FromArgb(70,255,255,255),Color.FromArgb(0,255,255,255),new Point(0,0),new Point(0.7,0.6))};
            var g=new Grid{Width=w-8,Height=h-8};g.Children.Add(glass);g.Children.Add(shine);
            var rim=new Border{Width=w,Height=h,CornerRadius=new CornerRadius(r+4),Background=Theme.ChromeBrush(),Padding=new Thickness(3),
                BorderBrush=Brushes.Black,BorderThickness=new Thickness(1.5),Child=new Border{CornerRadius=new CornerRadius(r),Background=Brushes.Black,Padding=new Thickness(1),Child=g}};
            return rim;
        }
        // Black-and-yellow tape for test flights, with an optional label.
        public static FrameworkElement Tape(string text,double height){
            var g=new Grid{Height=height,Background=Theme.HazardBrush(height*0.9)};
            if(!String.IsNullOrEmpty(text)){
                var t=new TextBlock{Text=text,FontFamily=Theme.Sign_,FontWeight=FontWeights.Bold,FontSize=height*0.55,Foreground=Theme.B(Theme.Ink),VerticalAlignment=VerticalAlignment.Center};
                g.Children.Add(new Border{Background=Theme.B(Theme.Hazard),Padding=new Thickness(10,0,10,0),HorizontalAlignment=HorizontalAlignment.Left,Margin=new Thickness(height*1.4,0,0,0),
                    BorderBrush=Theme.B(Theme.Ink),BorderThickness=new Thickness(1.5,0,1.5,0),Child=t});
            }
            return g;
        }
        // The green V of the logo, as a badge (small icon sizes and the title bar fallback).
        public static FrameworkElement VBadge(double size){
            var v=Geometry.Parse("M1,2 L7.4,2 L10,12.2 L12.6,2 L19,2 L12.9,18.4 L7.1,18.4 Z");
            var fill=new LinearGradientBrush(Theme.C("#7BE04A"),Theme.C("#2C7F14"),new Point(0,0),new Point(0.4,1));
            var path=new Path{Data=v,Fill=fill,Stroke=Brushes.White,StrokeThickness=1.3,StrokeLineJoin=PenLineJoin.Round,Stretch=Stretch.Uniform,Margin=new Thickness(size*0.16)};
            var under=new Path{Data=v,Stroke=Brushes.Black,StrokeThickness=3.2,StrokeLineJoin=PenLineJoin.Round,Stretch=Stretch.Uniform,Margin=new Thickness(size*0.16)};
            var g=new Grid();g.Children.Add(under);g.Children.Add(path);
            return new Border{Width=size,Height=size,CornerRadius=new CornerRadius(size*0.24),Background=Theme.ChromeBrush(),Padding=new Thickness(Math.Max(1,size*0.05)),
                Child=new Border{CornerRadius=new CornerRadius(size*0.2),Background=Theme.B(Theme.Ink),Child=g}};
        }
    }

    // Embedded pictures from the brand kit (tools/make_logo_assets.ps1): the logo and the gate pictures, the same view of
    // Los Santos through the visor in each colour variant of the logo (one per profile, "Imagen de perfil").
    public static class Assets {
        static BitmapSource Load(string name){
            var s=System.Reflection.Assembly.GetExecutingAssembly().GetManifestResourceStream("GtavxrSuite."+name);
            if(s==null)return null;
            using(s){var b=new BitmapImage();b.BeginInit();b.CacheOption=BitmapCacheOption.OnLoad;b.StreamSource=s;b.EndInit();b.Freeze();return b;}
        }
        static BitmapSource logo;
        static readonly Dictionary<string,BitmapSource> gates=new Dictionary<string,BitmapSource>();
        public static readonly string[] GateIds={"original","midnight","amber","noir","arctic"};
        public static BitmapSource Logo{get{return logo??(logo=Load("logo.png"));}}
        public static string GateId(string id){return id!=null&&Array.IndexOf(GateIds,id)>=0?id:GateIds[0];}
        public static BitmapSource Gate(string id){
            id=GateId(id);BitmapSource b;
            if(!gates.TryGetValue(id,out b)){b=Load("gate-"+id+".png");gates[id]=b;}
            return b;
        }
    }

    // Signage letters like the logo's: white fill, heavy black outline and a hard drop below.
    public class StickerText : FrameworkElement {
        readonly string text;readonly double size,stroke;readonly Brush fill;readonly FontStretch stretch;
        Geometry geo;
        public StickerText(string t,double sz,Brush f=null,double outline=0,bool condensed=true){
            text=t??"";size=sz;fill=f??Brushes.White;stroke=outline>0?outline:Math.Max(2,sz*0.11);stretch=condensed?FontStretches.Condensed:FontStretches.Normal;
            SnapsToDevicePixels=true;
        }
        Geometry Geo(){
            if(geo!=null)return geo;
            var ft=new FormattedText(text,System.Globalization.CultureInfo.CurrentUICulture,FlowDirection.LeftToRight,
                new Typeface(Theme.Sign_,FontStyles.Normal,FontWeights.Bold,stretch),size,Brushes.White,1.0);
            geo=ft.BuildGeometry(new Point(stroke,stroke));geo.Freeze();return geo;
        }
        protected override Size MeasureOverride(Size available){
            var b=Geo().Bounds;if(b.IsEmpty)return new Size(0,0);
            return new Size(b.Right+stroke,b.Bottom+stroke+size*0.06);
        }
        protected override void OnRender(DrawingContext dc){
            var g=Geo();var pen=new Pen(Brushes.Black,stroke*2){LineJoin=PenLineJoin.Round};
            dc.PushTransform(new TranslateTransform(0,size*0.06));dc.DrawGeometry(Brushes.Black,pen,g);dc.Pop();
            dc.DrawGeometry(Brushes.Black,pen,g);
            dc.DrawGeometry(fill,null,g);
        }
    }

    // One row of split-flap tiles. Text is shown at rest; Flip() replays the cascade.
    public class FlapText : StackPanel {
        readonly Border[] tiles;
        public FlapText(string text,int cells,Color ink,double size=17){
            Orientation=Orientation.Horizontal;
            text=(text??"").ToUpperInvariant();if(text.Length>cells)text=text.Substring(0,cells-1)+"…";
            tiles=new Border[cells];
            for(int i=0;i<cells;i++){
                char ch=i<text.Length?text[i]:' ';
                var g=new Grid();
                var t=new TextBlock{Text=ch.ToString(),FontFamily=Theme.Sign_,FontWeight=FontWeights.SemiBold,FontSize=size,Foreground=Theme.B(ink),
                    HorizontalAlignment=HorizontalAlignment.Center,VerticalAlignment=VerticalAlignment.Center,Margin=new Thickness(0,0,0,1)};
                g.Children.Add(t);
                g.Children.Add(new Rectangle{Height=1,Fill=Theme.B(Theme.FlapCut),VerticalAlignment=VerticalAlignment.Center});
                var b=new Border{Child=g,Width=size*0.86,Height=size*1.34,Margin=new Thickness(0,0,2,0),CornerRadius=new CornerRadius(2),
                    Background=ch==' '&&i>=text.Length?Theme.B(Theme.Flap,0.45):Theme.B(Theme.Flap),RenderTransformOrigin=new Point(0.5,0.5),RenderTransform=new ScaleTransform(1,1)};
                tiles[i]=b;Children.Add(b);
            }
        }
        public void Flip(){
            if(!SystemParameters.ClientAreaAnimation)return;
            for(int i=0;i<tiles.Length;i++){
                var a=new DoubleAnimation(0.05,1,TimeSpan.FromMilliseconds(320)){BeginTime=TimeSpan.FromMilliseconds(i*24),EasingFunction=new CubicEase{EasingMode=EasingMode.EaseOut}};
                ((ScaleTransform)tiles[i].RenderTransform).BeginAnimation(ScaleTransform.ScaleYProperty,a);
            }
        }
    }

    // Luggage tag outline (notched left corners) for mods.
    public class TagShape : Shape {
        protected override Geometry DefiningGeometry{get{
            double w=Math.Max(RenderSize.Width,40),h=Math.Max(RenderSize.Height,40),n=20;
            var g=new StreamGeometry();
            using(var c=g.Open()){c.BeginFigure(new Point(n,0),true,true);c.LineTo(new Point(w-8,0),true,true);c.ArcTo(new Point(w,8),new Size(8,8),0,false,SweepDirection.Clockwise,true,true);
                c.LineTo(new Point(w,h-8),true,true);c.ArcTo(new Point(w-8,h),new Size(8,8),0,false,SweepDirection.Clockwise,true,true);
                c.LineTo(new Point(n,h),true,true);c.LineTo(new Point(0,h-n),true,true);c.LineTo(new Point(0,n),true,true);}
            g.Freeze();return g;
        }}
    }

    // Barcode printed on the pass: bars derived from the profile id, so each pass is visibly its own.
    public class Barcode : FrameworkElement {
        readonly string seed;readonly Color ink;
        public Barcode(string id,Color c){seed=id??"";ink=c;Width=170;Height=30;}
        protected override void OnRender(DrawingContext dc){
            var b=Theme.B(ink);double x=0;int h=17;
            foreach(char ch in seed+seed){h=h*31+ch;int w=1+Math.Abs(h%3);int gap=1+Math.Abs((h/7)%3);if(x+w>Width)break;dc.DrawRectangle(b,null,new Rect(x,0,w,Height));x+=w+gap;}
        }
    }
}
