#pragma once
// Normalized descriptor/lifetime screening; real D3D enums are mapped by the
 
namespace gx_ui_depth
{
enum Family
{
    UNSUPPORTED,
    D24S8,
    D32FS8
};
struct Shape
{
    unsigned width, height, samples, quality, array, mips;
    Family resource, view;
    unsigned flags;
    bool multisampleView;
    Shape()
        : width(0), height(0), samples(0), quality(0), array(0), mips(0), resource(UNSUPPORTED),
          view(UNSUPPORTED), flags(0), multisampleView(false)
    {
    }
};
inline bool sameSampling(const Shape& a, const Shape& b)
{
    return a.width == b.width && a.height == b.height && a.samples == b.samples && a.quality == b.quality;
}
inline bool qualified(const Shape& color, const Shape& depth)
{
    return depth.width && depth.height && depth.width <= 3840 && depth.height <= 2160 &&
           (depth.samples == 1 || depth.samples == 2 || depth.samples == 4 || depth.samples == 8) &&
           depth.quality <= 255 && color.array == 1 && color.mips == 1 && depth.array == 1 &&
           depth.mips == 1 && sameSampling(color, depth) &&
           (depth.resource == D24S8 || depth.resource == D32FS8) && depth.view == depth.resource &&
           !depth.flags && depth.multisampleView == (depth.samples > 1);
}
inline bool clone(const Shape& color, const Shape& original, const Shape& owned, bool aliases)
{
    return !aliases && qualified(color, original) && qualified(color, owned) &&
           sameSampling(original, owned) && owned.resource == original.resource &&
           owned.view == original.view && owned.flags == original.flags;
}
class Lifetime
{
    bool busy_, pending_;
    unsigned creates_, closes_;

  public:
    Lifetime() : busy_(false), pending_(false), creates_(0), closes_(0) {}
    bool begin()
    {
        if (busy_ || pending_)
            return false;
        busy_ = true;
        return true;
    }
    void end()
    {
        busy_ = false;
    }
    void created()
    {
        ++creates_;
    }
    bool close(bool quiescent, bool locked)
    {
        if (busy_ || !quiescent || locked)
        {
            pending_ = true;
            return false;
        }
        pending_ = false;
        ++closes_;
        return true;
    }
    bool busy() const
    {
        return busy_;
    }
    bool pending() const
    {
        return pending_;
    }
    unsigned creates() const
    {
        return creates_;
    }
    unsigned closes() const
    {
        return closes_;
    }
};
}
