using System;
using System.Windows;
using System.Windows.Media;
using System.Windows.Markup;

namespace GtavxrSuite {
    // "Terminal Los Santos", in the colors of the GTAVXR logo: night terminal glass, Los Santos sky blue seen through the
    // visor, the chrome of the headset frame, white signage letters with a heavy black outline, and the green of the V
    // for the one thing you came to do (Play). Hazard yellow is kept only for test flights and warnings.
    // Bahnschrift (Windows' own signage face) for everything you scan, Segoe UI for reading.
    public static class Theme {
        public static readonly Color Terminal=C("#05070B"),Board=C("#0C1118"),Flap=C("#151C26"),FlapCut=C("#03050A"),
            Sign=C("#56B8FF"),Glass=C("#CBE9FF"),SkyDeep=C("#1B5FA6"),FlapInk=C("#F3F6F9"),
            Ticket=C("#F4F7FA"),Ink=C("#0B0F15"),InkSoft=C("#56606C"),Mist=C("#8C97A4"),Line=C("#1E2733"),Text=C("#E8EDF2"),
            Go=C("#5CC72E"),GoDeep=C("#2C7F14"),Stop=C("#FF5A5F"),Hazard=C("#FFC40C"),
            Hover=C("#111A24"),Selected=C("#10233A"),PaperLine=C("#C5CDD6"),Chrome=C("#C9D1D9");
        public static readonly Color[] Bands={C("#25BFB0"),C("#FF6A45"),C("#8C7BF5"),C("#4C9AFF"),C("#F25CA2"),C("#9BD14B")};
        public static readonly FontFamily Sign_=new FontFamily("Bahnschrift"),Body=new FontFamily("Segoe UI Variable Text, Segoe UI"),Mono=new FontFamily("Consolas");
        public static Color C(string hex){return (Color)ColorConverter.ConvertFromString(hex);}
        public static SolidColorBrush B(Color c){var b=new SolidColorBrush(c);b.Freeze();return b;}
        public static SolidColorBrush B(Color c,double opacity){var b=new SolidColorBrush(Color.FromArgb((byte)(opacity*255),c.R,c.G,c.B));b.Freeze();return b;}
        public static Color Band(int i){return Bands[((i%Bands.Length)+Bands.Length)%Bands.Length];}

        static LinearGradientBrush Lin(Point a,Point b,params object[] stops){
            var g=new LinearGradientBrush{StartPoint=a,EndPoint=b};
            for(int i=0;i<stops.Length;i+=2)g.GradientStops.Add(new GradientStop(C((string)stops[i]),(double)stops[i+1]));
            g.Freeze();return g;
        }
        // Polished chrome like the visor frame and the X and R of the logo.
        public static Brush ChromeBrush(){return Lin(new Point(0,0),new Point(0,1),"#FFFFFF",0.0,"#DCE3EA",0.32,"#8E99A6",0.5,"#C3CCD5",0.64,"#F1F4F7",1.0);}
        // Thin chrome rim for panels (bright top, dark bottom).
        public static Brush RimBrush(){return Lin(new Point(0,0),new Point(0,1),"#7D8995",0.0,"#2A333E",0.45,"#151B23",1.0);}
        public static Brush PanelBrush(){return Lin(new Point(0,0),new Point(0,1),"#111923",0.0,"#0A0F15",1.0);}
        public static Brush GoBrush(){return Lin(new Point(0,0),new Point(0,1),"#6BD33A",0.0,"#3FA51F",0.5,"#2C7F14",1.0);}
        // The window: night terminal with the glow of the sky through the windows.
        public static Brush WindowBrush(){
            var r=new RadialGradientBrush{Center=new Point(0.82,-0.05),GradientOrigin=new Point(0.82,-0.05),RadiusX=0.95,RadiusY=0.95};
            r.GradientStops.Add(new GradientStop(C("#0F2740"),0));r.GradientStops.Add(new GradientStop(C("#081321"),0.45));r.GradientStops.Add(new GradientStop(Terminal,1));
            r.Freeze();return r;
        }
        public static Brush SkyLine(){return Lin(new Point(0,0),new Point(1,0),"#0056B8FF",0.0,"#FF56B8FF",0.5,"#0056B8FF",1.0);}
        // Black and yellow tape: test flights.
        public static Brush HazardBrush(double size=12){
            var g=new LinearGradientBrush{StartPoint=new Point(0,0),EndPoint=new Point(size,size),MappingMode=BrushMappingMode.Absolute,SpreadMethod=GradientSpreadMethod.Repeat};
            g.GradientStops.Add(new GradientStop(Hazard,0));g.GradientStops.Add(new GradientStop(Hazard,0.5));
            g.GradientStops.Add(new GradientStop(C("#111111"),0.5));g.GradientStops.Add(new GradientStop(C("#111111"),1));
            g.Freeze();return g;
        }

