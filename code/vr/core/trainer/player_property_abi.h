#pragma once
#include <stdint.h>
namespace menyoo_vr
{
enum PlayerProperty
{
    PlayerInvincibility,
    PlayerInvisibility,
    PlayerNoRagdoll,
    PlayerSeatbelt,
    PlayerPropertyCount
};
static const uint64_t SourceHostPlayerEpoch = UINT64_C(0xfffffffffffff300);
static const uint64_t SourceHostReadPlayerProperty = UINT64_C(0xfffffffffffff310);
static const uint64_t SourceHostWritePlayerProperty = UINT64_C(0xfffffffffffff320);
}
