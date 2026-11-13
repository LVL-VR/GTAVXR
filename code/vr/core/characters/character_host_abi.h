#pragma once
#include <stdint.h>
namespace menyoo_vr
{
// POD source catalogue. The caller owns output storage for this synchronous
// UPDATE operation; no engine entity pointer crosses the DLL boundary.
enum SourceCatalogue
{
    CataloguePed,
    CatalogueVehicle,
    CatalogueWeapon
};
struct CatalogueEntry
{
    uint32_t size, total, hash, category;
    char name[64];
};
static const uint64_t SourceHostCatalogueRead = UINT64_C(0xfffffffffffff200);
static const uint64_t SourceHostPlayerModelRequest = UINT64_C(0xfffffffffffff201);
static const uint64_t SourceHostRestoreStory = UINT64_C(0xfffffffffffff202);
static const uint64_t SourceHostSwitchStory = UINT64_C(0xfffffffffffff203);
}
