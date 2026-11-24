// Indices 0 through 13 are persisted by earlier four-slot profiles. Append new entries.
struct WheelWeapon {unsigned hash;int category;const wchar_t* name;};
constexpr WheelWeapon catalog[]={
 {0x1B06D571u,0,L"Pistol"},
 {0x5EF9FEC4u,0,L"Combat Pistol"},
 {0x99AEEB3Bu,0,L"Pistol .50"},
 {0x13532244u,1,L"Micro SMG"},
 {0x1D073A89u,5,L"Pump Shotgun"},
 {0x7846A318u,5,L"Sawed-Off Shotgun"},
 {0xE284C527u,5,L"Assault Shotgun"},
 {0x9D61E50Fu,5,L"Bullpup Shotgun"},
 {0x3AABBBAAu,5,L"Heavy Shotgun"},
 {0xEF951FBBu,5,L"Double-Barrel Shotgun"},
 {0x12E82D3Du,5,L"Sweeper Shotgun"},
 {0x555AF99Au,5,L"Pump Shotgun Mk2"},
 {0x05A96BA4u,5,L"Combat Shotgun"},
 {0xA89CB99Eu,5,L"Musket"},
 {0xD8DF3C3Cu,4,L"Knuckle Duster"},
 {0xDFE37640u,4,L"Switchblade"},
 {0x99B507EAu,4,L"Knife"},
 {0x678B81B1u,4,L"Night Stick"},
 {0x4E875F73u,4,L"Hammer"},
 {0x958A4A8Fu,4,L"Baseball Bat"},
 {0x440E4788u,4,L"Golf Club"},
 {0x84BD7BFDu,4,L"Crowbar"},
 {0xF9E6AA4Bu,4,L"Bottle"},
 {0x92A27487u,4,L"Antique Cavalry Dagger"},
 {0xF9DCBF2Du,4,L"Hatchet"},
 {0xDD5DF8D9u,4,L"Machete"},
 {0x8BB05FD7u,4,L"Flashlight"},
 {0xCD274149u,4,L"Battle Axe"},
 {0x94117305u,4,L"Pool Cue"},
 {0x19044EE0u,4,L"Wrench"},
 {0x3813FC08u,4,L"Stone Hatchet"},
 {0x184140A1u,7,L"Fertilizer Can"},
 {0xDBA2E809u,4,L"Metal Detector"},
 {0x6589186Au,4,L"Candy Cane"},
 {0xDAC00025u,4,L"Stun Rod"},
 {0xBFE256D4u,0,L"Pistol Mk2"},
 {0x22D8FE39u,0,L"AP Pistol"},
 {0xBFD21232u,0,L"SNS Pistol"},
 {0x88374054u,0,L"SNS Pistol Mk2"},
 {0xD205520Eu,0,L"Heavy Pistol"},
 {0x083839C4u,0,L"Vintage Pistol"},
 {0xDC4DB296u,0,L"Marksman Pistol"},
 {0xDB1AA450u,1,L"Machine Pistol"},
 {0xC1B3C3D1u,0,L"Revolver"},
 {0xCB96392Fu,0,L"Revolver Mk2"},
 {0x97EA20B8u,0,L"Double-Action Revolver"},
 {0x3656C8C1u,0,L"Stun Gun"},
 {0x47757124u,0,L"Flare Gun"},
 {0xAF3696A1u,0,L"Ray Pistol"},
 {0x917F6C8Cu,0,L"Navy Revolver"},
 {0x2B5EF5ECu,0,L"Ceramic Pistol"},
 {0x57A4368Cu,0,L"Perico Pistol"},
 {0x45CD9CF3u,0,L"Stun Gun MP"},
 {0x1BC4FDB9u,0,L"Pistol XM3"},
 {0x2BE6766Bu,1,L"SMG"},
 {0x78A97CD0u,1,L"SMG Mk2"},
 {0xEFE7E2DFu,1,L"Assault SMG"},
 {0x0A3D4D34u,1,L"Combat PDW"},
 {0xBD248B55u,1,L"Mini SMG"},
 {0x14E5AFD5u,1,L"Tactical SMG"},
 {0x9D07F764u,1,L"MG"},
 {0x7FD62962u,1,L"Combat MG"},
 {0xDBBD7280u,1,L"Combat MG Mk2"},
 {0x61012683u,1,L"Gusenberg Sweeper"},
 {0x476BF155u,1,L"Ray Carbine Rifle"},
 {0xBFEFFF6Du,2,L"Assault Rifle"},
 {0x394F415Cu,2,L"Assault Rifle Mk2"},
 {0x83BF0278u,2,L"Carbine Rifle"},
 {0xFAD1F1C9u,2,L"Carbine Rifle Mk2"},
 {0xAF113F99u,2,L"Advanced Rifle"},
 {0xC0A3098Du,2,L"Special Carbine"},
 {0x969C3D67u,2,L"Special Carbine Mk2"},
 {0x7F229F94u,2,L"Bullpup Rifle"},
 {0x84D6FAFDu,2,L"Bullpup Rifle Mk2"},
 {0x624FE830u,2,L"Compact Rifle"},
 {0x9D1F17E6u,2,L"Military Rifle"},
 {0xC78D71B4u,2,L"Heavy Rifle"},
 {0xD1D5F52Bu,2,L"Tactical Rifle"},
 {0x72B66B11u,2,L"Battle Rifle"},
 {0x05FC3C11u,3,L"Sniper Rifle"},
 {0x0C472FE2u,3,L"Heavy Sniper"},
 {0x0A914799u,3,L"Heavy Sniper Mk2"},
 {0xC734385Au,3,L"Marksman Rifle"},
 {0x6A6C02E0u,3,L"Marksman Rifle Mk2"},
 {0x6E7DDDECu,3,L"Precision Rifle"},
 {0xA284510Bu,6,L"Grenade Launcher"},
 {0xB1CA77B1u,6,L"RPG"},
 {0x42BF8A85u,6,L"Minigun"},
 {0x7F7497E5u,6,L"Firework Launcher"},
 {0x6D544C99u,6,L"Railgun"},
 {0x63AB0442u,6,L"Homing Launcher"},
 {0x0781FE4Au,6,L"Compact Grenade Launcher"},
 {0xB62D1F67u,6,L"Ray Minigun"},
 {0xDB26713Au,6,L"EMP Launcher"},
 {0xFEA23564u,6,L"Railgun XM3"},
 {0x03BF5575u,6,L"Snow Launcher"},
 {0x93E220BDu,7,L"Grenade"},
 {0xFDBC8A50u,7,L"Tear Gas"},
 {0x2C3731D9u,7,L"Sticky Bomb"},
 {0xAB564B93u,7,L"Proximity Mine"},
 {0x24B17070u,7,L"Molotov"},
 {0x060EC506u,7,L"Fire Extinguisher"},
 {0x34A67B97u,7,L"Jerry Can"},
 {0x497FACC3u,7,L"Flare"},
 {0x0787F0BBu,7,L"Snowball"},
 {0xBA45E8B8u,7,L"Pipe Bomb"},
 {0xBA536372u,7,L"Hazardous Jerry Can"},
 {0xF7F1E25Eu,7,L"Acid Package"},
 {0xA2719263u,4,L"Unarmed"},
 {0x23C9F95Cu,7,L"Ball"},
 {0xA0973D5Eu,7,L"BZ Gas"},
};
constexpr int weaponCount=_countof(catalog),nextPageCard=weaponCount;
constexpr int editCard=weaponCount+1,doneCard=weaponCount+2;
constexpr wchar_t const* categoryNames[]={L"Handguns",L"Machine guns",L"Assault rifles",L"Sniper rifles",L"Melee",L"Shotguns",L"Heavy weapons",L"Throwables"};
constexpr float wheelY=-.18f,wheelZ=-.43f;
constexpr float innerRadius=.107f,outerRadius=.232f,iconRadius=.167f;
constexpr float diagonal=iconRadius*.70710678f;
constexpr std::array<XrVector3f,9> cards={{{0,wheelY+iconRadius,wheelZ},{diagonal,wheelY+diagonal,wheelZ},{iconRadius,wheelY,wheelZ},
 {diagonal,wheelY-diagonal,wheelZ},{0,wheelY-iconRadius,wheelZ},{-diagonal,wheelY-diagonal,wheelZ},{-iconRadius,wheelY,wheelZ},{-diagonal,wheelY+diagonal,wheelZ},{0,wheelY,wheelZ}}};
int angleSector(float x,float y){return int(floorf(atan2f(x,y)*1.27323954f+.5f)+8)&7;}
extern bool editor;
std::array<int,weaponCount> available{};
std::array<int,8> selected{};
std::array<int,9> pageCards(){
 std::array<int,9> result;result.fill(-1);
 if(editor){result[8]=doneCard;return result;}
 for(int i=0;i<weaponCount;i++)if(available[i]){
    int category=catalog[i].category;
    if(result[category]<0||selected[category]==i)result[category]=i;
 }

 return result;
}
void cycleCategory(int category){
 if(category<0||category>=8)return;
 int current=pageCards()[category];
 for(int n=1;n<=weaponCount;n++){
    int item=(current+n)%weaponCount;
    if(available[item]&&catalog[item].category==category){selected[category]=item;return;}
 }
}