        // Control templates live in XAML (parsed at start) so they can use triggers.
        public static ResourceDictionary Resources(){
            const string x=@"<ResourceDictionary xmlns='http://schemas.microsoft.com/winfx/2006/xaml/presentation' xmlns:x='http://schemas.microsoft.com/winfx/2006/xaml'>
  <LinearGradientBrush x:Key='GoFill' StartPoint='0,0' EndPoint='0,1'><GradientStop Color='#6BD33A' Offset='0'/><GradientStop Color='#3FA51F' Offset='0.5'/><GradientStop Color='#2C7F14' Offset='1'/></LinearGradientBrush>
  <LinearGradientBrush x:Key='GoHot' StartPoint='0,0' EndPoint='0,1'><GradientStop Color='#82E24F' Offset='0'/><GradientStop Color='#4DB82A' Offset='0.5'/><GradientStop Color='#348F18' Offset='1'/></LinearGradientBrush>
  <LinearGradientBrush x:Key='ChromeFill' StartPoint='0,0' EndPoint='0,1'><GradientStop Color='#FFFFFF' Offset='0'/><GradientStop Color='#DCE3EA' Offset='0.32'/><GradientStop Color='#8E99A6' Offset='0.5'/><GradientStop Color='#C3CCD5' Offset='0.64'/><GradientStop Color='#F1F4F7' Offset='1'/></LinearGradientBrush>
  <Style x:Key='Btn' TargetType='Button'>
    <Setter Property='Foreground' Value='#E8EDF2'/><Setter Property='Background' Value='#151C26'/><Setter Property='BorderBrush' Value='#2A3542'/>
    <Setter Property='FontFamily' Value='Bahnschrift'/><Setter Property='FontWeight' Value='SemiBold'/><Setter Property='FontSize' Value='14'/>
    <Setter Property='Padding' Value='14,8'/><Setter Property='Cursor' Value='Hand'/><Setter Property='FocusVisualStyle' Value='{x:Null}'/>
    <Setter Property='Template'><Setter.Value><ControlTemplate TargetType='Button'>
      <Border x:Name='b' Background='{TemplateBinding Background}' BorderBrush='{TemplateBinding BorderBrush}' BorderThickness='1.5' CornerRadius='7' Padding='{TemplateBinding Padding}'>
        <ContentPresenter HorizontalAlignment='Center' VerticalAlignment='Center'/></Border>
      <ControlTemplate.Triggers>
        <Trigger Property='IsMouseOver' Value='True'><Setter TargetName='b' Property='BorderBrush' Value='#56B8FF'/></Trigger>
        <Trigger Property='IsKeyboardFocused' Value='True'><Setter TargetName='b' Property='BorderBrush' Value='#56B8FF'/></Trigger>
        <Trigger Property='IsEnabled' Value='False'><Setter TargetName='b' Property='Opacity' Value='0.4'/></Trigger>
      </ControlTemplate.Triggers></ControlTemplate></Setter.Value></Setter>
  </Style>
  <Style x:Key='Primary' TargetType='Button' BasedOn='{StaticResource Btn}'>
    <Setter Property='Background' Value='{StaticResource GoFill}'/><Setter Property='BorderBrush' Value='#0B0F15'/><Setter Property='Foreground' Value='#FFFFFF'/><Setter Property='FontWeight' Value='Bold'/>
    <Setter Property='Template'><Setter.Value><ControlTemplate TargetType='Button'>
      <Border x:Name='b' Background='{TemplateBinding Background}' BorderBrush='{TemplateBinding BorderBrush}' BorderThickness='2' CornerRadius='7' Padding='{TemplateBinding Padding}'>
        <ContentPresenter HorizontalAlignment='Center' VerticalAlignment='Center'/></Border>
      <ControlTemplate.Triggers>
        <Trigger Property='IsMouseOver' Value='True'><Setter TargetName='b' Property='Background' Value='{StaticResource GoHot}'/></Trigger>
        <Trigger Property='IsKeyboardFocused' Value='True'><Setter TargetName='b' Property='BorderBrush' Value='#FFFFFF'/></Trigger>
        <Trigger Property='IsEnabled' Value='False'><Setter TargetName='b' Property='Opacity' Value='0.4'/></Trigger>
      </ControlTemplate.Triggers></ControlTemplate></Setter.Value></Setter>
  </Style>
  <Style x:Key='Ghost' TargetType='Button' BasedOn='{StaticResource Btn}'><Setter Property='Background' Value='Transparent'/></Style>
  <Style x:Key='OnTicket' TargetType='Button' BasedOn='{StaticResource Btn}'>
    <Setter Property='Background' Value='Transparent'/><Setter Property='Foreground' Value='#0B0F15'/><Setter Property='BorderBrush' Value='#0B0F15'/><Setter Property='Padding' Value='12,5'/>
  </Style>
  <Style x:Key='Go' TargetType='Button' BasedOn='{StaticResource Primary}'><Setter Property='FontSize' Value='26'/><Setter Property='Padding' Value='10,13'/><Setter Property='BorderBrush' Value='#000000'/></Style>
  <Style x:Key='Link' TargetType='Button'>
    <Setter Property='Foreground' Value='#0B0F15'/><Setter Property='FontFamily' Value='Bahnschrift'/><Setter Property='FontSize' Value='14'/><Setter Property='Cursor' Value='Hand'/><Setter Property='FocusVisualStyle' Value='{x:Null}'/>
    <Setter Property='Template'><Setter.Value><ControlTemplate TargetType='Button'>
      <Border x:Name='b' BorderBrush='{TemplateBinding Foreground}' BorderThickness='0,0,0,1.5' Padding='0,0,0,1'><ContentPresenter/></Border>
      <ControlTemplate.Triggers><Trigger Property='IsMouseOver' Value='True'><Setter TargetName='b' Property='Opacity' Value='0.7'/></Trigger></ControlTemplate.Triggers>
    </ControlTemplate></Setter.Value></Setter>
  </Style>
  <Style x:Key='Plain' TargetType='Button'>
    <Setter Property='Cursor' Value='Hand'/><Setter Property='FocusVisualStyle' Value='{x:Null}'/><Setter Property='Background' Value='Transparent'/>
    <Setter Property='Template'><Setter.Value><ControlTemplate TargetType='Button'><Border Background='{TemplateBinding Background}'><ContentPresenter/></Border></ControlTemplate></Setter.Value></Setter>
  </Style>
  <Style x:Key='Danger' TargetType='Button' BasedOn='{StaticResource Btn}'><Setter Property='Background' Value='Transparent'/><Setter Property='Foreground' Value='#FF5A5F'/><Setter Property='BorderBrush' Value='#5A2A2E'/></Style>
  <Style x:Key='Nav' TargetType='RadioButton'>
    <Setter Property='Foreground' Value='#8C97A4'/><Setter Property='FontFamily' Value='Bahnschrift'/><Setter Property='FontWeight' Value='SemiBold'/><Setter Property='FontSize' Value='15'/><Setter Property='Cursor' Value='Hand'/>
    <Setter Property='FocusVisualStyle' Value='{x:Null}'/>
    <Setter Property='Template'><Setter.Value><ControlTemplate TargetType='RadioButton'>
      <Border x:Name='b' Background='Transparent' BorderBrush='Transparent' BorderThickness='1.5' CornerRadius='16' Padding='15,5'><ContentPresenter VerticalAlignment='Center'/></Border>
      <ControlTemplate.Triggers>
        <Trigger Property='IsMouseOver' Value='True'><Setter Property='Foreground' Value='#E8EDF2'/></Trigger>
        <Trigger Property='IsChecked' Value='True'><Setter TargetName='b' Property='Background' Value='{StaticResource ChromeFill}'/><Setter TargetName='b' Property='BorderBrush' Value='#000000'/><Setter Property='Foreground' Value='#0B0F15'/></Trigger>
        <Trigger Property='IsKeyboardFocused' Value='True'><Setter TargetName='b' Property='BorderBrush' Value='#56B8FF'/></Trigger>
      </ControlTemplate.Triggers></ControlTemplate></Setter.Value></Setter>
  </Style>
  <Style x:Key='Tab' TargetType='RadioButton'>
    <Setter Property='Foreground' Value='#8C97A4'/><Setter Property='FontFamily' Value='Bahnschrift'/><Setter Property='FontSize' Value='14'/><Setter Property='Cursor' Value='Hand'/>
    <Setter Property='FocusVisualStyle' Value='{x:Null}'/>
    <Setter Property='Template'><Setter.Value><ControlTemplate TargetType='RadioButton'>
      <Border x:Name='b' BorderBrush='Transparent' BorderThickness='0,0,0,3' Padding='16,11'><ContentPresenter/></Border>
      <ControlTemplate.Triggers>
        <Trigger Property='IsMouseOver' Value='True'><Setter Property='Foreground' Value='#E8EDF2'/></Trigger>
        <Trigger Property='IsChecked' Value='True'><Setter TargetName='b' Property='BorderBrush' Value='#56B8FF'/><Setter Property='Foreground' Value='#FFFFFF'/></Trigger>
      </ControlTemplate.Triggers></ControlTemplate></Setter.Value></Setter>
  </Style>
  <Style x:Key='Switch' TargetType='CheckBox'>
    <Setter Property='Cursor' Value='Hand'/><Setter Property='FocusVisualStyle' Value='{x:Null}'/>
    <Setter Property='Template'><Setter.Value><ControlTemplate TargetType='CheckBox'>
      <Border x:Name='t' Width='44' Height='24' CornerRadius='12' Background='#1B2430' BorderBrush='#2A3542' BorderThickness='1'>
        <Ellipse x:Name='k' Width='18' Height='18' Fill='#8C97A4' HorizontalAlignment='Left' Margin='2,0,0,0'/></Border>
      <ControlTemplate.Triggers>
        <Trigger Property='IsChecked' Value='True'><Setter TargetName='t' Property='Background' Value='#1D4A10'/><Setter TargetName='t' Property='BorderBrush' Value='#3FA51F'/><Setter TargetName='k' Property='Fill' Value='#6BD33A'/><Setter TargetName='k' Property='Margin' Value='22,0,0,0'/></Trigger>
        <Trigger Property='IsEnabled' Value='False'><Setter TargetName='t' Property='Opacity' Value='0.35'/></Trigger>
        <Trigger Property='IsKeyboardFocused' Value='True'><Setter TargetName='t' Property='BorderBrush' Value='#56B8FF'/></Trigger>
      </ControlTemplate.Triggers></ControlTemplate></Setter.Value></Setter>
  </Style>
  <Style x:Key='Field' TargetType='TextBox'>
    <Setter Property='Background' Value='#05070B'/><Setter Property='Foreground' Value='#E8EDF2'/><Setter Property='BorderBrush' Value='#2A3542'/>
    <Setter Property='CaretBrush' Value='#56B8FF'/><Setter Property='FontFamily' Value='Bahnschrift'/><Setter Property='FontSize' Value='18'/><Setter Property='Padding' Value='10,8'/>
    <Setter Property='Template'><Setter.Value><ControlTemplate TargetType='TextBox'>
      <Border x:Name='b' Background='{TemplateBinding Background}' BorderBrush='{TemplateBinding BorderBrush}' BorderThickness='1.5' CornerRadius='7'>
        <ScrollViewer x:Name='PART_ContentHost' Margin='{TemplateBinding Padding}'/></Border>
      <ControlTemplate.Triggers><Trigger Property='IsKeyboardFocused' Value='True'><Setter TargetName='b' Property='BorderBrush' Value='#56B8FF'/></Trigger></ControlTemplate.Triggers>
    </ControlTemplate></Setter.Value></Setter>
  </Style>
  <Style TargetType='ScrollBar'>
    <Setter Property='Width' Value='8'/><Setter Property='MinWidth' Value='8'/><Setter Property='Background' Value='Transparent'/>
    <Setter Property='Template'><Setter.Value><ControlTemplate TargetType='ScrollBar'>
      <Track x:Name='PART_Track' IsDirectionReversed='True'><Track.Thumb><Thumb><Thumb.Template><ControlTemplate TargetType='Thumb'>
        <Border CornerRadius='4' Background='#26313E' Margin='2,0'/></ControlTemplate></Thumb.Template></Thumb></Track.Thumb></Track>
    </ControlTemplate></Setter.Value></Setter>
  </Style>
  <Style TargetType='ToolTip'><Setter Property='Background' Value='#151C26'/><Setter Property='Foreground' Value='#E8EDF2'/><Setter Property='BorderBrush' Value='#2A3542'/><Setter Property='FontFamily' Value='Segoe UI'/></Style>
</ResourceDictionary>";
            return (ResourceDictionary)XamlReader.Parse(x);
        }
    }
}
