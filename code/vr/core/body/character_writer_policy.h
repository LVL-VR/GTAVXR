#pragma once
// Native adapter supplies the eight bones it actually reads/writes. No
// optional IK/torso/gait tag, model hash or native pointer is stored here.
namespace gx_character_writer
{
enum ArmBone
{
    L_UPPER,
    L_FORE,
    L_HAND,
    L_PH,
    R_UPPER,
    R_FORE,
    R_HAND,
    R_PH,
    ARM_COUNT
};
template <class Bones> inline unsigned armMask(const Bones& bones)
{
    unsigned mask = 0;
    for (unsigned j = 0; j < ARM_COUNT; ++j)
        if (bones.writable((ArmBone)j))
            mask |= 1u << j;
    return mask;
}
inline bool armReady(unsigned mask)
{
    return mask == 255u;
}
inline bool publish(bool seated, bool neutralSpineRoot, unsigned armsWritten)
{
    return armsWritten < 4 && (seated || neutralSpineRoot || armsWritten != 0);
}
}
