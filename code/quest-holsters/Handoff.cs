using System;
using System.IO;
using System.Globalization;
using System.Runtime.InteropServices;
using GTA;
using GTA.Math;
using GTA.Native;

public sealed partial class QuestInventory {
    int probeEnd,nextSample,attempt;
    bool wheelSeen;

    void ProbeLog(string message){
        try{
            Directory.CreateDirectory(saveDir);
            const string log="native-four-slots.log";
            File.AppendAllText(Path.Combine(saveDir,log),DateTime.UtcNow.ToString("O")+" "+message+Environment.NewLine);
        }catch(IOException){}catch(UnauthorizedAccessException){}
    }
    string DescribeObject(int handle,Vector3 left,Vector3 right){
        if(handle==0 || !Function.Call<bool>(Hash.DOES_ENTITY_EXIST,handle))return "none";
        var position=Function.Call<Vector3>(Hash.GET_ENTITY_COORDS,handle,false);
        return String.Format(CultureInfo.InvariantCulture,"{0}/model={1:X8}/L={2:F2}/R={3:F2}",handle,
            Function.Call<uint>(Hash.GET_ENTITY_MODEL,handle),position.DistanceTo(left),position.DistanceTo(right));
    }
    void Sample(Ped ped,string phase){
        var left=Function.Call<Vector3>(Hash.GET_PED_BONE_COORDS,ped.Handle,0x49D9,0f,0f,0f);
        var right=Function.Call<Vector3>(Hash.GET_PED_BONE_COORDS,ped.Handle,0xDEAD,0f,0f,0f);
        uint weapon=sampledWeapon;
        uint model=Function.Call<uint>(Hash.GET_WEAPONTYPE_MODEL,weapon);
        int nearLeft=Function.Call<int>(Hash.GET_CLOSEST_OBJECT_OF_TYPE,left.X,left.Y,left.Z,.55f,model,false,false,false);
        int nearRight=Function.Call<int>(Hash.GET_CLOSEST_OBJECT_OF_TYPE,right.X,right.Y,right.Z,.55f,model,false,false,false);
        int current=Function.Call<int>(Hash.GET_CURRENT_PED_WEAPON_ENTITY_INDEX,ped.Handle,false);
        using(var clip=new OutputArgument()){
            bool read=Function.Call<bool>(Hash.GET_AMMO_IN_CLIP,ped.Handle,weapon,clip);
            ProbeLog(String.Format("{0} attempt={1} current={2} nearestLeft={3} nearestRight={4} selected={5:X8} total={6} clip={7} wheelSeen={8}",
                phase,attempt,DescribeObject(current,left,right),DescribeObject(nearLeft,left,right),DescribeObject(nearRight,left,right),
                Function.Call<uint>(Hash.GET_SELECTED_PED_WEAPON,ped.Handle),Function.Call<int>(Hash.GET_AMMO_IN_PED_WEAPON,ped.Handle,weapon),
                read?clip.GetResult<int>():-1,wheelSeen));
        }
    }
    void TestHandoff(object sender,EventArgs args){
        var module=GetModuleHandle(LayerModule);
        if(module==IntPtr.Zero){poll=null;bound=IntPtr.Zero;return;}
        if(module!=bound){
            var entry=GetProcAddress(module,"PollHolster");
            if(entry==IntPtr.Zero)return;
            poll=(ReadTap)Marshal.GetDelegateForFunctionPointer(entry,typeof(ReadTap));bound=module;
            ProbeLog("Session started");
        }
        var ped=Game.Player.Character;
        bool active=ped.Exists()&&!ped.IsDead&&!ped.IsInVehicle()&&!Game.IsPaused&&!Function.Call<bool>(Hash.IS_CUTSCENE_ACTIVE);
        // No inventory cards or wheel routes. Only the right-hand tap on the right hip is acted on.
        int gesture=poll(active?201:0);
        if(!active){if(probeEnd!=0)ProbeLog("Sampling interrupted");probeEnd=0;return;}
        int now=Game.GameTime;
        if(gesture==75){
            if(!Function.Call<bool>(Hash.HAS_PED_GOT_WEAPON,ped.Handle,weapons[0],false)){
                GTA.UI.Screen.ShowSubtitle("Handoff test needs the standard Pistol in your inventory.");return;
            }
            attempt++;wheelSeen=false;Sample(ped,"before request");
            Function.Call(Hash.SET_CURRENT_PED_WEAPON,ped.Handle,weapons[0],true);
            probeEnd=now+60000;nextSample=now;
            GTA.UI.Screen.ShowSubtitle("Native pistol request "+attempt);
        }
        if(probeEnd==0)return;
        wheelSeen|=Function.Call<bool>(Hash.IS_HUD_COMPONENT_ACTIVE,19);
        if(now>=nextSample){Sample(ped,"observed");nextSample=now+450;}
        if(now>=probeEnd){probeEnd=0;ProbeLog("Sampling complete");}
    }
}
