// Side-view inventory pictograms, in card-local pixels.
void drawCardIcon(HDC dc,int item,COLORREF ink){
 if(item>=4){
    const POINT shotgun[]={{5,45},{38,29},{43,21},{115,21},{115,17},{160,17},{160,23},{120,25},{112,36},{69,36},{57,42},{50,42},{46,38},{11,59}};
    const POINT next[]={{8,28},{62,28},{62,11},{91,36},{62,61},{62,45},{8,45}};
    SetDCBrushColor(dc,ink);
    if(item==nextPageCard)Polygon(dc,next,_countof(next));else Polygon(dc,shotgun,_countof(shotgun));
    return;
 }
 const POINT pistol[]={{5,12},{18,9},{19,5},{26,5},{27,9},{104,9},{104,25},{70,25},{61,38},{45,38},{33,67},{12,63},{27,30},{9,28}};
 const POINT combat[]={{8,14},{20,11},{20,6},{26,6},{28,10},{91,10},{98,15},{98,29},{70,29},{59,44},{42,44},{34,68},{14,64},{24,34},{7,29}};
 const POINT heavy[]={{3,18},{17,15},{19,7},{27,7},{32,12},{115,12},{123,19},{123,30},{77,30},{64,45},{44,45},{34,68},{10,62},{25,36},{7,32}};
 const POINT micro[]={{5,19},{26,19},{28,12},{47,12},{48,6},{58,6},{60,13},{93,13},{96,20},{115,20},{115,29},{96,29},{94,37},{65,37},{65,46},{47,46},{43,68},{27,66},{29,40},{7,39}};
 const POINT* outlines[]={pistol,combat,heavy,micro};
 const int counts[]={_countof(pistol),_countof(combat),_countof(heavy),_countof(micro)};
 SetDCBrushColor(dc,ink);Polygon(dc,outlines[item],counts[item]);
}

void wheelIcon(HDC dc,int item,COLORREF ink){
 int category=catalog[item].category;
 if(category==0||category==1||category==5){
    drawCardIcon(dc,category==0?(item<3?item:0):category==1?3:4,ink);return;
 }
 SetDCBrushColor(dc,ink);
 constexpr POINT rifle[]={{3,31},{30,26},{40,18},{92,18},{95,22},{146,22},{146,28},{100,30},{90,36},{94,54},{83,57},{74,38},{55,38},{46,54},{36,51},{40,36},{6,46}};
 constexpr POINT sniper[]={{2,42},{33,27},{73,25},{85,19},{92,22},{157,22},{157,27},{92,30},{81,35},{57,36},{45,48},{38,45},{40,36},{5,53}};
 constexpr POINT launcher[]={{4,21},{15,17},{22,21},{112,21},{117,15},{139,20},{157,29},{139,39},{117,42},{111,35},{65,35},{61,50},{51,50},{49,35},{15,35},{6,39}};
 constexpr POINT blade[]={{8,49},{49,33},{47,27},{52,25},{57,30},{139,12},{119,32},{61,40},{62,46},{56,48},{53,41},{13,58}};
 if(category==2)Polygon(dc,rifle,_countof(rifle));
 if(category==3){Polygon(dc,sniper,_countof(sniper));RoundRect(dc,46,10,85,18,5,5);Rectangle(dc,55,17,60,25);Rectangle(dc,73,17,78,25);}
 if(category==4){
    if(catalog[item].hash==0xA2719263u){for(int i=0;i<4;i++)RoundRect(dc,44+i*12,12+abs(i-1)*2,58+i*12,42,8,8);RoundRect(dc,43,31,94,61,12,12);RoundRect(dc,33,27,48,48,9,9);}
    else Polygon(dc,blade,_countof(blade));
 }
 if(category==6)Polygon(dc,launcher,_countof(launcher));
 if(category==7){Ellipse(dc,51,24,94,65);Rectangle(dc,63,16,82,29);constexpr POINT lever[]={{65,13},{86,13},{101,38},{97,41},{82,20},{65,20}};Polygon(dc,lever,_countof(lever));}
}
