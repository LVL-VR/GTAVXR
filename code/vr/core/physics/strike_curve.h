#pragma once
// Build157 original math. Provenance and measured transforms: work157/CONTACT_REFERENCES.md.
// Same energy fraction for all targets; native world response consumes m*v / target mass.
#include "human_motion.h"
#include <float.h>
namespace gx_strike_curve
{
enum Class
{
    FIST,
    BAT,
    SMALL,
    KNIFE_SWING,
    KNIFE_STAB,
    HATCHET,
    PISTOL,
    RIFLE,
    CLASS_COUNT
};
inline const char* name(Class c)
{
    static const char* const names[] = {"fist",       "bat",     "small",  "knife_swing",
                                        "knife_stab", "hatchet", "pistol", "rifle"};
    return c >= FIST && c < CLASS_COUNT ? names[c] : "unknown";
}
inline bool finite(double v)
{
    return _finite(v) != 0;
}
// build165 item 17: Fisica > Avanzado > Velocidad de golpe scales every class gate (set by the physics snapshot; 1 = today)
inline double& gateScale()
{
    static double scale = 1.0;
    return scale;
}
inline double gate(Class c)
{
    return (c == FIST ? 2.5 : c == KNIFE_STAB ? 2.0 : 3.5) * gateScale();
}
inline double maximum(Class c)
{
    return c == FIST ? 2.5 : 1.25;
}
// Supplied by the integration host; excluded data/code stay outside the core.
double reference(Class c);
inline double fraction(double speed, Class c)
{
    if (c < FIST || c >= CLASS_COUNT || !finite(speed) || speed < 0 || speed > gx_motion::GLITCH)
        return 0;
    const double g = gate(c), ref = reference(c);
    if (speed <= g || ref <= g)
        return 0;
    const double f = (speed * speed - g * g) / (ref * ref - g * g), cap = maximum(c);
    return f < cap ? f : cap;
}
inline double damage(double tableDamage, double f)
{
    if (!finite(tableDamage) || tableDamage <= 0 || tableDamage > 200 || !finite(f) || f < 0)
        return 0;
    if (f > 2.5)
        f = 2.5;
    const double d = tableDamage * f;
    return d < 200 ? d : 200; // existing native API damage budget
}
 
 
 
inline double npcMultiplier(double speed, Class c)
{
    if (c < FIST || c >= CLASS_COUNT || !finite(speed) || speed <= gate(c) || speed > gx_motion::GLITCH)
        return 0;
    const double r = reference(c);
    double m = speed * speed / (r * r);
    if (m < 1)
        m = 1;
    if (c == FIST && m > 2.5)
        m = 2.5;
    return m;
}
inline double npcDamage(double tableDamage, double multiplier, bool power)
{
    if (!finite(tableDamage) || tableDamage <= 0 || !finite(multiplier) || multiplier <= 0)
        return 0;
    return tableDamage * multiplier * (power ? 1.5 : 1.0);
}
// The push follows the blow's momentum: 4..12 native units up to the reference speed (unchanged), then 12 x v/vref.
inline double npcImpulse(double speed, Class c)
{
    if (c < FIST || c >= CLASS_COUNT || !finite(speed) || speed <= gate(c) || speed > gx_motion::GLITCH)
        return 0;
    const double r = reference(c);
    if (speed >= r)
        return 12 * speed / r;
    const double f = fraction(speed, c);
    return 4 + 8 * (f < 1 ? f : 1);
}
// Inelastic normal momentum delivered to the target, kg*m/s. No damage/force tuning factor.
inline double impulse(double mass, double closing)
{
    return finite(mass) && mass > 0 && finite(closing) && closing > 0 && closing <= gx_motion::GLITCH
               ? mass * closing
               : 0;
}
inline double deltaV(double momentum, double targetMass)
{
    return finite(momentum) && momentum >= 0 && finite(targetMass) && targetMass > 0 ? momentum / targetMass
                                                                                     : 0;
}
}
