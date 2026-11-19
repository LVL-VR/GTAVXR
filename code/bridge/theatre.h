#pragma once
// Stereo theatre geometry. GTAVXR-owned math, no graphics or runtime state.
//
// The filming camera is the game's cutscene camera, untouched. The two eyes are that camera moved
// sideways by a small separation, with off-axis frusta that share the authored frame at the convergence
// distance. What lies there appears on the screen surface, what is farther appears behind it and the
// horizon appears at the viewer's own infinity. The eye images keep square pixels: the frustum grows on
// one axis to match the eye image and the authored frame is the centred crop that the screen shows.
#include "bridge.h"
#include "frame.h"
#include "theatre_api.h"
#include <float.h>
#include <math.h>

namespace gx_theatre {
const double ScreenDistance = 3.0;  // metres from the head when the screen is placed
const double ScreenWidth = 4.2;     // metres; about 70 degrees wide from that distance
const double Convergence = 1.5;     // metres in front of the filming camera that land on the screen
const double DefaultIpd = .064;
const double AnchorHoldSeconds = 2; // a shorter interruption keeps the screen where it was

inline bool valid(const GxTheatre& r) {
    return r.size == sizeof(r) && r.version == 1 && !r.reserved &&
        _finite(r.vertical_fov_degrees) && r.vertical_fov_degrees >= 1 && r.vertical_fov_degrees <= 130 &&
        _finite(r.aspect) && r.aspect >= 1 && r.aspect <= 3 &&
        _finite(r.near_z) && _finite(r.far_z) && r.near_z >= .001 && r.near_z <= 1 && r.far_z > r.near_z && r.far_z <= 100000;
}
inline double screen_height(double aspect) { return ScreenWidth / aspect; }

struct Plan {
    bool valid = false;
    double separation = 0;          // metres between the two filming positions
    double offset[2]{};             // each eye along the camera's right axis
    pose075::Fov fov[2];            // off-axis frustum of each eye, radians
    double uv[4]{};                 // u0, v0, u1, v1 of the authored frame inside each eye image
    double center_fov_degrees = 0;  // frustum that contains both eyes, for the game's own culling
    double center_aspect = 0;
};

// eyeWidth/eyeHeight: pixels of one eye image. viewerIpd: metres between the viewer's eyes this frame.
inline Plan plan(const GxTheatre& r, uint32_t eyeWidth, uint32_t eyeHeight, double viewerIpd) {
    Plan p;
    if (!valid(r) || !eyeWidth || !eyeHeight) return p;
    const double ipd = _finite(viewerIpd) && viewerIpd >= .045 && viewerIpd <= .08 ? viewerIpd : DefaultIpd;
    const double tanV = tan(r.vertical_fov_degrees * (3.14159265358979323846 / 360)), tanH = tanV * r.aspect;
    const double pixels = double(eyeWidth) / double(eyeHeight);
    const double renderH = (std::max)(tanH, tanV * pixels), renderV = (std::max)(tanV, tanH / pixels);
    // The horizon lands one viewer IPD apart on the screen; never film wider apart than the viewer's eyes.
    p.separation = (std::min)(ipd, ipd * 2 * Convergence * tanH / ScreenWidth);
    const double shift = p.separation / (2 * Convergence);
    for (unsigned e = 0; e < 2; ++e) {
        const double side = e ? 1.0 : -1.0;
        p.offset[e] = side * p.separation / 2;
        p.fov[e] = {atan(-renderH - side * shift), atan(renderH - side * shift), atan(renderV), atan(-renderV)};
        for (double angle : {p.fov[e].left, p.fov[e].right, p.fov[e].up, p.fov[e].down})
            if (!_finite(angle) || fabs(angle) >= 1.55) return p;
    }
    p.uv[0] = (renderH - tanH) / (2 * renderH); p.uv[2] = 1 - p.uv[0];
    p.uv[1] = (renderV - tanV) / (2 * renderV); p.uv[3] = 1 - p.uv[1];
    // An eye is translated as well as sheared. Cover both eyes at the near and far
    // planes, including the padded render extent and a pixel for projection jitter.
    const double nearShift = fabs(p.separation / (2 * r.near_z) - shift);
    const double farShift = fabs(p.separation / (2 * r.far_z) - shift);
    const double boundH = renderH * (1 + 2.0 / eyeWidth) + (std::max)(nearShift, farShift);
    const double boundV = renderV * (1 + 2.0 / eyeHeight);
    p.center_fov_degrees = 2 * atan(boundV) * (180 / 3.14159265358979323846);
    p.center_aspect = boundH / boundV;
    p.valid = true;
    return p;
}

// Fills the two eye cameras and projections for the game. camera: the filming camera, row-major rigid
// view-inverse (right, up, back, position), exactly as the game stored it.
inline void compose(const GxTheatre& r, const Plan& p, const double* camera, GxViews& out) {
    pose075::demand(p.valid && camera && out.size == sizeof(out), "THEATRE_COMPOSE");
    eye036::Matrix base{}; std::copy_n(camera, 16, base.begin()); reference084::rigid(base);
    const auto clip = shared086::clip_projection({r.near_z, r.far_z});
    std::copy(base.begin(), base.end(), out.center);
    for (unsigned e = 0; e < 2; ++e) {
        auto local = eye036::identity(); local[12] = p.offset[e];
        const auto vi = eye036::mul(local, base);
        const auto projection = pose075::projection(p.fov[e], clip);
        std::copy(vi.begin(), vi.end(), out.camera[e]);
        std::copy(projection.begin(), projection.end(), out.projection[e]);
    }
    out.center_fov_degrees = p.center_fov_degrees; out.center_aspect = p.center_aspect;
    out.center_near = r.near_z; out.center_far = r.far_z;
}

// Where the screen stands in the tracking space: upright, facing the yaw the head had on entry.
// The screen is placed again in front of the head when:
//  - no theatre frame was armed for more than AnchorHoldSeconds of display time (a pause, the headset
//    taken off, a non-presentable XR state, a shot outside the lens range, a cut to another director);
//  - the player recenters (GxTheatre::placement changes, theatre_layer.inl);
//  - the reference space changes (pauseSpaceChange in Session::begin);
//  - the theatre has been unused for 600 XR frames, which also releases its private textures
//    (reset_theatre_frame), or the XR session closes (close_theatre).
class Anchor {
    GxPose pose_{}; bool valid_ = false; int64_t last_ = 0;
public:
    void reset() { valid_ = false; }
    bool valid() const { return valid_; }
    const GxPose& pose() const { return pose_; }
    // time: predicted display time in nanoseconds of the frame being armed.
    bool enter(const GxPose& head, int64_t time) {
        const int64_t hold = int64_t(AnchorHoldSeconds * 1e9);
        if (valid_ && time >= last_ && time - last_ <= hold) { last_ = time; return true; }
        const double yaw = atan2(2 * (head.qw * head.qy + head.qx * head.qz), 1 - 2 * (head.qy * head.qy + head.qx * head.qx));
        const double norm = head.qx * head.qx + head.qy * head.qy + head.qz * head.qz + head.qw * head.qw;
        if (!_finite(head.x) || !_finite(head.y) || !_finite(head.z) || !_finite(yaw) || !_finite(norm) || norm < .5 || norm > 1.5) {
            valid_ = false; return false;
        }
        pose_ = head; pose_.qx = pose_.qz = 0; pose_.qy = sin(yaw * .5); pose_.qw = cos(yaw * .5);
        pose_.x -= ScreenDistance * sin(yaw); pose_.z -= ScreenDistance * cos(yaw);
        valid_ = true; last_ = time; return true;
    }
};

// Clip-space corners of the screen for one eye: top-left, top-right, bottom-left, bottom-right.
struct Corners { float clip[4][4]; };
inline bool corners(const GxPose& anchor, double width, double height, const GxPose& eye, const GxFov& fov, Corners& out) {
    const double l = tan(fov.left), r = tan(fov.right), u = tan(fov.up), d = tan(fov.down);
    if (!_finite(l) || !_finite(r) || !_finite(u) || !_finite(d) || !(l < r) || !(d < u) || !(width > 0) || !(height > 0)) return false;
    eye036::Matrix toView;
    try {
        toView = eye036::mul(pose075::pose({anchor.x, anchor.y, anchor.z, anchor.qx, anchor.qy, anchor.qz, anchor.qw}),
                             eye036::inverse(pose075::pose({eye.x, eye.y, eye.z, eye.qx, eye.qy, eye.qz, eye.qw})));
    } catch (const std::exception&) { return false; }
    for (unsigned i = 0; i < 4; ++i) {
        const double x = (i & 1 ? .5 : -.5) * width, y = (i & 2 ? -.5 : .5) * height;
        const double vx = x * toView[0] + y * toView[4] + toView[12], vy = x * toView[1] + y * toView[5] + toView[13],
                     vz = x * toView[2] + y * toView[6] + toView[14];
        const double w = -vz;
        const double c[4] = {(2 * vx + (r + l) * vz) / (r - l), (2 * vy + (u + d) * vz) / (u - d), w * .5, w};
        for (unsigned k = 0; k < 4; ++k) { if (!_finite(c[k])) return false; out.clip[i][k] = float(c[k]); }
    }
    return true;
}
}
