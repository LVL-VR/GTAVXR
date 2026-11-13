#pragma once
 
// SEA COMO ESTA TODO AHORA EN LA ULTIMA VERSION"): the body view of each situation, FULL BODY / HANDS ONLY / MOTION CONTROLS
 
// show hands only, everything else the full body); quads are not bikes in that code, so their default is FULL BODY.
 
namespace gx_body_situation
{
enum Mode
{
    FULL_BODY = 0,
    HANDS_ONLY = 1,
    MOTION_OFF = 2,
    MODE_COUNT = 3
};
enum Situation
{
    FOOT,
    CROUCH_COVER,
    SWIM,
    CLIMB,
    PARACHUTE,
    PHONE,
    CAR_DRIVER,
    CAR_PASSENGER,
    BIKE,
    QUAD,
    PLANE,
    HELICOPTER,
    BOAT,
    SUBMARINE,
    TURRET,
    ANIMAL,
    COUNT
};
struct State
{
    bool animal, inVehicle, driver, bike, quad, plane, aircraft, boat, submarine, turretSeat, phone,
        parachute, swim, climb, crouchCover;
    State()
    {
        animal = inVehicle = driver = bike = quad = plane = aircraft = boat = submarine = turretSeat = phone =
            parachute = swim = climb = crouchCover = false;
    }
};
// First match wins. Vehicle rows come before the phone so a phone on a bike keeps the bike row (as in 166); aircraft
// gunners stay on the aircraft rows (166 showed hands only there), the turret row is every other weapon / turret seat.
inline unsigned situation(const State& s)
{
    if (s.animal)
        return ANIMAL;
    if (s.inVehicle)
    {
        if (s.bike)
            return BIKE;
        if (s.plane)
            return PLANE;
        if (s.aircraft)
            return HELICOPTER; // helicopters, blimps and other aircraft
        if (s.turretSeat && !s.driver)
            return TURRET;
        if (s.quad)
            return QUAD;
        if (s.submarine)
            return SUBMARINE;
        if (s.boat)
            return BOAT;
        return s.driver ? CAR_DRIVER : CAR_PASSENGER;
    }
    if (s.phone)
        return PHONE;
    if (s.parachute)
        return PARACHUTE;
    if (s.swim)
        return SWIM;
    if (s.climb)
        return CLIMB;
    if (s.crouchCover)
        return CROUCH_COVER;
    return FOOT;
}
inline int defaultMode(unsigned s)
{
    return s == BIKE || s == PLANE || s == HELICOPTER ? HANDS_ONLY : FULL_BODY;
}
// an animal has no human hands to show alone
inline bool allowed(unsigned s, int m)
{
    return s < COUNT && m >= 0 && m < MODE_COUNT && !(s == ANIMAL && m == HANDS_ONLY);
}
inline int sanitize(unsigned s, int m)
{
    return allowed(s, m) ? m : defaultMode(s);
}
inline int step(unsigned s, int m, int d)
{
    int v = sanitize(s, m);
    for (unsigned n = 0; n < MODE_COUNT; ++n)
    {
        v = (v + (d > 0 ? 1 : MODE_COUNT - 1)) % MODE_COUNT;
        if (allowed(s, v))
            return v;
    }
    return defaultMode(s);
}
inline const char* key(unsigned s)
{
    static const char* const k[COUNT] = {
        "foot", "crouch", "swim",  "climb",      "parachute", "phone",     "car_driver", "car_passenger",
        "bike", "quad",   "plane", "helicopter", "boat",      "submarine", "turret",     "animal"};
    return s < COUNT ? k[s] : "";
}
// UTF-8
inline const char* name(unsigned s, bool es)
{
    static const char* const en[COUNT] = {"On foot",
                                          "Crouched / cover",
                                          "Swimming / diving",
                                          "Climbing / ladders",
                                          "Parachute / skydive",
                                          "Phone (on foot)",
                                          "Car, driver",
                                          "Car, passenger",
                                          "Motorcycle / bicycle",
                                          "Quad",
                                          "Plane",
                                          "Helicopter",
                                          "Boat / jet ski",
                                          "Submarine",
                                          "Turret / gunner seat",
                                          "Playing as an animal"};
    static const char* const sp[COUNT] = {"A pie",
                                          "Agachado / cobertura",
                                          "Nadando / buceando",
                                          "Escalando / escaleras",
                                          "Paraca\303\255das / ca\303\255da libre",
                                          "Tel\303\251fono (a pie)",
                                          "Auto, conductor",
                                          "Auto, pasajero",
                                          "Moto / bicicleta",
                                          "Cuatrimoto",
                                          "Avi\303\263n",
                                          "Helic\303\263ptero",
                                          "Barco / moto acu\303\241tica",
                                          "Submarino",
                                          "Torreta / asiento de artillero",
                                          "Jugando como animal"};
    return s < COUNT ? (es ? sp[s] : en[s]) : "";
}
inline const char* modeName(int m, bool es)
{
    static const char* const en[MODE_COUNT] = {"Full body", "Hands only", "Motion controls off"};
    static const char* const sp[MODE_COUNT] = {"Cuerpo completo", "Solo manos",
                                               "Sin controles de movimiento"};
    return m >= 0 && m < MODE_COUNT ? (es ? sp[m] : en[m]) : "";
}
} // namespace gx_body_situation
