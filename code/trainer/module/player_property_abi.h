#pragma once
#include <stdint.h>
namespace menyoo_vr {
enum PlayerProperty { PlayerInvincibility,PlayerInvisibility,PlayerNoRagdoll,PlayerSeatbelt,PlayerPropertyCount };
static const uint64_t SourceHostPlayerEpoch=UINT64_C(0xfffffffffffff300);
static const uint64_t SourceHostReadPlayerProperty=UINT64_C(0xfffffffffffff310);
static const uint64_t SourceHostWritePlayerProperty=UINT64_C(0xfffffffffffff320);
// Invincibility preserves all8 proofs, general immunity and drowning. No
// blanket reset to guessed GTA defaults. NoRagdoll preserves both lock/impact.
inline uint64_t playerEnabledValue(unsigned property){return property==PlayerInvincibility?511u:(property==PlayerNoRagdoll?3u:1u);}
inline uint64_t playerDisabledValue(unsigned property){return property==PlayerInvincibility?512u:(property==PlayerSeatbelt?4u:0u);}
inline bool playerPropertyEnabled(unsigned property,uint64_t value){return property==PlayerSeatbelt?(value&7u)==1u:(value&1u)!=0;}
}
