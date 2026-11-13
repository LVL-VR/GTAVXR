#pragma once
#include <stdint.h>
namespace gx_body_roles
{
inline uint32_t components(unsigned count)
{
    return count == 32 ? UINT32_MAX : count && count < 32 ? ((uint32_t(1) << count) - 1) : 0;
}
// Filter solver contacts only. Anatomical bounds/query geometry remain native.
struct MaskLease
{
    uint32_t before, applied;
    bool owned;
    MaskLease() : before(0), applied(0), owned(false) {}
    bool acquire(uint32_t current, unsigned count, uint32_t arms)
    {
        const uint32_t all = components(count);
        arms &= all;
        if (owned || !all || !arms)
            return false;
        before = current;
        applied = current | (all & ~arms);
        owned = true;
        return true;
    }
    uint32_t restore(uint32_t current)
    {
        // Do not overwrite a native/debug owner's intervening policy.
        const uint32_t result = owned && current == applied ? before : current;
        owned = false;
        return result;
    }
};
}
