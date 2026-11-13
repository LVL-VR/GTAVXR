#pragma once
#include <stdint.h>
// Separate versioned native-render transport. No strings/allocators/STL cross
// the modern module <-> VS2012 boundary. Not the existing GxOverlay ABI.
namespace menyoo_vr
{
enum
{
    PresentationVersion = 1,
    MaxPrimitives = 64
};
struct DrawPrimitive
{
    uint32_t kind, font, red, green, blue, alpha, centered, rightAligned, outline, reserved;
    float x, y, width, height, rotation, scale;
    char text[512], dictionary[48], texture[64];
};
struct DrawFrame
{
    uint32_t size, version, count, selected, total, offset, reserved[2];
    uint64_t generation;
    float cropLeft, cropTop, cropRight, cropBottom;
    DrawPrimitive commands[MaxPrimitives];
};
static_assert(sizeof(DrawFrame) < 65536, "Native buffered draw packet must fit a bounded data block");
inline bool validDrawFrame(const DrawFrame& f)
{
    if (f.size != sizeof(f) || f.version != PresentationVersion || f.count > MaxPrimitives || !f.generation ||
        f.reserved[0] || f.reserved[1])
        return false;
    if (f.count &&
        (!f.total || f.selected >= f.total || f.offset > f.selected || f.selected - f.offset >= 14))
        return false;
    if (!(f.cropLeft >= 0 && f.cropTop >= 0 && f.cropRight <= 1 && f.cropBottom <= 1 &&
          f.cropRight > f.cropLeft && f.cropBottom > f.cropTop))
        return false;
    for (unsigned i = 0; i < f.count; ++i)
    {
        const DrawPrimitive& c = f.commands[i];
        if (c.kind > 2 || c.font > 7 || c.red > 255 || c.green > 255 || c.blue > 255 || c.alpha > 255 ||
            c.centered > 1 || c.rightAligned > 1 || c.outline > 1 || c.reserved)
            return false;
        if (!(c.x >= 0 && c.x <= 1 && c.y >= 0 && c.y <= 1 && c.width >= 0 && c.width <= 1 && c.height >= 0 &&
              c.height <= 1 && c.rotation >= -360 && c.rotation <= 360 && c.scale >= 0 && c.scale <= 2))
            return false;
        bool text = false, dictionary = false, texture = false;
        for (unsigned j = 0; j < sizeof(c.text); ++j)
            text = text || c.text[j] == 0;
        for (unsigned j = 0; j < sizeof(c.dictionary); ++j)
            dictionary = dictionary || c.dictionary[j] == 0;
        for (unsigned j = 0; j < sizeof(c.texture); ++j)
            texture = texture || c.texture[j] == 0;
        if (!text || !dictionary || !texture)
            return false;
        if (c.kind == 2 && (!c.text[0] || !c.scale))
            return false;
        if (c.kind == 1 && (!c.dictionary[0] || !c.texture[0] || !c.width || !c.height))
            return false;
    }
    return true;
}
}
