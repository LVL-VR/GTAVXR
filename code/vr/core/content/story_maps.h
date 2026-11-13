#pragma once
// build166 item 6 (Mods > Online maps in Story, the native replacement for Simple Trainer's "enable mp maps" that World of
 
 
// Update thread only (script commands and AfterScripts141).
namespace gtavxr_mods
{
inline long& StoryMapsEpoch()
{
    static long epoch = 0;
    return epoch;
}
}
