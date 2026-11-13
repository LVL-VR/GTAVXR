#pragma once
// Side = hand WIELDING the weapon. SUPPORT uses the opposite physical tracker.
// No pointers, controller writes, ammo or body policy. Read-only resolve performs NO IO.
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
namespace gx_weapon_calibration
{
enum Anchor
{
    WEAPON = 0,
    SUPPORT = 1
};
enum Loaded
{
    ABSENT = 0,
    PRESENT = 1,
    INVALID = 2
};
struct Key
{
    unsigned hand, hash;
    Anchor anchor;
    bool family;
    Key(unsigned h = 1, Anchor a = WEAPON, unsigned v = 0, bool f = false)
        : hand(h), hash(v), anchor(a), family(f)
    {
    }
    bool valid() const
    {
        return hand < 2 && hash && (anchor == WEAPON || anchor == SUPPORT);
    }
    bool same(const Key& k) const
    {
        return hand == k.hand && hash == k.hash && anchor == k.anchor && family == k.family;
    }
};
struct Pose
{
    int value[6];
    Pose()
    {
        for (unsigned p = 0; p < 6; ++p)
            value[p] = 0;
    }
    bool valid() const
    {
        for (unsigned p = 0; p < 6; ++p)
            if (value[p] < -(p < 3 ? 150 : 180) || value[p] > (p < 3 ? 150 : 180))
                return false;
        return true;
    }
};
struct Record
{
    Pose value, legacy;
    unsigned mask, follow, legacyMask;
    Loaded loaded;
    bool saved;
    Record() : mask(0), follow(0), legacyMask(0), loaded(ABSENT), saved(true) {}
    bool valid(const Key& key) const
    {
        return key.valid() && value.valid() && legacy.valid() && mask < 64 && follow < 64 &&
               !(mask & follow) && (key.hand == 0 || !follow) && legacyMask < 8 &&
               (!legacyMask || (key.hand == 1 && key.anchor == SUPPORT));
    }
    bool sameStored(const Record& r) const
    {
        if (mask != r.mask || follow != r.follow)
            return false;
        for (unsigned p = 0; p < 6; ++p)
            if (value.value[p] != r.value.value[p])
                return false;
        return true;
    }
};
inline int mirrorSign(unsigned parameter)
{
    return parameter == 0 || parameter == 4 || parameter == 5 ? -1 : 1;
}
inline bool decode(const char* text, const Key& key, Record& out)
{
    if (!text || strlen(text) > 160 || !key.valid())
        return false;
    long v[9];
    const char* at = text;
    for (unsigned p = 0; p < 9; ++p)
    {
        while (*at == ' ' || *at == '\t')
            ++at;
        if (!*at)
            return false;
        errno = 0;
        char* end = NULL;
        v[p] = strtol(at, &end, 10);
        if (errno || end == at || v[p] < -1000 || v[p] > 1000 || (*end && *end != ' ' && *end != '\t'))
            return false;
        at = end;
    }
    while (*at == ' ' || *at == '\t')
        ++at;
    if (*at || v[0] != 2 || v[1] < 0 || v[2] < 0)
        return false;
    Record r;
    r.mask = (unsigned)v[1];
    r.follow = (unsigned)v[2];
    r.loaded = PRESENT;
    for (unsigned p = 0; p < 6; ++p)
        r.value.value[p] = (int)v[3 + p];
    if (!r.valid(key))
        return false;
    out = r;
    return true;
}
inline bool encode(const Key& key, const Record& r, char* out, size_t bytes)
{
    if (!out || bytes < 96 || !r.valid(key))
        return false;
    return sprintf_s(out, bytes, "2 %u %u %d %d %d %d %d %d", r.mask, r.follow, r.value.value[0],
                     r.value.value[1], r.value.value[2], r.value.value[3], r.value.value[4],
                     r.value.value[5]) > 0;
}
class Backend
{
  public:
    virtual ~Backend() {}
    virtual Record load(const Key&) = 0;
    virtual bool write(const Key&, const Record*) = 0;
};
struct Value
{
    Pose pose;
    unsigned follow;
    bool saved;
    Value() : follow(0), saved(true) {}
};
class Cache
{
    Cache(const Cache&);
    Cache& operator=(const Cache&);
    struct Entry
    {
        Key key;
        Record record;
        Entry(const Key& k, const Record& r) : key(k), record(r) {}
    };
    Backend& backend_;
    std::vector<Entry> entries_;
    unsigned revision_;
    const Record* find(const Key& key) const
    {
        for (size_t n = 0; n < entries_.size(); ++n)
            if (entries_[n].key.same(key))
                return &entries_[n].record;
        return NULL;
    }
    size_t acquire(const Key& key)
    {
        for (size_t n = 0; n < entries_.size(); ++n)
            if (entries_[n].key.same(key))
                return n;
        Record r = backend_.load(key);
        if (!r.valid(key))
        {
            r = Record();
            r.loaded = INVALID;
            r.saved = false;
        }
        entries_.push_back(Entry(key, r));
        ++revision_;
        return entries_.size() - 1;
    }
    static void rightLayer(const Record& r, Value& v)
    {
        for (unsigned p = 0; p < 6; ++p)
        {
            const unsigned bit = 1u << p;
            if (r.legacyMask & bit)
                v.pose.value[p] = r.legacy.value[p];
            if (r.mask & bit)
                v.pose.value[p] = r.value.value[p];
        }
        v.saved = v.saved && r.saved;
    }
    static void leftLayer(const Record& r, const Pose& right, Value& v)
    {
        for (unsigned p = 0; p < 6; ++p)
        {
            const unsigned bit = 1u << p;
            if (r.mask & bit)
            {
                v.pose.value[p] = r.value.value[p];
                v.follow &= ~bit;
            }
            if (r.follow & bit)
            {
                v.pose.value[p] = right.value[p] * mirrorSign(p);
                v.follow |= bit;
            }
        }
        v.saved = v.saved && r.saved;
    }

