#pragma once
 
 
// Variety Plus) fill it inside CGame::Init: "Error allocating 2688 bytes in heap 0" in ptxManager::CreateEffectInstPool,
// Suite session 20261103T050457. -gtavxrHeapMB=N sets N MB; the Suite passes it only for profiles that serve content mods.
// Absent, malformed or outside [MIN_MB, MAX_MB] = the original 474 MB. sysMemSimpleAllocator takes an int byte size and
 
extern "C" __declspec(dllimport) char* __stdcall GetCommandLineA(void);

namespace gx_game_heap
{
const unsigned DEFAULT_MB = 474u;
const unsigned MIN_MB = 474u;
const unsigned MAX_MB = 2000u;

// N of a whole "-gtavxrHeapMB=N" argument (it starts the line or follows a space, tab or quote; the digits end at a
// space, tab, quote or the end of the line); DEFAULT_MB otherwise
inline unsigned ParseMb(const char* line)
{
    static const char key[] = "-gtavxrHeapMB=";
    if (!line)
        return DEFAULT_MB;
    for (const char* p = line; *p; ++p)
    {
        if (p != line && p[-1] != ' ' && p[-1] != '\t' && p[-1] != '"')
            continue;
        unsigned k = 0;
        while (key[k] && p[k] == key[k])
            ++k;
        if (key[k])
            continue;
        const char* d = p + k;
        unsigned mb = 0, n = 0;
        while (n < 5 && d[n] >= '0' && d[n] <= '9')
        {
            mb = mb * 10u + (unsigned)(d[n] - '0');
            ++n;
        }
        const char end = d[n];
        if (n == 0 || (end && end != ' ' && end != '\t' && end != '"'))
            return DEFAULT_MB;
        return (mb < MIN_MB || mb > MAX_MB) ? DEFAULT_MB : mb;
    }
    return DEFAULT_MB;
}

// SIMPLE_HEAP_SIZE on PC, in KB (InitGameHeap reads it once, before anything else allocates)
inline unsigned Kb()
{
    return ParseMb(GetCommandLineA()) * 1024u;
}
}
