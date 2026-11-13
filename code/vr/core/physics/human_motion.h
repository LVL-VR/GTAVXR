#pragma once
// Build157 tracking-error rejection, shared by hands, weapon points, body and throws.
// 60 m/s is nearly twice the cited 31 m/s collegiate bat-tip upper value and
// five times the cited 12 m/s hook. This is a glitch threshold, never a launch cap.
// Root/recenter, freshness, identity and finite-input gates remain independent.
namespace gx_motion
{
static const double GLITCH = 60.0;
}
