#pragma once
#include <stddef.h>
#include <wchar.h>
namespace menyoo_vr
{
// Published trainer is beside the exact bridge selected by the launcher,
// not in the per-activation diagnostics namespace. No PATH/current-dir search.
inline bool TrainerSiblingPath(const wchar_t* bridge, wchar_t* out, size_t capacity)
{
    if (!bridge || !out || !capacity)
        return false;
    out[0] = 0;
    const size_t length = wcslen(bridge);
    if (length < 4 || bridge[1] != L':' || (bridge[2] != L'\\' && bridge[2] != L'/'))
        return false;
    size_t base = 0;
    for (size_t i = 0; i < length; ++i)
        if (bridge[i] == L'\\' || bridge[i] == L'/')
            base = i + 1;
    if (!base || _wcsicmp(bridge + base, L"gtavxr_native_openxr.dll"))
        return false;
    const wchar_t name[] = L"gtavxr_trainer.dll";
    const size_t count = sizeof(name) / sizeof(name[0]);
    if (base + count > capacity)
        return false;
    wmemcpy(out, bridge, base);
    wmemcpy(out + base, name, count);
    return true;
}
}
