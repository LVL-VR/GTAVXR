#ifndef GTAVXR_SCRIPT_HOST_OBJECTS_H
#define GTAVXR_SCRIPT_HOST_OBJECTS_H
// GTAVXR script host v9 (Suite session 20261104T180023): retail ScriptHookVDotNet NOPs, when it starts, the two checks of
// CREATE_OBJECT after the model lookup (NativeMemory.cs "Nopping this enables to spawn some drawable objects without a
// dedicated collision"): model info null and MODEL_IS_TYPE_OBJECT. Script mods rely on it: Open World Interiors creates the
// hospital decor with v_ilev_ph_doorframe, CREATE_OBJECT returned 0 here and the mod waited forever on entity 0 faded out.
// Our fork writes no game code (fork change 8), so this EXE accepts such a model itself, only while the script host runs one
// native for a hosted mod (State::hosted), only a loaded drawable model (MI_TYPE_BASE / MI_TYPE_TIME), and keeps the null
// check. Every other caller (the game's own scripts included) keeps the original check unchanged.
 
namespace gx_script_host_objects
{
struct State
{
    bool hosted;
    unsigned accepted;
    unsigned lastHash;
};
// one instance for every translation unit (function-local static of an inline function)
inline State& Get()
{
    static State state = {false, 0u, 0u};
    return state;
}
// true = create this model although it is not type object (a script host call); false = the original check decides
inline bool AcceptNonObjectModel(bool isTypeObject, bool isDrawableType, unsigned modelHash)
{
    State& state = Get();
    if (isTypeObject || !isDrawableType || !state.hosted)
    {
        return false;
    }
    ++state.accepted;
    state.lastHash = modelHash;
    return true;
}
}
#endif
