#pragma once
#include "bridge.h"
#include "appearance.h"
// Separate versioned POD export, preserving the existing GxApi/overlay/radial
// layouts. Only a same-cohort bridge with this export may be paired with it.
typedef int(GX_CALL*GxSetMenuAppearance)(void*,const menyoo_vr::Appearance*);
