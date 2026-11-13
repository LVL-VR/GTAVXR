#pragma once
 
// world?" / "ME GUSTA INCLUYELO AL PLAN"): the force a real person can apply, from sourced ergonomics data, replacing our own
// 120 N hand, 400 N shared two-hand and 120 N body numbers and the refusal of objects over 20 kg / 200 kg. A heavy object
 
//   Das et al., push/pull strength, adult males standing: maximum isometric push 251 N, pull 400 N (seated push 227 N);
//     https://yadda.icm.edu.pl/baztech/element/bwmeta1.element.baztech-1d80f4a1-1fe4-4ae0-9748-f598072c9de4
//   Same study, two-handed horizontal isometric standing: push 253.8 N, pull 234.2 N (means), .../c/Das1.pdf
 
//   CCOHS recommended (not maximum) limits: 225 N whole body standing, 110 N arms fully extended;
//     https://ccohs.gc.ca/oshanswers/ergonomics/push1.html
// No one-handed maximum was found. ASSUMPTION (labelled): one hand's ceiling = the standing push maximum (251 N, the lower of
// push/pull, direction is not resolved here); two hands each keep their own ceiling and their forces add in the solver.
// Isometric maxima are a conservative floor for brief dynamic efforts.
namespace gx_human
{
// one hand (Das standing push maximum; see the assumption above)
static const double HAND_N = 251.0;
// the whole body leaning into an object (Das standing push maximum; two-handed 253.8 N)
static const double BODY_N = 251.0;
// ASSUMPTION (not from the sources above): a single hand couples torque across its own breadth, ~8.5 cm for an adult hand.
static const double PALM_LEVER_M = .085;
// The torque a hand can apply about the object's centre: its force times the real lever (never below its own palm couple).
inline double handTorque(double force, double lever)
{
    if (!(force > 0) || !(lever >= 0))
        return 0;
    return force * (lever > PALM_LEVER_M ? lever : PALM_LEVER_M);
}
}
