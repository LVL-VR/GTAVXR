static int GET_CLOCK_HOURS(){
 return static_cast<int>(native(0x09fc827691dace59ULL,NULL,0));
}
static int GET_CLOCK_MINUTES(){
 return static_cast<int>(native(0x80f97d7d29800a1aULL,NULL,0));
}
static int GET_CLOCK_SECONDS(){
 return static_cast<int>(native(0xaa2844cad88768b5ULL,NULL,0));
}
static int GET_CLOCK_MONTH(){
 return static_cast<int>(native(0x771485043fdc55deULL,NULL,0));
}
static int GET_CLOCK_DAY_OF_MONTH(){
 return static_cast<int>(native(0xd52bd97e61483713ULL,NULL,0));
}
static int GET_CLOCK_YEAR(){
 return static_cast<int>(native(0x1137fd08b8d3f874ULL,NULL,0));
}
static void SET_CLOCK_DATE(int a0,int a1,int a2){
 uint64_t arguments[3]={};
 memcpy(&arguments[0],&a0,sizeof(a0));
 memcpy(&arguments[1],&a1,sizeof(a1));
 memcpy(&arguments[2],&a2,sizeof(a2));
 native(0xf9352288b8dec888ULL,arguments,3);
}
static void NETWORK_OVERRIDE_CLOCK_TIME(int a0,int a1,int a2){
 uint64_t arguments[3]={};
 memcpy(&arguments[0],&a0,sizeof(a0));
 memcpy(&arguments[1],&a1,sizeof(a1));
 memcpy(&arguments[2],&a2,sizeof(a2));
 native(0x42ea11f7821d9466ULL,arguments,3);
}
static void SET_TIME_SCALE(float a0){
 uint64_t arguments[1]={};
 memcpy(&arguments[0],&a0,sizeof(a0));
 native(0x434e3ae126ddc5b5ULL,arguments,1);
}
