#pragma once

namespace gx_weapon_buttons
{
inline float support(bool swapped, float trigger, float grip)
{
    return swapped ? grip : trigger;
}
inline float part(bool swapped, float trigger, float grip)
{
    return swapped ? trigger : grip;
}
}
