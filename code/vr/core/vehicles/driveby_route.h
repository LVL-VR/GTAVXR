#pragma once
 
 
 
//   reads IsVehicleAttackInputDown while TASK_AIM_GUN_VEHICLE_DRIVE_BY runs)
 
 
// Aircraft pilots: their triggers belong to the flight controls (no pilot drive-by until the user decides).
namespace gx_driveby
{
enum Route
{
    NONE = 0,
    VEHICLE = 1,
    FOOT = 2
};
struct Seat
{
    bool seated, aircraftDriver, thirdPerson, layoutHasDriver, train, turretSeat;
};
inline Route route(const Seat& s)
{
    if (!s.seated || s.aircraftDriver || s.thirdPerson)
        return NONE;
    if (s.turretSeat)
        return FOOT;
    if (!s.layoutHasDriver && !s.train)
        return FOOT;
    return VEHICLE;
}
 
// handOnControl comes first: the aim hand is on the wheel / handlebar / flight control / door, so we never asked.
enum Reason
{
    OK = 0,
    HAND_ON_CONTROL,
    DISABLED,
    MOUNTED,
    NO_AMMO,
    NO_CLIPS,
    DRIVER_FLAG,
    PHONE,
    TILT,
    REASON_COUNT
};
struct Facts
{
    bool handOnControl, canDoDriveBys, mountedSeat, outOfAmmo, clips, isDriver, driverNoDriveBy, phone,
        firstPerson, tilted, forcedAim;
};
// Integration host supplies the native rejection order.
Reason reason(const Facts& f);
inline const char* name(Reason r)
{
    static const char* n[REASON_COUNT] = {"ok",      "hand_on_control", "disabled",    "mounted_weapon",
                                          "no_ammo", "no_clips",        "driver_flag", "phone",
                                          "tilt"};
    return r >= OK && r < REASON_COUNT ? n[r] : "unknown";
}
inline const char* name(Route r)
{
    return r == VEHICLE ? "vehicle" : (r == FOOT ? "foot" : "none");
}
// One attempt = the aim-hand Grip crosses .65 while seated with a gun or a seat weapon; judged once, 500 ms later.
class Attempts
{
    float grip_;
    bool open_;
    unsigned long long at_;

  public:
    Attempts() : grip_(0), open_(false), at_(0) {}
    void reset()
    {
        grip_ = 0;
        open_ = false;
        at_ = 0;
    }
    // returns true once per attempt, when it is due
    bool update(bool armedSeat, float grip, unsigned long long nowMs)
    {
        if (!(grip >= 0 && grip <= 1))
            grip = 0;
        const bool press = grip >= .65f && grip_ < .65f;
        grip_ = grip;
        if (!armedSeat)
        {
            open_ = false;
            return false;
        }
        if (press)
        {
            open_ = true;
            at_ = nowMs;
        }
        if (!open_ || nowMs - at_ < 500)
            return false;
        open_ = false;
        return true;
    }
};
}
