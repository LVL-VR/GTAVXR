#pragma once
#include "../core/presentation_abi.h"
namespace menyoo_vr
{
enum
{
    SharedMenuVersion = 1,
    SharedMenuRows = 14
};
struct SharedMenuRow
{
    uint32_t kind, enabled, submenu;
    char label[256], value[64], help[512];
};
struct SharedMenu
{
    uint32_t size, version, selected, total, offset, count;
    uint64_t generation;
    char title[256];
    SharedMenuRow rows[SharedMenuRows];
};
typedef int (*PresentSharedMenu)(const SharedMenu*, DrawFrame*);
}
