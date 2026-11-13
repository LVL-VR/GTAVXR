#pragma once
#include <math.h>
#include <float.h>
#include <stdint.h>
// POD configuration and zoom only; no native Camera::Update or global setters.
namespace gx_scope_camera
{
enum SourceMode
{
    Unavailable = 0,
    EyeFinalCrop = 1,
    NativeOpticalView = 2
};
struct Config
{
    uint32_t weapon, component, camera, reticle;
    double baseFov, minFov, maxFov, nearClip;
    bool variable;
    Config()
        : weapon(0), component(0), camera(0), reticle(0), baseFov(0), minFov(0), maxFov(0), nearClip(0),
          variable(false)
    {
    }
    bool valid() const
    {
        return weapon && camera && reticle && _finite(baseFov) && _finite(minFov) && _finite(maxFov) &&
               _finite(nearClip) && baseFov > 0 && baseFov < 179 && minFov > 0 && maxFov < 179 &&
               minFov <= maxFov && nearClip > 0 && nearClip < 100;
    }
    bool same(const Config& o) const
    {
        return weapon == o.weapon && component == o.component && camera == o.camera && reticle == o.reticle &&
               baseFov == o.baseFov && minFov == o.minFov && maxFov == o.maxFov && nearClip == o.nearClip &&
               variable == o.variable;
    }
};
inline double radians(double degrees)
{
    return degrees * 0.017453292519943295;
}
class Zoom
{
    Config config_;
    double fov_;
    bool held_;

  public:
    Zoom() : fov_(0), held_(false) {}
    void configure(const Config& c)
    {
        if (!c.valid())
        {
            *this = Zoom();
            return;
        }
        if (!config_.same(c))
        {
            config_ = c;
            fov_ = c.baseFov < c.minFov ? c.minFov : (c.baseFov > c.maxFov ? c.maxFov : c.baseFov);
            held_ = false;
        }
    }
    bool ready() const
    {
        return config_.valid() && fov_ >= config_.minFov && fov_ <= config_.maxFov;
    }
    double fov() const
    {
        return fov_;
    }
    double halfAngle() const
    {
        return ready() ? radians(config_.maxFov) * .5 : 0;
    }
    double magnification() const
    {
        return ready() ? tan(halfAngle()) / tan(radians(fov_) * .5) : 1;
    }
    // A dedicated producer ALREADY renders the effective FOV; never crop-zoom twice.
    double compositionMagnification(SourceMode source) const
    {
        return source == EyeFinalCrop ? magnification() : 1;
    }
    bool update(bool viewing, double stick)
    {
        if (!ready() || !viewing || !_finite(stick))
        {
            held_ = false;
            return false;
        }
        if (fabs(stick) < .25)
        {
            held_ = false;
            return false;
        }
        if (!config_.variable || fabs(stick) < .65 || held_)
            return false;
        held_ = true;
        // Same native maxFov / zoomFactor relation, with discrete VR stick gestures.
        const double next = stick > 0 ? fov_ * .5 : fov_ * 2;
        fov_ = next < config_.minFov ? config_.minFov : (next > config_.maxFov ? config_.maxFov : next);
        return true;
    }
};
}
