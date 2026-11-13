#pragma once
#include <stdint.h>
namespace menyoo_vr {
enum LegacyCheat { LegacyUnlimitedAbility,LegacyInfiniteAmmo,LegacyInfiniteClip,LegacyFireAmmo,LegacyExplosiveAmmo,LegacyExplosiveMelee,LegacyNeverWanted,LegacyCheatCount };
static const uint64_t SourceHostReadLegacyCheat=UINT64_C(0xfffffffffffff500);
static const uint64_t SourceHostWriteLegacyCheat=UINT64_C(0xfffffffffffff501);
}
