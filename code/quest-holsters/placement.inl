using Layout=std::array<XrVector3f,4>;
Layout defaultLayout(){
 Layout result{};for(int i=0;i<4;i++){const auto& b=bounds[i/2];result[i]={(i%2?1.f:-1.f)*(b.left+b.right)/2,(b.bottom+b.top)/2,(b.back+b.front)/2};}return result;
}
Layout centers=defaultLayout();
std::filesystem::path layoutFile;
bool markersEnabled=true;
int editSlot=-1,dragHand=-1;
bool fitting=false,waitRelease=false;
float squeezes[2]{};
ULONGLONG gripSeen[2]{},chordSince=0;
void resetFitting(){
 fitting=false;waitRelease=false;chordSince=0;squeezes[0]=squeezes[1]=0;
 gripSeen[0]=gripSeen[1]=0;editSlot=dragHand=-1;
}
int nearestSlot(XrVector3f pos){
 int selected=0;float best=std::numeric_limits<float>::max();
 for(int i=0;i<4;i++){auto p=centers[i];float dx=pos.x-p.x,dy=pos.y-p.y,dz=pos.z-p.z;
 float d=dx*dx+dy*dy+dz*dz;if(d<best){best=d;selected=i;}}
 return selected;
}
// Both controllers held out front, away from the headset wheel gesture.
bool handsInTogglePosition(ULONGLONG now){
 for(int i=0;i<2;i++){auto& h=hands[i];auto p=h.position;
 if(!h.seen||now-h.seen>150||
    p.z>-.27f||p.y<-.45f||p.y>.10f)return false;}
 auto a=hands[0].position,b=hands[1].position;
 return fabsf(a.x-b.x)<.22f && fabsf(a.y-b.y)<.16f && fabsf(a.z-b.z)<.16f;
}
bool fittingChord(ULONGLONG now){
 return handsInTogglePosition(now)&&now-gripSeen[0]<=150&&now-gripSeen[1]<=150&&squeezes[0]>=.7f&&squeezes[1]>=.7f;
}
bool validCenter(XrVector3f p){
 float squared=p.x*p.x+p.y*p.y+p.z*p.z;return std::isfinite(squared)&&squared>.015f&&squared<2.25f;
}
bool readLayout(const std::filesystem::path& path,Layout& result){
 std::ifstream input(path);Layout candidate{};
 for(auto& v:candidate)if(!(input>>v.x>>v.y>>v.z)||!validCenter(v))return false;
 input>>std::ws;if(!input.eof())return false;result=candidate;return true;
}
void loadPlacement(){
 centers=defaultLayout();layoutFile.clear();wchar_t base[32768]{},choice[4]{};
 auto length=GetEnvironmentVariableW(L"LOCALAPPDATA",base,32768);
 if(length&&length<32768){layoutFile=std::filesystem::path(base)/L"GTAVXR-QuestHolsters"/L"positions-shoulders-v2.txt";
 if(!readLayout(layoutFile,centers)){
 Layout old;if(readLayout(layoutFile.parent_path()/L"positions-v1.txt",old)){
 centers[0]=old[0];centers[1]=old[1];}
 }}
 markersEnabled=GetEnvironmentVariableW(L"QUEST_HOLSTER_HIDE_MARKERS",choice,4)==0;
 resetFitting();
 if(markersEnabled&&GetEnvironmentVariableW(L"QUEST_HOLSTER_EDIT",choice,4)==1&&choice[0]>=L'1'&&choice[0]<=L'4')editSlot=choice[0]-L'1';
}
bool commitCenter(int slot,XrVector3f pos){
 if(layoutFile.empty()||!validCenter(pos))return false;
 Layout updated=centers;updated[slot]=pos;
 std::error_code err;std::filesystem::create_directories(layoutFile.parent_path(),err);if(err)return false;
 auto tmp=layoutFile;tmp+=L".tmp";std::ofstream out(tmp,std::ios::trunc);out.precision(9);
 for(auto v:updated)out<<v.x<<' '<<v.y<<' '<<v.z<<'\n';
 out.close();if(!out||!MoveFileExW(tmp.c_str(),layoutFile.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return false;
 centers=updated;return true;
}
