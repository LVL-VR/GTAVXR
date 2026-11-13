#pragma once
#include "module_abi_v2.h"
namespace menyoo_vr
{
// Same POD layout, a STRICT new protocol tag for the additional controller
// event. A v2 DLL/provider pair must never accept an unnegotiated v3 input.
enum
{
    ModuleAbiVersion3 = 3
};
enum ModuleInput3
{
    ModuleFavourite = 10
};
typedef Input2 Input3;
typedef Frame2 Frame3;
typedef Module2 Module3;
typedef GetModule2 GetModule3;
typedef ConfigureModule2 ConfigureModule3;
}
