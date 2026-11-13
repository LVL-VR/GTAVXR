#pragma once
#include <stdint.h>
namespace menyoo_vr {
// Request reuses the accepted asynchronous, offline/native spawn pipeline.
// No stock ASI CREATE_VEHICLE/network-control wait or retail memory layouts.
static const uint64_t SourceHostVehicleSpawnRequest=UINT64_C(0xfffffffffffff400);
enum TrainerAction { TrainerGiveAllWeapons,TrainerGiveMaxAmmo,TrainerRepairVehicle,TrainerCleanVehicle,TrainerOpenLegacy,TrainerActionCount };
static const uint64_t SourceHostTrainerAction=UINT64_C(0xfffffffffffff401);
static const uint64_t SourceHostWantedRead=UINT64_C(0xfffffffffffff403);
static const uint64_t SourceHostWantedAdjust=UINT64_C(0xfffffffffffff404);
static const uint64_t SourceHostWantedSet=UINT64_C(0xfffffffffffff405);
}
