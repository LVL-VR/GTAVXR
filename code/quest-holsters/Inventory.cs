using System;
using System.IO;
using System.Globalization;
using System.Runtime.InteropServices;
using GTA;
using GTA.Native;

public sealed partial class QuestInventory : Script {
    const string LayerModule="QuestFourSlotsXR.dll", LayerName="XR_APILAYER_GTAVXR_quest_holsters", SlotsFile="four-native-slots.txt";
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode)] static extern IntPtr GetModuleHandle(string name);
    [DllImport("kernel32.dll", CharSet=CharSet.Ansi, ExactSpelling=true)] static extern IntPtr GetProcAddress(IntPtr module,string name);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int ReadTap(int mode);
    ReadTap poll;
    IntPtr bound;
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate void Report(int slot,int visible,int match);
    Report report;
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int PanelState();
    PanelState panelState;
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate void RouteSlot(int hand,int item);
    RouteSlot route;
    readonly uint[] saved=new uint[4];
    readonly string[] slots={"Left hip","Right hip","Left shoulder","Right shoulder"};
    readonly string saveDir=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"GTAVXR-QuestHolsters");
    bool slowed;
    float previousScale;
    const float BrowsingScale=.25f;
    void SetBrowsing(bool open){
        if(open){
            if(!slowed){
                previousScale=Game.TimeScale;
                if(!(previousScale>0f && previousScale<=1f))previousScale=1f;
            }
            Function.Call(Hash.SET_TIME_SCALE,Math.Min(BrowsingScale,previousScale));
        }else if(slowed){
            Function.Call(Hash.SET_TIME_SCALE,previousScale);
        }
        slowed=open;
    }
    uint[] weapons;
    int[] inventory;
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate uint CatalogHash(int item);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate void SetInventory([In,MarshalAs(UnmanagedType.LPArray)] int[] flags);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate void VisibleItems([Out,MarshalAs(UnmanagedType.LPArray,SizeConst=8)] int[] items);
    VisibleItems readVisible;
    readonly int[] visible=new int[8];
    int scanItem;
    SetInventory reportInventory;
    bool Owns(int item){return inventory[item]!=0;}
    int owned,nextScan;
    const uint Unarmed=0xA2719263;
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    delegate void AmmoReport(int item,int clip,int total);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    delegate void SyncSlots([In,Out,MarshalAs(UnmanagedType.LPArray,SizeConst=4)] int[] indices);
    SyncSlots syncSlots;
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int SaveSlot(int hand,int slot,int item);
    SaveSlot saveSlot;
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate void SkipHand(int target);
    SkipHand skipEmptyHand;
    int AssignmentWeapon(Ped ped,int hand){
        int item=VerifiedRequestedItem(ped,hand);
        return item>=0?item:HeldItem(ped,hand);
    }
    void AssignHeld(Ped ped,int target){
        int hand=target/4,slot=target%4,item=AssignmentWeapon(ped,hand);
        ProbeLog("Overwrite hold completed: "+slots[slot]+", "+(hand==0?"left":"right")+" hand, detected="+(item<0?"none":((WeaponHash)weapons[item]).ToString()));
        if(item<0||weapons[item]==Unarmed){GTA.UI.Screen.ShowSubtitle("No weapon identified in that hand. Holster unchanged.");return;}
        if(saveSlot(hand,slot,item)==0){GTA.UI.Screen.ShowSubtitle("Could not save the holster.");return;}
        ReadEditorSlots();
        GTA.UI.Screen.ShowSubtitle(slots[slot]+" saved: "+(WeaponHash)weapons[item]);
        ProbeLog("Assigned "+slots[slot]+" from "+(hand==0?"left":"right")+" hand: "+(WeaponHash)weapons[item]);
    }
    void ReadEditorSlots(){
        int[] indices=Array.ConvertAll(saved,w=>Array.IndexOf(weapons,w));
        syncSlots(indices);
        for(int i=0;i<4;i++)saved[i]=indices[i]>=0 && indices[i]<weapons.Length?weapons[indices[i]]:0;
    }
    AmmoReport reportAmmo;
    ulong nextAmmoRead;
    void RefreshCardAmmo(Ped ped,bool browsing){
        if(!browsing){nextAmmoRead=0;return;}
        ulong now=(uint)Environment.TickCount;
        if(nextAmmoRead!=0 && now<nextAmmoRead && nextAmmoRead-now<=200)return;
        nextAmmoRead=now+200;
        using(var rounds=new OutputArgument()){
            readVisible(visible);
            foreach(int item in visible){
                if(item<0||!Owns(item))continue;
                uint weapon=weapons[item];
                int total=Function.Call<int>(Hash.GET_AMMO_IN_PED_WEAPON,ped.Handle,weapon);
                int clip=Function.Call<bool>(Hash.GET_AMMO_IN_CLIP,ped.Handle,weapon,rounds)?rounds.GetResult<int>():-1;
                reportAmmo(item,clip,total);
            }
        }
    }
    int directPed,pendingDirect=-1;
    DateTime verifyAfter,deadline;
    bool pendingCard;
    void RecordSelection(int hand,int item,string result){
        try{
            Directory.CreateDirectory(saveDir);
            File.AppendAllText(Path.Combine(saveDir,"inventory-selection.log"),
                String.Format("{0:O} {1} {2} {3}\n",DateTime.UtcNow,hand==1?"right":"left",(WeaponHash)weapons[item],result));
        }catch(IOException){}catch(UnauthorizedAccessException){}
    }
    void ForgetDirect(){
        pendingDirect=-1;directPed=0;pendingCard=false;
    }
    bool OnlyRightEquipped(Ped ped,int current,uint selected){
        if(current==0 || !Function.Call<bool>(Hash.DOES_ENTITY_EXIST,current))return false;
        if(Function.Call<uint>(Hash.GET_ENTITY_MODEL,current)!=Function.Call<uint>(Hash.GET_WEAPONTYPE_MODEL,selected))return false;
        var left=Function.Call<GTA.Math.Vector3>(Hash.GET_PED_BONE_COORDS,ped.Handle,0x49D9,0f,0f,0f);
        var right=Function.Call<GTA.Math.Vector3>(Hash.GET_PED_BONE_COORDS,ped.Handle,0xDEAD,0f,0f,0f);
        var gun=Function.Call<GTA.Math.Vector3>(Hash.GET_ENTITY_COORDS,current,false);
        float fromRight=gun.DistanceTo(right);
        if(fromRight>.55f || fromRight+.045f>=gun.DistanceTo(left))return false;
        for(int i=0;i<weapons.Length;i++){
            if(!Owns(i))continue;
            uint model=Function.Call<uint>(Hash.GET_WEAPONTYPE_MODEL,weapons[i]);
            int other=Function.Call<int>(Hash.GET_CLOSEST_OBJECT_OF_TYPE,left.X,left.Y,left.Z,.55f,model,false,false,false);
            if(other!=0 && other!=current)return false;
        }
        return true;
    }
    void SelectItem(Ped ped,int hand,int item,int held,bool card){
        if(card)route(hand,-2);
        RequestSavedWeapon(ped,hand,item,card);
    }
    void CheckDirect(Ped ped){
        if(directPed==0)return;
        if(ped.Handle!=directPed || Function.Call<bool>(Hash.IS_HUD_COMPONENT_ACTIVE,19)){ForgetDirect();return;}
        uint selected=Function.Call<uint>(Hash.GET_SELECTED_PED_WEAPON,ped.Handle);
        if(DateTime.UtcNow<verifyAfter)return;
        if(selected==weapons[pendingDirect] && HeldItem(ped,1)==pendingDirect){
            RecordSelection(1,pendingDirect,"direct equip verified");ForgetDirect();return;
        }
        if(DateTime.UtcNow>=deadline){
            int item=pendingDirect;ForgetDirect();route(1,item);RecordSelection(1,item,"direct equip unverified; wheel requested");
        }else verifyAfter=DateTime.UtcNow.AddMilliseconds(60);
    }
    public QuestInventory(){
        string file=Path.Combine(saveDir,SlotsFile);
        if(!File.Exists(file))file=Path.Combine(saveDir,"offhand-slots.txt");
        if(!File.Exists(file))file=Path.Combine(saveDir,"inventory-slots.txt");
        if(!File.Exists(file))file=Path.Combine(saveDir,"slots.txt");
        if(File.Exists(file)){
            var lines=File.ReadAllLines(file);
            for(int i=0;i<Math.Min(4,lines.Length);i++)uint.TryParse(lines[i],out saved[i]);
        }
        string layerDir=Path.Combine(saveDir,"layer");
        AddSetting("XR_API_LAYER_PATH",layerDir);
        const string mirror="XR_APILAYER_NOVENDOR_OBSMirror";
        string mirrorDll=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles),"OpenXR-OBSMirror",mirror+".dll");
        if(File.Exists(Path.Combine(layerDir,"OBSMirror.json")) && File.Exists(mirrorDll)){
            Environment.SetEnvironmentVariable("DISABLE_"+mirror,"1");
            AddSetting("XR_ENABLE_API_LAYERS",mirror);
        }
        AddSetting("XR_ENABLE_API_LAYERS",LayerName);
        Tick+=Update;
        Aborted+=delegate {SetBrowsing(false);if(poll!=null && bound==GetModuleHandle(LayerModule))poll(0);};
    }
    static void AddSetting(string name,string value){
        var current=Environment.GetEnvironmentVariable(name)??"";
        if(!Array.Exists(current.Split(';'),s=>String.Equals(s,value,StringComparison.OrdinalIgnoreCase)))
            Environment.SetEnvironmentVariable(name,current.Length==0?value:value+";"+current);
    }
    int HeldItem(Ped ped,int hand){
        var palm=Function.Call<GTA.Math.Vector3>(Hash.GET_PED_BONE_COORDS,ped.Handle,hand==0?0x49D9:0xDEAD,0f,0f,0f);
        var other=Function.Call<GTA.Math.Vector3>(Hash.GET_PED_BONE_COORDS,ped.Handle,hand==0?0xDEAD:0x49D9,0f,0f,0f);
        int current=Function.Call<int>(Hash.GET_CURRENT_PED_WEAPON_ENTITY_INDEX,ped.Handle,false);
        int found=-1;float nearest=.55f;
        if(current!=0 && Function.Call<bool>(Hash.DOES_ENTITY_EXIST,current)){
            var at=Function.Call<GTA.Math.Vector3>(Hash.GET_ENTITY_COORDS,current,false);
            uint selected=Function.Call<uint>(Hash.GET_SELECTED_PED_WEAPON,ped.Handle);
            int index=Array.IndexOf(weapons,selected);
            if(index>=0&&selected!=Unarmed&&at.DistanceTo(palm)<.32f&&at.DistanceTo(palm)+.045f<at.DistanceTo(other)
                && Function.Call<uint>(Hash.GET_ENTITY_MODEL,current)==Function.Call<uint>(Hash.GET_WEAPONTYPE_MODEL,selected))return index;
        }
        for(int i=0;i<weapons.Length;i++){
            if(!Owns(i))continue;
            uint model=Function.Call<uint>(Hash.GET_WEAPONTYPE_MODEL,weapons[i]);
            int obj=Function.Call<int>(Hash.GET_CLOSEST_OBJECT_OF_TYPE,palm.X,palm.Y,palm.Z,.55f,model,false,false,false);
            if(obj==0 || (obj!=current && !Function.Call<bool>(Hash.IS_ENTITY_ATTACHED_TO_ENTITY,obj,ped.Handle)))continue;
            var pos=Function.Call<GTA.Math.Vector3>(Hash.GET_ENTITY_COORDS,obj,false);
            float distance=pos.DistanceTo(palm);
            if(distance<nearest && distance+.045f<pos.DistanceTo(other)){nearest=distance;found=i;}
        }
        return found;
    }
    void UseSlot(Ped ped,int code){
        int slot=code%4,hand=(code/4)%2;
        ProbeLog(String.Format("{0} {1} via {2} hand; saved={3:X8}",code>=8?"Assign":"Tap",slots[slot],hand==0?"left":"right",saved[slot]));
        int held=code>=8?HeldItem(ped,hand):-1;
        if(code>=8){
            if(held<0){GTA.UI.Screen.ShowSubtitle("Assignment cancelled. Could not identify a weapon in that hand.");return;}
            var next=(uint[])saved.Clone();next[slot]=weapons[held];
            string path=Path.Combine(saveDir,SlotsFile),tmp=path+".tmp";
            try{
                Directory.CreateDirectory(saveDir);
                File.WriteAllLines(tmp,Array.ConvertAll(next,x=>x.ToString(CultureInfo.InvariantCulture)));
                if(File.Exists(path))File.Replace(tmp,path,null);else File.Move(tmp,path);
            }catch(IOException){GTA.UI.Screen.ShowSubtitle("Could not save holster assignment.");return;}
            catch(UnauthorizedAccessException){GTA.UI.Screen.ShowSubtitle("Could not save holster assignment.");return;}
            saved[slot]=next[slot];
            GTA.UI.Screen.ShowSubtitle(slots[slot]+" saved: "+((WeaponHash)next[slot]).ToString());return;
        }
        int item=Array.IndexOf(weapons,saved[slot]);
        if(item<0 || !Owns(item)){GTA.UI.Screen.ShowSubtitle(slots[slot]+" has no available assigned weapon.");return;}
        // Native tap stows an equipped match. Otherwise select exactly the saved weapon.
        SelectItem(ped,hand,item,held,false);
    }
    void Update(object sender,EventArgs args){
        var module=GetModuleHandle(LayerModule);
        if(module==IntPtr.Zero){ForgetDirect();SetBrowsing(false);bound=IntPtr.Zero;poll=null;return;}
        if(bound!=module){
            var fn=GetProcAddress(module,"PollHolster");var feedback=GetProcAddress(module,"WheelFeedback");
            var panel=GetProcAddress(module,"IsInventoryOpen");
            var slotFn=GetProcAddress(module,"ResolveInventoryHip");
            if(fn==IntPtr.Zero||feedback==IntPtr.Zero||panel==IntPtr.Zero||slotFn==IntPtr.Zero)return;
            route=(RouteSlot)Marshal.GetDelegateForFunctionPointer(slotFn,typeof(RouteSlot));
            panelState=(PanelState)Marshal.GetDelegateForFunctionPointer(panel,typeof(PanelState));
            var ammoFn=GetProcAddress(module,"ReportCardAmmo");
            var syncFn=GetProcAddress(module,"SyncSlots");
            var saveFn=GetProcAddress(module,"SaveSlotForHand");
            var skipFn=GetProcAddress(module,"SkipEmptyAssignmentHand");
            if(ammoFn==IntPtr.Zero||syncFn==IntPtr.Zero||saveFn==IntPtr.Zero||skipFn==IntPtr.Zero)return;
            skipEmptyHand=(SkipHand)Marshal.GetDelegateForFunctionPointer(skipFn,typeof(SkipHand));
            saveSlot=(SaveSlot)Marshal.GetDelegateForFunctionPointer(saveFn,typeof(SaveSlot));
            var hashFn=GetProcAddress(module,"CatalogHash");var inventoryFn=GetProcAddress(module,"SetInventory");
            var visibleFn=GetProcAddress(module,"VisibleItems");
            if(hashFn==IntPtr.Zero||inventoryFn==IntPtr.Zero||visibleFn==IntPtr.Zero)return;
            readVisible=(VisibleItems)Marshal.GetDelegateForFunctionPointer(visibleFn,typeof(VisibleItems));
            var hash=(CatalogHash)Marshal.GetDelegateForFunctionPointer(hashFn,typeof(CatalogHash));
            var hashes=new System.Collections.Generic.List<uint>();
            for(int i=0;hash(i)!=0;i++)hashes.Add(hash(i));
            weapons=hashes.ToArray();inventory=new int[weapons.Length];scanItem=0;nextScan=0;
            reportInventory=(SetInventory)Marshal.GetDelegateForFunctionPointer(inventoryFn,typeof(SetInventory));
            syncSlots=(SyncSlots)Marshal.GetDelegateForFunctionPointer(syncFn,typeof(SyncSlots));ReadEditorSlots();
            reportAmmo=(AmmoReport)Marshal.GetDelegateForFunctionPointer(ammoFn,typeof(AmmoReport));
            poll=(ReadTap)Marshal.GetDelegateForFunctionPointer(fn,typeof(ReadTap));
            report=(Report)Marshal.GetDelegateForFunctionPointer(feedback,typeof(Report));bound=module;
            GTA.UI.Screen.ShowSubtitle("Hold shoulder grip. Hover a category and click the stick to cycle. Release grip to equip.");
        }
        var ped=Game.Player.Character;
        bool active=ped.Exists()&&!ped.IsDead&&!ped.IsInVehicle()&&!Game.IsPaused&&!Function.Call<bool>(Hash.IS_CUTSCENE_ACTIVE);
        if(!initialized)BindDraw();
        bool wheel=Function.Call<bool>(Hash.IS_HUD_COMPONENT_ACTIVE,19);
        active=active && submit!=null;
        if(active)Function.Call(Hash.DISABLE_CONTROL_ACTION,0,37,true); // INPUT_SELECT_WEAPON
        if(probeEnd!=0 && ped.Exists()){
            wheelSeen|=wheel;
            if(Game.GameTime>=nextSample){Sample(ped,"after request");nextSample=Game.GameTime+200;}
            if(Game.GameTime>=probeEnd)probeEnd=0;
        }
        if(active && Game.GameTime>=nextScan){
            int end=Math.Min(scanItem+12,weapons.Length);
            for(int i=scanItem;i<end;i++)
                inventory[i]=weapons[i]==Unarmed||Function.Call<bool>(Hash.HAS_PED_GOT_WEAPON,ped.Handle,weapons[i],false)?1:0;
            scanItem=end;
            if(scanItem==weapons.Length){reportInventory(inventory);scanItem=0;nextScan=Game.GameTime+750;}
        }
        int e=poll(active?(201|(owned<<8)):0);
        if(active)CheckDirect(ped);else ForgetDirect();
        bool browsing=active && (panelState()!=0||pendingCard);
        SetBrowsing(browsing);
        RefreshCardAmmo(ped,browsing);
        if(e==500)GTA.UI.Screen.ShowSubtitle("Feel the pulse, then grip to move. Center the right stick, then hold it down to save and exit.",7000);
        if(active&&e>=620&&e<628){
            int target=e-620;
            int holding=AssignmentWeapon(ped,target/4);
            if(holding<0||weapons[holding]==Unarmed){
                skipEmptyHand(target);
                ProbeLog("Skipped empty overwrite candidate: "+slots[target%4]+", "+(target/4==0?"left":"right")+" hand");
            }else{
            GTA.UI.Screen.ShowSubtitle("Hold the right stick tilted down to save to "+slots[target%4]+".",750);
            ProbeLog("Overwrite hold started: "+slots[target%4]+", "+(target/4==0?"left":"right")+" hand");
            }
        }
        if(e>=650&&e<654)ProbeLog("Overwrite vibration "+((e%2==0)?"left":"right")+
            (e<652?": both pulses accepted by OpenXR":": confirmation timed out"));
        if(active&&e>=600&&e<608)AssignHeld(ped,e-600);
        if(e==550){ReadEditorSlots();GTA.UI.Screen.ShowSubtitle("Holsters saved.");}
        if(e==552)GTA.UI.Screen.ShowSubtitle("Move the holster clear of the other slots and release grip.");
        if(e==551)GTA.UI.Screen.ShowSubtitle("Could not save holsters. Center the right stick, then hold it down to retry.");
        const int equipBase=1000;
        if(e>=equipBase && e<equipBase+2*weapons.Length){
            int choice=e-equipBase,hand=choice/weapons.Length,item=choice%weapons.Length;
            SelectItem(ped,hand,item,-1,true);
        }
        if(e>=70 && e<86)UseSlot(ped,e-70);
        if(e>=100 && e<100+weapons.Length){
            int slot=e-100;
            bool visible=Function.Call<bool>(Hash.IS_HUD_COMPONENT_ACTIVE,19);
            bool matches=visible && Function.Call<uint>(Hash.HUD_GET_WEAPON_WHEEL_CURRENTLY_HIGHLIGHTED)==weapons[slot];
            if(visible&&!matches)Function.Call(Hash.HUD_SET_WEAPON_WHEEL_TOP_SLOT,weapons[slot]);
            report(slot,visible?1:0,matches?1:0);
        }
        if(e==60)GTA.UI.Screen.ShowSubtitle("Inventory selection cancelled.");
        if(e==61)GTA.UI.Screen.ShowSubtitle("No supported inventory weapons owned.");
    }
}
