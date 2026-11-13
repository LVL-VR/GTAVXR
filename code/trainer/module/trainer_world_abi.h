#pragma once
#include <stdint.h>
namespace menyoo_vr {
// typed host operations for the retained Trainer pages. The EXE implements them over game objects
// (src/legacy3889/native_trainer_world137.inl); the DLL never reads engine memory.
// SourceHostEntityQuery(arg0=EntityQuery*): fills script GUIDs of source pool entities within radius of (x,y,z),
// excluding the local player ped. Only entities that already have or can register a script GUID are returned.
static const uint64_t SourceHostEntityQuery=UINT64_C(0xfffffffffffff710);
enum EntityQueryType { QueryPeds=1u,QueryVehicles=2u,QueryObjects=4u,QueryAll=7u };
struct EntityQuery {uint32_t size,types;float x,y,z,radius;uint32_t capacity,count;int*handles;};
// SourceHostVariationInfo(arg0=VariationQuery*): Wardrobe Collection / Local ID (Menyoo BuildDrawableCollectionData /
// BuildPropCollectionData without retail pointers). For ped (script GUID) component slot (isProp=0, 0..11) or prop anchor
// (isProp=1, 0..9) the EXE walks the global indices of CPedModelInfo::GetVarInfo() with GetVariationInfoFromDrawableIdx /
// GetVariationInfoFromPropIdx and returns the consecutive collection runs in order: DLC name (base = "basegame"), first
// global index and count. Local ID = global - first. Reading only; the DLL applies with the retained variation natives.
static const uint64_t SourceHostVariationInfo=UINT64_C(0xfffffffffffff711);
struct VariationCollection {char name[64];uint32_t first,count;};
struct VariationQuery {uint32_t size;int ped,isProp,slot;uint32_t capacity,count,total,status;VariationCollection*collections;};
enum VariationStatus { VariationOk=0,VariationNoPed=1,VariationBadSlot=2,VariationNoCollection=3,VariationOverflow=4 };
// 0x...f712 is intentionally unused: Menyoo FixDoor is the ORIGINAL SET_VEHICLE_DOOR_CONTROL(door, DT_DOOR_INTACT) native.
// SourceHostVehicleGuard(arg0=mode,arg1=value): the accepted spawn guards for vehicles the Trainer script creates with the
// ORIGINAL CREATE_VEHICLE (Menyoo SpawnVehicle). GuardModel(value=model hash): loaded vehicle model with drawable and a
// CSE_VEHICLE master shader (same gx_trainer::modelShaderFailure contract). GuardInstance(value=vehicle script GUID):
// draw handler with a CSE_VEHICLE effect, then the offline MPBitset bit30 exemption on that vehicle only.
// Success = qualified; any other status = reject (the DLL deletes its own vehicle).
static const uint64_t SourceHostVehicleGuard=UINT64_C(0xfffffffffffff713);
enum VehicleGuardMode { GuardModel=0u,GuardInstance=1u };
}
