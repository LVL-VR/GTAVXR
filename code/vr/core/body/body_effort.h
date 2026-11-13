#pragma once
#include <math.h>
#include <stdint.h>
namespace gx_effort
{
inline bool finite(double x)
{
    return x == x && x > -1e30 && x < 1e30;
}
// Aggregate impulse per native simulation step, not per contact/iteration.
// One native character body weight of effort. Native mass/inertia still determine
// each recipient's linear/angular response; no object-size weight guesses.
inline int64_t allowance(double characterMass, double simulationDt)
{
    if (!finite(characterMass) || characterMass <= 0 || characterMass > 10000 || !finite(simulationDt) ||
        simulationDt <= 0 || simulationDt > .2)
        return 0;
    return (int64_t)(characterMass * 9.81 * simulationDt * 1000000.0);
}
inline int64_t scaledAllowance(double characterMass, double simulationDt, int percent)
{
    if (percent < 25 || percent > 500)
        return 0;
    // Scale the already finite aggregate budget, never recipient mass/inertia,
    // geometry, tracking or one budget PER manifold/contact/solver iteration.
    const int64_t base = allowance(characterMass, simulationDt);
    return base * (int64_t)percent / 100;
}
inline int64_t charge(double magnitude)
{
    if (!finite(magnitude) || magnitude <= 0)
        return 0;
    return magnitude > 1e9 ? INT64_MAX : (int64_t)ceil(magnitude * 1000000.0);
}
inline double ratio(int64_t available, int64_t requested)
{
    return available <= 0 || requested <= 0
               ? 0
               : (available >= requested ? 1 : (double)available / (double)requested);
}
}
