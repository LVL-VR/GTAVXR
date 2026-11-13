#pragma once
// Native Windows x64 atomic POD publication. Writer serialization is provided
// by the EXISTING InputGuard at final UPDATE/reset; readers never acquire it.
// Every shared lane is an MSVC Interlocked operation, not a plain C++ seqlock.
#include "../../../core/core/held_local_pair.h"
#include <intrin.h>
namespace gx_held_local_pair
{
class Publication
{
    enum
    {
        BODY_CELLS = 11,
        PREFIX_CELLS = 6,
        CELLS = PREFIX_CELLS + 3 * BODY_CELLS,
        LOCAL_INSTANCE_CELL = PREFIX_CELLS
    };
    __declspec(align(8)) mutable volatile __int64 sequence_;
    __declspec(align(8)) mutable volatile __int64 cells_[CELLS];
    Publication(const Publication&);
    Publication& operator=(const Publication&);
    static uint64_t load(volatile __int64& v)
    {
        return (uint64_t)_InterlockedCompareExchange64(&v, 0, 0);
    }
    static void store(volatile __int64& v, uint64_t n)
    {
        _InterlockedExchange64(&v, (__int64)n);
    }
    static void packBody(const Body& b, uint64_t* v)
    {
        v[0] = b.instance;
        v[1] = b.entity;
        v[2] = b.archetype;
        v[3] = b.whole;
        v[4] = b.part;
        v[5] = b.level;
        v[6] = b.generation;
        v[7] = b.component;
        v[8] = b.lod;
        v[9] = b.nativeClass;
        v[10] = b.kind;
    }
    static Body unpackBody(const uint64_t* v)
    {
        Body b;
        b.instance = (uintptr_t)v[0];
        b.entity = (uintptr_t)v[1];
        b.archetype = (uintptr_t)v[2];
        b.whole = (uintptr_t)v[3];
        b.part = (uintptr_t)v[4];
        b.level = (unsigned)v[5];
        b.generation = (unsigned)v[6];
        b.component = (unsigned)v[7];
        b.lod = (unsigned)v[8];
        b.nativeClass = (unsigned)v[9];
        b.kind = (unsigned)v[10];
        return b;
    }

  public:
    Publication() : sequence_(0)
    {
        for (unsigned i = 0; i < CELLS; ++i)
            cells_[i] = 0;
    } // before publication to any thread
    void publish(const Snapshot& input)
    {
        const Snapshot s = input.valid() ? input : Snapshot();
        uint64_t values[CELLS] = {0};
        values[0] = s.tick;
        values[1] = s.frame;
        values[2] = s.updateGeneration;
        values[3] = s.inputSequence;
        values[4] = s.epoch;
        values[5] = s.heldMask;
        packBody(s.local, values + PREFIX_CELLS);
        packBody(s.held[0], values + PREFIX_CELLS + BODY_CELLS);
        packBody(s.held[1], values + PREFIX_CELLS + 2 * BODY_CELLS);
        // Exactly one externally serialized writer. A clear also advances the
        // stamp, including same epoch/frame/multiple native UPDATE publications.
        const uint64_t before = load(sequence_);
        store(sequence_, before + 1);
        for (unsigned i = 0; i < CELLS; ++i)
            store(cells_[i], values[i]);
        store(sequence_, before + 2);
    }
    void clear()
    {
        publish(Snapshot());
    }
    bool mayOwnLocal(uintptr_t instance) const
    {
        return instance && load(cells_[LOCAL_INSTANCE_CELL]) == instance;
    }
    bool read(Snapshot& out, uint64_t& stamp) const
    {
        out = Snapshot();
        stamp = 0;
        for (unsigned attempt = 0; attempt < 2; ++attempt)
        { // bounded, never waits/spins until writer completes
            const uint64_t before = load(sequence_);
            if (before & 1)
                continue;
            uint64_t values[CELLS];
            for (unsigned i = 0; i < CELLS; ++i)
                values[i] = load(cells_[i]);
            const uint64_t after = load(sequence_);
            if (before != after || (after & 1))
                continue;
            Snapshot s;
            s.tick = values[0];
            s.frame = values[1];
            s.updateGeneration = values[2];
            s.inputSequence = values[3];
            s.epoch = (unsigned)values[4];
            s.heldMask = (unsigned)values[5];
            s.local = unpackBody(values + PREFIX_CELLS);
            s.held[0] = unpackBody(values + PREFIX_CELLS + BODY_CELLS);
            s.held[1] = unpackBody(values + PREFIX_CELLS + 2 * BODY_CELLS);
            out = s;
            stamp = after;
            return true;
        }
        return false; // collision remains native on contention
    }
    bool current(uint64_t stamp) const
    {
        return stamp && !(stamp & 1) && load(sequence_) == stamp;
    }
};
}
