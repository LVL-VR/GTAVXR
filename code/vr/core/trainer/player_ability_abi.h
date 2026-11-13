#pragma once
#include <stdint.h>
namespace menyoo_vr
{
// build136: retained Menyoo Player abilities (Trainer V3 block P04). One typed host operation per module tick:
// arguments[0]=ability mask, arguments[1]=Forcefield mode (0 Off, 1 Push Out, 2 Destroy). A mask of 0 stops every
// effect and restores what the host captured on the same ped. result[0] echoes the applied mask.
static const uint64_t SourceHostPlayerAbilities = UINT64_C(0xfffffffffffff700);
enum PlayerAbility
{
    AbilitySuperRun = 1,
    AbilityWalkUnderwater = 2,
    AbilityForcefield = 4,
    AbilitySmash = 8,
    AbilityFlyAuto = 16,
    AbilityIgnored = 32,
    AbilityAll = 63
};
enum ForcefieldMode
{
    ForcefieldOff = 0,
    ForcefieldPushOut = 1,
    ForcefieldDestroy = 2,
    ForcefieldModeCount = 3
};
}
