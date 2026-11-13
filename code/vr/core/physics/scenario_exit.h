#pragma once
// Build161 D. Facts are sampled on UPDATE, outside InputGuard.
namespace gx_scenario
{
enum Decision
{
    EXIT_SCENARIO,
    REFUSE_IDENTITY,
    NOT_LIVING_HUMAN,
    VEHICLE,
    SEATED,
    NOT_SCENARIO,
    NATIVE_PROOF,
    ALREADY_RAGDOLL,
    PHYSICAL_ROUTE
};
struct Facts
{
    bool identity, livingHuman, vehicle, seated, scenario, proof, ragdoll;
    Facts()
        : identity(false), livingHuman(false), vehicle(false), seated(false), scenario(false), proof(false),
          ragdoll(false)
    {
    }
};
inline Decision decide(const Facts& f, bool gtaImpact = true)
{
    if (!gtaImpact)
        return PHYSICAL_ROUTE;
    if (!f.identity)
        return REFUSE_IDENTITY;
    if (!f.livingHuman)
        return NOT_LIVING_HUMAN;
    if (f.vehicle)
        return VEHICLE;
    if (f.seated)
        return SEATED;
    if (!f.scenario)
        return NOT_SCENARIO;
    if (f.proof)
        return NATIVE_PROOF;
    if (f.ragdoll)
        return ALREADY_RAGDOLL;
    return EXIT_SCENARIO;
}
inline const char* name(Decision d)
{
    static const char* const names[] = {"exit",         "identity",       "not_living_human",
                                        "vehicle",      "seated",         "not_scenario",
                                        "native_proof", "native_ragdoll", "physical_response"};
    return d >= EXIT_SCENARIO && d <= PHYSICAL_ROUTE ? names[d] : "unknown";
}
}
