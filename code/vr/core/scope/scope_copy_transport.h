#pragma once
#include "scope_copy_mode.h"
// POD interpretation only. No native pointer is followed; no lifetime/job fence.
namespace gx_scope_transport
{
struct Capture
{
    gx_scope_copy::Key key;
    gx_scope_copy::Mode mode;
    Capture() : mode(gx_scope_copy::Unknown) {}
    bool known() const
    {
        return key.valid() && (mode == gx_scope_copy::NormalRows || mode == gx_scope_copy::ModelRelativeRows);
    }
};
inline Capture executed(const Capture& captured, uintptr_t readBuffer, int nativeOffset, unsigned count,
                        uintptr_t nativeSet, uintptr_t installedSet)
{
    if (!captured.known() || captured.key.buffer != readBuffer || captured.key.offset != nativeOffset ||
        captured.key.count != count || !nativeSet || nativeSet != installedSet)
        return Capture();
    return captured; // captured stamp, NOT the builder's post-Flip mutable stamp
}
}
