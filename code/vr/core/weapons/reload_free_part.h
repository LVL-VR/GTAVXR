#pragma once
 
// al soltar el grip y la A, ya este tambor ya no se mueva, se queda en la posicion donde estaba"). Shared by the swing-out
// revolvers and the break actions (flare gun, Marksman, Gadget, compact launchers). Replaces the 158 wrist-flick thresholds.
// Reference (H3VR wiki, Classic Controls / revolver pages): a button releases the cylinder; a sharp wrist snap or a push closes.
// While released the part is a 1-DOF body on its hinge, in the gun frame:
//   - inertia: it keeps its world rotation when the gun turns about the hinge (rate -= change of the gun's rate);
//   - pseudo-gravity: gravity minus the gun's own acceleration, along the part's opening tangent, over its lever (a snap that
//     stops sharply flings it, gravity swings it open or shut);
//   - friction, and the hinge stops (closed / fully open).
// Not released: it does not move at all. No game headers, C++03.
namespace gx_free
{
inline bool finite(double v)
{
    return v == v && v < 1e30 && v > -1e30;
}
class Hinge
{
  public:
    Hinge()
    {
        reset(0);
    }
    void reset(double angle)
    {
        angle_ = angle;
        rate_ = 0;
        prevGun_ = 0;
        primed_ = false;
    }
    // released: grip + A (X on the left hand) held on the gun hand. gunRate: the gun's angular rate about the hinge axis, rad/s,
    // + = the part's opening sense. accel: rad/s^2 that pseudo-gravity applies about the hinge, + = opening. max: open stop, rad.
    void step(bool released, double gunRate, double accel, double max, double dt)
    {
        if (!released || !finite(gunRate) || !finite(accel) || !finite(max) || max <= 0 || !(dt > 0) ||
            dt > .1)
        {
            rate_ = 0;
            primed_ = false;
            if (angle_ > max && max > 0)
                angle_ = max;
            return;
        }
        if (!primed_)
        {
            prevGun_ = gunRate;
            primed_ = true;
        }
        rate_ -= COUPLING() * (gunRate - prevGun_);
        prevGun_ = gunRate;
        if (accel > ACCEL_MAX())
            accel = ACCEL_MAX();
        else if (accel < -ACCEL_MAX())
            accel = -ACCEL_MAX();
        rate_ += accel * dt;
        rate_ /= 1.0 + FRICTION() * dt;
        angle_ += rate_ * dt;
        if (angle_ <= 0)
        {
            angle_ = 0;
            if (rate_ < 0)
                rate_ = 0;
        }
        if (angle_ >= max)
        {
            angle_ = max;
            if (rate_ > 0)
                rate_ = 0;
        }
    }
    double angle() const
    {
        return angle_;
    }
    double rate() const
    {
        return rate_;
    }
    // a crane / hinge pin turns freely once released (the part keeps its world rotation through a snap)
    static double COUPLING()
    {
        return 1.0;
    }
    // 1/s viscous friction of the hinge
    static double FRICTION()
    {
        return 4.0;
    }
    // rad/s^2 bound of the pseudo-gravity term (tracking spikes)
    static double ACCEL_MAX()
    {
        return 600.0;
    }

  private:
    double angle_, rate_, prevGun_;
    bool primed_;
};
// rad/s^2 about the hinge from the pseudo-gravity component (m/s^2) along the opening tangent and the part's lever (m)
inline double accel(double alongOpen, double lever)
{
    return finite(alongOpen) && finite(lever) && lever > .005 ? alongOpen / lever : 0;
}
// the latch: released only while the gun hand holds its grip and the latch button together
inline bool released(double gunGrip, bool button)
{
    return button && finite(gunGrip) && gunGrip >= .45;
}
}
