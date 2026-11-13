#pragma once
namespace gx_pair
{
enum Completion
{
    COMPLETE,
    NATIVE_DISCARD,
    FAULT
};
// Only exact native discard provenance can explain a wholly unexecuted pair.
// Never turn a partially executed/stale pair into a benign empty frame.
inline Completion classify(bool left, bool right, int eye, long mask, bool ended, long update, long pair,
                           long discarded, bool healthy)
{
    if (!healthy || update != pair)
        return FAULT;
    if (left && right && eye == -1 && mask == 3 && ended)
        return COMPLETE;
    if (pair > 0 && discarded == pair && !left && !right && eye == -1 && mask == 0 && !ended)
        return NATIVE_DISCARD;
    return FAULT;
}
}
