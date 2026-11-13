#pragma once
// A native screen-local composed batch: bounds of vertices/text ACTUALLY emitted.
// Never guesses event identity or captures the parent fullscreen on a failure.
#include <float.h>
#include <math.h>
namespace gx_script_crop
{
struct Bounds
{
    double left, top, right, bottom;
    bool any, failed;
    unsigned emitted;
    Bounds() : left(0), top(0), right(0), bottom(0), any(false), failed(false), emitted(0) {}
    void reject()
    {
        failed = true;
    }
    bool add(double l, double t, double r, double b)
    {
        if (failed)
            return false;
        if (!_finite(l) || !_finite(t) || !_finite(r) || !_finite(b) || r <= l || b <= t ||
            fabs(l) > 100000 || fabs(t) > 100000 || fabs(r) > 100000 || fabs(b) > 100000)
        {
            failed = true;
            return false;
        }
        if (++emitted > 512)
        {
            failed = true;
            return false;
        } // finite actual producer budget
        if (!any)
        {
            left = l;
            top = t;
            right = r;
            bottom = b;
            any = true;
        }
        else
        {
            if (l < left)
                left = l;
            if (t < top)
                top = t;
            if (r > right)
                right = r;
            if (b > bottom)
                bottom = b;
        }
        return true;
    }
};
}
