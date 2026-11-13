#pragma once
// Main-thread native camera classification. Render commands cache the result;
// no actor clone, visibility/task mutation or render-thread ped dereference.
namespace gx_body_view
{
enum Mode
{
    NATIVE = 0,
    HIDDEN_LOCAL = 1,
    CINEMATIC_ACTOR = 2
};
inline Mode mode(bool ready, bool local, bool scripted, bool firstPerson, bool hideHead)
{
    if (!ready || !local || !scripted)
        return NATIVE;
    return firstPerson || hideHead ? HIDDEN_LOCAL : CINEMATIC_ACTOR;
}
inline bool hide(Mode m, bool xr, bool scene)
{
    return m == HIDDEN_LOCAL && xr && scene;
}
inline bool actor(Mode m, bool xr, bool scene)
{
    return m == CINEMATIC_ACTOR && xr && scene;
}
}
