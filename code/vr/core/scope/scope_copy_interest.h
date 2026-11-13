#pragma once
#include <stdint.h>
// Caller serializes with EXISTING ScopeCopyMetadataGuard. Address VALUES only;
// no pointer is dereferenced, no first native copy is reconstructed retroactively.
namespace gx_scope_copy
{
class Interest
{
    uintptr_t a_[2];

  public:
    Interest()
    {
        replace(0, 0);
    }
    void replace(uintptr_t a, uintptr_t b)
    {
        a_[0] = a;
        a_[1] = b;
    }
    bool contains(uintptr_t skel) const
    {
        return skel && (a_[0] == skel || a_[1] == skel);
    }
};
}
