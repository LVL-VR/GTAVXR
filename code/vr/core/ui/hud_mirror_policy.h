#pragma once
#include <string.h>
#include <stdint.h>
namespace gx_hud_mirror
{
enum
{
    MaxTargets = 16,
    MaxDepth = 64,
    MaxBytes = 512 * 1024 * 1024
};
enum Traits
{
    Filtered = 1,
    ThreeDimensional = 2,
    DisplayCallback = 4,
    NonNormalBlend = 8
};
struct Identity
{
    const void* clip;
    const void* parent;
    const char* url;
    unsigned id;
    Identity() : clip(0), parent(0), url(0), id(0) {}
};
// Exact prepared handle identity AND exact resource URL. A basename/pattern is
// useful for selecting a plan, never sufficient for matching a Draw producer.
inline bool same(const Identity& a, const void* clip, const void* parent, const char* url)
{
    return a.clip && a.clip == clip && a.parent == parent && a.url && url && !strcmp(a.url, url);
}
class Journal
{
    Identity targets_[MaxTargets];
    const void* stack_[MaxDepth];
    unsigned stackTraits_[MaxDepth], count_, depth_, seen_[MaxTargets];
    int active_;
    unsigned passes_;
    bool prepared_, bad_;

  public:
    Journal()
    {
        reset();
    }
    void reset()
    {
        count_ = depth_ = passes_ = 0;
        active_ = -1;
        prepared_ = bad_ = false;
        memset(seen_, 0, sizeof(seen_));
    }
    bool prepare(const Identity* targets, unsigned count)
    {
        reset();
        if (!targets || !count || count > MaxTargets)
            return false;
        for (unsigned i = 0; i < count; ++i)
        {
            if (!targets[i].clip || !targets[i].parent || !targets[i].url || !targets[i].url[0] ||
                !targets[i].id)
                return false;
            for (unsigned j = 0; j < i; ++j)
                if (targets[j].clip == targets[i].clip || targets[j].id == targets[i].id)
                    return false;
        }
        memcpy(targets_, targets, count * sizeof(*targets));
        count_ = count;
        prepared_ = true;
        return true;
    }
    int enter(const void* clip, const void* parent, const char* url, unsigned traits)
    {
        if (!prepared_ || bad_)
            return -1;
        if (!clip || depth_ == MaxDepth)
        {
            bad_ = true;
            return -1;
        }
        // Pre-Display mask characters may live outside this hierarchy, so don't
        // mistake their parent for the call-stack parent. Actual entry identity
        // and balanced pointer exit are checked independently.
        stack_[depth_] = clip;
        stackTraits_[depth_++] = traits;
        int match = -1;
        for (unsigned i = 0; i < count_; ++i)
            if (targets_[i].clip == clip)
            {
                if (!same(targets_[i], clip, parent, url))
                {
                    bad_ = true;
                    return -1;
                }
                match = (int)i;
                break;
            }
        if (match >= 0)
        {
            if (active_ >= 0 || passes_ || seen_[match]++)
            {
                bad_ = true;
                return -1;
            }
            active_ = match;
            for (unsigned d = 0; d < depth_; ++d)
                if (stackTraits_[d])
                {
                    bad_ = true;
                    return -1;
                }
        }
        if (traits && (active_ >= 0 || match >= 0))
        {
            bad_ = true;
            return -1;
        }
        return match;
    }
    void leave(const void* clip)
    {
        if (!prepared_ || bad_)
            return;
        if (!depth_ || stack_[depth_ - 1] != clip)
        {
            bad_ = true;
            return;
        }
        --depth_;
        if (active_ >= 0 && targets_[active_].clip == clip)
        {
            if (passes_)
            {
                bad_ = true;
                return;
            }
            active_ = -1;
        }
    }
    void pass(bool begin, bool safe)
    {
        if (!prepared_ || bad_)
            return;
        if (begin)
        {
            if (passes_ || !safe)
            {
                if (active_ >= 0)
                    bad_ = true;
            }
            ++passes_;
        }
        else if (!passes_)
            bad_ = true;
        else
            --passes_;
    }
    int drawable() const
    {
        return prepared_ && !bad_ && active_ >= 0 && passes_ == 1 ? active_ : -1;
    }
    int active() const
    {
        return prepared_ && !bad_ ? active_ : -1;
    }
    void reject()
    {
        bad_ = true;
    }
    bool good() const
    {
        return prepared_ && !bad_ && !depth_ && !passes_ && active_ < 0;
    }
    bool seen(unsigned i) const
    {
        return i < count_ && seen_[i] == 1;
    }
    bool complete() const
    {
        if (!good())
            return false;
        for (unsigned i = 0; i < count_; ++i)
            if (!seen(i))
                return false;
        return true;
    }
};
 
// native occlusion/predicate queries even when this HUD experiment is OFF.
// Overflow/unbalanced pairs fail closed; never try to repair original queries.
struct QueryLedger
{
    const void* queries[16];
    unsigned count;
    bool failed;
    bool safe() const
    {
        return !failed && !count;
    }
    void begin(const void* q)
    {
        if (!q || count == 16)
        {
            failed = true;
            return;
        }
        for (unsigned i = 0; i < count; ++i)
            if (queries[i] == q)
            {
                failed = true;
                return;
            }
        queries[count++] = q;
    }
    void end(const void* q)
    {
        for (unsigned i = 0; i < count; ++i)
            if (queries[i] == q)
            {
                queries[i] = queries[--count];
                queries[count] = 0;
                return;
            }
        failed = true;
    }
};
inline bool budget(unsigned width, unsigned height, unsigned count)
{
    return width && height && width <= 3840 && height <= 2160 && count && count <= MaxTargets &&
           uint64_t(width) * height * 12 * count <= MaxBytes; // color4 + worst D32FS8 depth8
}
}
