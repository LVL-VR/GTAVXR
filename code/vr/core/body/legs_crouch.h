#pragma once
 
// interfieren"; "si me gusta que las piernas tengan colisiones ... si paso por encima de un cadaver ... me gusta que el mundo
// reaccione"; option 3 accepted). The player's inferred legs keep every contact, except with:
//   - the object a hand holds or is about to grip (grip pressed with the hand at it);
//   - while crouched, a light loose object within reach of a hand.
// The veto acts on the contact pair (the existing body pair gate), so walking, stepping and vehicles are unchanged.
// No game headers, C++03.
namespace gx_legs_crouch
{
// kg: heavier objects keep their contact with the legs even crouched (a crate, a body)
inline double LIGHT_KG()
{
    return 25.0;
}
// m from the object's origin to the nearest hand
inline double REACH_M()
{
    return .6;
}
// m the head must drop below the standing reference to count as crouched
inline double CROUCH_DROP_M()
{
    return .30;
}
// build165 item 17: Fisica > Avanzado > Agacharse (set by the physics snapshot; .30 = CROUCH_DROP_M = today)
inline double& crouchDrop()
{
    static double drop = .30;
    return drop;
}
// m/s the standing reference sinks when the head stays low (it rises with the head at once)
inline double REFERENCE_SINK_MPS()
{
    return .005;
}
inline bool finite(double v)
{
    return v == v && v < 1e30 && v > -1e30;
}
// build161 G (worn 160: leaning forward to take a bin or a two-tier box, "mis pies se mueven ... las colisiones de nuevo vuelven
// a patear las cajas"): no crouch requirement. A hand this close to the object's bounding sphere is reaching for it (m)
inline double REACH_GAP_M()
{
    return .35;
}
inline bool vetoNear(bool enabled, bool loose, bool held, double mass, double handGap)
{
    if (!enabled || !loose)
        return false;
    if (held)
        return true;
    return finite(mass) && mass > 0 && mass <= LIGHT_KG() && finite(handGap) && handGap <= REACH_GAP_M();
}
inline bool veto(bool enabled, bool loose, bool held, bool crouched, double mass, double nearestHand)
{
    if (!enabled || !loose)
        return false;
    if (held)
        return true;
    return crouched && finite(mass) && mass > 0 && mass <= LIGHT_KG() && finite(nearestHand) &&
           nearestHand <= REACH_M();
}
class Crouch
{
  public:
    Crouch() : reference_(0), valid_(false) {}
    // headHeight: tracked head height above the play-space floor (m)
    bool update(bool tracked, double headHeight, double dt)
    {
        if (!tracked || !finite(headHeight) || !finite(dt) || dt < 0 || dt > 1)
        {
            return valid_ && headHeight < reference_ - crouchDrop();
        }
        if (!valid_ || headHeight > reference_)
        {
            reference_ = headHeight;
            valid_ = true;
        }
        else
            reference_ -= REFERENCE_SINK_MPS() * dt;
        if (reference_ < headHeight)
            reference_ = headHeight;
        return headHeight < reference_ - crouchDrop();
    }
    double reference() const
    {
        return reference_;
    }

  private:
    double reference_;
    bool valid_;
};
}
