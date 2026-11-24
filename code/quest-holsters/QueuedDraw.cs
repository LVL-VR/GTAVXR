using System;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using GTA;
using GTA.Math;
using GTA.Native;

public sealed partial class QuestInventory {
    // Verified against this release's deferred-request writer. The game
    // owns synchronization, expiration and the eventual hand-specific equip.
    [UnmanagedFunctionPointer(CallingConvention.Winapi)]
    delegate void SubmitDraw(IntPtr unusedContext,int hand,uint weapon,int epoch);
    SubmitDraw submit;
    IntPtr imageBase;
    bool initialized;
    int nextDraw;
    [StructLayout(LayoutKind.Sequential)]
    struct HipPress { public int zone,age; public float x,y,z; }
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    delegate int TakePress(int hand,out HipPress press);
    TakePress takePress;
    // At a hip tap the two wrists can still be close. An object visible to
    // both proximity queries only belongs to the closer wrist.
    internal static int PistolAtWrist(float here,float other){
        if(here>.32f || other+.045f<here)return 0;
        return here+.045f<other?1:-1;
    }
    int ReadTargetAtHand(Ped ped,int hand,uint hash){
        var here=Function.Call<Vector3>(Hash.GET_PED_BONE_COORDS,ped.Handle,hand==0?0x49D9:0xDEAD,0f,0f,0f);
        var other=Function.Call<Vector3>(Hash.GET_PED_BONE_COORDS,ped.Handle,hand==0?0xDEAD:0x49D9,0f,0f,0f);
        uint model=Function.Call<uint>(Hash.GET_WEAPONTYPE_MODEL,hash);
        int obj=Function.Call<int>(Hash.GET_CLOSEST_OBJECT_OF_TYPE,here.X,here.Y,here.Z,.32f,model,false,false,false);
        if(obj==0 || !Function.Call<bool>(Hash.DOES_ENTITY_EXIST,obj))return 0;
        var pos=Function.Call<Vector3>(Hash.GET_ENTITY_COORDS,obj,false);
        return PistolAtWrist(pos.DistanceTo(here),pos.DistanceTo(other));
    }
    void BindDraw(){
        initialized=true;
        using(var process=Process.GetCurrentProcess()){
            var image=process.MainModule;
            using(var stream=File.OpenRead(image.FileName))
            using(var sha=SHA256.Create()){
                if(BitConverter.ToString(sha.ComputeHash(stream)).Replace("-","")!="A6776E715859B9299A2951018FE639198FA7B7CCEA5322985B6A06C99ABE390D"){
                    ProbeLog("Unsupported executable");return;
                }
            }
            imageBase=image.BaseAddress;
            submit=(SubmitDraw)Marshal.GetDelegateForFunctionPointer(IntPtr.Add(imageBase,0x1dac90),typeof(SubmitDraw));
        }
        ProbeLog("Native draw ready");
    }
    uint sampledWeapon;
    readonly int[] requestedInHand={-1,-1};
    int requestedActor;
    int VerifiedRequestedItem(Ped ped,int hand){
        int item=requestedInHand[hand];
        if(requestedActor!=ped.Handle||item<0||!Owns(item))return -1;
        return ReadTargetAtHand(ped,hand,weapons[item])==1?item:-1;
    }
    void RequestSavedWeapon(Ped ped,int hand,int item,bool card){
        uint hash=weapons[item];
        int held=hash==Unarmed?0:ReadTargetAtHand(ped,hand,hash);
        if(held<0){GTA.UI.Screen.ShowSubtitle("Separate your hands and tap again.");return;}
        if(card && held==1)return;
        int epoch=Marshal.ReadInt32(imageBase,0x27b2670);
        if(epoch==0 || Marshal.ReadInt64(imageBase,0x2ac3030)!=0){
            GTA.UI.Screen.ShowSubtitle("Weapon change pending. Tap again.");return;
        }
        attempt++;wheelSeen=false;sampledWeapon=hash;
        string action=(hand==0?"left ":"right ")+(held==1?"stow ":"draw ")+(WeaponHash)hash;
        Sample(ped,"before "+action);
        submit(IntPtr.Zero,hand,held==1?Unarmed:hash,epoch);
        if(requestedActor!=ped.Handle){requestedInHand[0]=requestedInHand[1]=-1;requestedActor=ped.Handle;}
        requestedInHand[hand]=held==1||hash==Unarmed?-1:item;
        ProbeLog("Submitted "+action);
        nextSample=Game.GameTime;probeEnd=nextSample+2000;
    }
    void TestQueuedDraw(object sender,EventArgs args){
        var module=GetModuleHandle(LayerModule);
        if(module==IntPtr.Zero){bound=IntPtr.Zero;poll=null;return;}
        if(module!=bound){
            var entry=GetProcAddress(module,"PollHolster");if(entry==IntPtr.Zero)return;
            var diag=GetProcAddress(module,"TakeHipPress");if(diag==IntPtr.Zero)return;
            takePress=(TakePress)Marshal.GetDelegateForFunctionPointer(diag,typeof(TakePress));
            poll=(ReadTap)Marshal.GetDelegateForFunctionPointer(entry,typeof(ReadTap));bound=module;
        }
        if(!initialized)BindDraw();
        var ped=Game.Player.Character;
        bool wheel=Function.Call<bool>(Hash.IS_HUD_COMPONENT_ACTIVE,19);
        bool active=submit!=null && ped.Exists() && !ped.IsDead && !ped.IsInVehicle() && !Game.IsPaused &&
            !Function.Call<bool>(Hash.IS_CUTSCENE_ACTIVE) && !wheel;
        int gesture=poll(active?9:0);
        for(int i=0;i<2;i++){
            HipPress press;
            if(takePress(i,out press)!=0)ProbeLog(String.Format("grip hand={0} zone={1} poseAgeMs={2} pos={3:F2},{4:F2},{5:F2} eligible={6}",
                i,press.zone,press.age,press.x,press.y,press.z,active));
        }
        int now=Game.GameTime;
        if(probeEnd!=0 && ped.Exists()){
            wheelSeen|=wheel;
            if(now>=nextSample){Sample(ped,"after request");nextSample=now+200;}
            if(now>=probeEnd)probeEnd=0;
        }
        if(!active || (gesture!=40 && gesture!=41))return;
        if(now<nextDraw){ProbeLog("Hip tap during equip cooldown");return;}
        if(!Function.Call<bool>(Hash.HAS_PED_GOT_WEAPON,ped.Handle,weapons[0],false)){
            ProbeLog("Hip tap without owned pistol");GTA.UI.Screen.ShowSubtitle("This test requires the standard Pistol.");return;
        }
        int epoch=Marshal.ReadInt32(imageBase,0x27b2670);
        if(epoch==0 || Marshal.ReadInt64(imageBase,0x2ac3030)!=0){ProbeLog("Skipped busy or inactive request");return;}
        int hand=gesture-40;
        int held=ReadTargetAtHand(ped,hand,weapons[0]);
        if(held<0){ProbeLog("Hip tap with ambiguous pistol position");GTA.UI.Screen.ShowSubtitle("Separate your hands and tap again.");return;}
        string action=(hand==0?"left ":"right ")+(held==1?"stow":"draw");
        attempt++;wheelSeen=false;Sample(ped,"before "+action);
        submit(IntPtr.Zero,hand,held==1?Unarmed:weapons[0],epoch);
        nextDraw=now+1000;nextSample=now;probeEnd=now+2500;
        ProbeLog("Submitted "+action+"; epoch="+epoch);
    }
}
