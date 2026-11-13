#pragma once
 
// saved height "Se aplica al empezar"): Calibracion > Altura > Medir altura (3 s countdown) and the eye height above the
// runtime floor (OpenXR STAGE, bridge v14 GtavxrReadFloor1). Recenter and the +-2 cm trim are untouched. No game headers, C++03.
namespace gx_height
{
inline double COUNTDOWN_S()
{
    return 3.0;
}
// cm: plausible standing eye height above the floor (a typed value is clamped, a measured one outside is not saved)
inline int MIN_CM()
{
    return 90;
}
inline int MAX_CM()
{
    return 230;
}
inline bool finite(double v)
{
    return v == v && v < 1e30 && v > -1e30;
}
inline bool plausible(int cm)
{
    return cm >= MIN_CM() && cm <= MAX_CM();
}
inline int clampCm(int cm)
{
    return cm < MIN_CM() ? MIN_CM() : (cm > MAX_CM() ? MAX_CM() : cm);
}
// eye height above the floor (cm) from a LOCAL head height and the LOCAL origin height above the floor (m); 0 = unknown
inline int eyeCm(double headY, double localAboveFloor)
{
    const double cm = (headY + localAboveFloor) * 100;
    if (!finite(cm) || cm < -1e6 || cm > 1e6)
        return 0;
    return (int)(cm < 0 ? cm - .5 : cm + .5);
}
// LOCAL standing head height for a saved / typed eye height
inline double standingY(int cm, double localAboveFloor)
{
    return cm / 100.0 - localAboveFloor;
}
class Countdown
{
    double start_;
    bool active_;

  public:
    Countdown() : start_(0), active_(false) {}
    void begin(double now)
    {
        start_ = now;
        active_ = finite(now);
    }
    bool active() const
    {
        return active_;
    }
    // 3, 2, 1 while counting; 0 when idle
    int secondsLeft(double now) const
    {
        if (!active_)
            return 0;
        const double left = COUNTDOWN_S() - (now - start_);
        if (!(left > 0))
            return 0;
        const int whole = (int)left;
        return left > whole ? whole + 1 : whole;
    }
    // true once, when the countdown ends
    bool due(double now)
    {
        if (!active_ || !(now - start_ >= COUNTDOWN_S()))
            return false;
        active_ = false;
        return true;
    }
};
}