  public:
    explicit Cache(Backend& backend) : backend_(backend), revision_(1) {}
    void warm(unsigned model, unsigned family)
    {
        if (!model)
            return;
        for (unsigned a = 0; a < 2; ++a)
            for (unsigned h = 0; h < 2; ++h)
            {
                if (family)
                    acquire(Key(h, (Anchor)a, family, true));
                acquire(Key(h, (Anchor)a, model, false));
            }
    }
    bool resolve(unsigned hand, Anchor anchor, unsigned model, unsigned family, const Pose& nativeRight,
                 Value& out, bool familyOnly = false) const
    {
        if (hand > 1 || !model || !nativeRight.valid())
            return false;
        const Record* rf = family ? find(Key(1, anchor, family, true)) : NULL;
        const Record* re = familyOnly ? NULL : find(Key(1, anchor, model));
        if ((!familyOnly && !re) || (family && !rf) || (familyOnly && !family))
            return false;
        Value v;
        v.pose = nativeRight;
        if (rf)
            rightLayer(*rf, v);
        if (re)
            rightLayer(*re, v);
        if (!hand)
        {
            const Pose right = v.pose;
            for (unsigned p = 0; p < 6; ++p)
                v.pose.value[p] = right.value[p] * mirrorSign(p);
            v.follow = 63;
            const Record* lf = family ? find(Key(0, anchor, family, true)) : NULL;
            const Record* le = familyOnly ? NULL : find(Key(0, anchor, model));
            if ((!familyOnly && !le) || (family && !lf))
                return false;
            if (lf)
                leftLayer(*lf, right, v);
            if (le)
                leftLayer(*le, right, v);
        }
        out = v;
        return true;
    }
    bool edit(const Key& key, unsigned parameter, int value)
    {
        if (!key.valid() || parameter >= 6 || value < -(parameter < 3 ? 150 : 180) ||
            value > (parameter < 3 ? 150 : 180))
            return false;
        const size_t n = acquire(key);
        Record next = entries_[n].record;
        next.mask |= 1u << parameter;
        next.follow &= ~(1u << parameter);
        next.value.value[parameter] = value;
        next.loaded = PRESENT;
        next.saved = true;
        const bool ok = backend_.write(key, &next);
        entries_[n].record.saved = ok;
        if (ok)
        {
            entries_[n].record = next;
            ++revision_;
        }
        return ok;
    }
    bool reset(const Key& key, unsigned parameter)
    {
        if (!key.valid() || parameter >= 6)
            return false;
        const size_t n = acquire(key);
        Record next = entries_[n].record;
        next.mask &= ~(1u << parameter);
        if (!key.hand)
            next.follow |= 1u << parameter;
        next.value.value[parameter] = 0;
        next.saved = true;
        const bool any = (next.mask | next.follow) != 0;
        const bool ok = backend_.write(key, any ? &next : NULL);
        entries_[n].record.saved = ok;
        if (ok)
        {
            next.loaded = any ? PRESENT : ABSENT;
            entries_[n].record = next;
            ++revision_;
        }
        return ok;
    }
    unsigned revision() const
    {
        return revision_;
    }
    bool resetAll(const Key& key)
    {
        if (!key.valid())
            return false;
        const size_t n = acquire(key);
        Record next = entries_[n].record;
        next.mask = 0;
        next.follow = key.hand ? 0 : 63;
        next.value = Pose();
        next.loaded = next.follow ? PRESENT : ABSENT;
        next.saved = true;
        const bool ok = backend_.write(key, next.follow ? &next : NULL);
        entries_[n].record.saved = ok;
        if (ok)
        {
            entries_[n].record = next;
            ++revision_;
        }
        return ok;
    }
};
}
