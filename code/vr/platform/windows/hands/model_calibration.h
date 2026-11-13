#pragma once
// Stable model identity + context cache. No engine pointers, tracking writes or per-frame IO.
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
namespace gx_profile
{
// Kind2 is retained only for reading historical post-solve transforms. Support
// POINT records have a new identity: old transforms must never migrate silently.
enum Kind
{
    VEHICLE_CAMERA = 1,
    WEAPON_SECOND_HAND = 2,
    VEHICLE_CLASS = 3,
    CAR_REACH = 4,
    WEAPON_SUPPORT_POINT = 5,
    WEAPON_SUPPORT_CLASS = 6
};
enum VehicleClass
{
    NATIVE = 0,
    CAR = 1,
    MOTORCYCLE = 2,
    AIRCRAFT = 3,
    BOAT = 4
};
enum Layer
{
    NATIVE_DEFAULT = 0,
    CLASS_DEFAULT = 1,
    EXACT_MODEL = 2
};
enum Loaded
{
    ABSENT = 0,
    PRESENT = 1,
    INVALID = 2
};
struct Key
{
    unsigned model;
    int seat;
    Kind kind;
    Key(Kind k = VEHICLE_CAMERA, unsigned m = 0, int s = 0) : model(m), seat(s), kind(k) {}
    bool same(const Key& o) const
    {
        return model == o.model && seat == o.seat && kind == o.kind;
    }
    bool valid() const
    {
        return kind == CAR_REACH
                   ? model == 1 && seat == 0
                   : (kind == VEHICLE_CLASS
                          ? model >= CAR && model <= BOAT && seat == 0
                          : model != 0 && ((kind == WEAPON_SECOND_HAND || kind == WEAPON_SUPPORT_POINT ||
                                            kind == WEAPON_SUPPORT_CLASS)
                                               ? seat == 0
                                               : (kind == VEHICLE_CAMERA && seat >= 0 && seat < 32)));
    }
};
struct Pose
{
    int mm[3], degrees[3];
    Pose()
    {
        for (unsigned c = 0; c < 3; ++c)
            mm[c] = degrees[c] = 0;
    }
    bool same(const Pose& o) const
    {
        for (unsigned c = 0; c < 3; ++c)
            if (mm[c] != o.mm[c] || degrees[c] != o.degrees[c])
                return false;
        return true;
    }
    bool valid(Kind k) const
    {
        const int limit = (k == WEAPON_SECOND_HAND || k == WEAPON_SUPPORT_POINT || k == WEAPON_SUPPORT_CLASS)
                              ? 150
                              : (k == CAR_REACH ? 1000 : 500);
        for (unsigned c = 0; c < 3; ++c)
            if (mm[c] < -limit || mm[c] > limit || degrees[c] < -180 || degrees[c] > 180 ||
                (k != WEAPON_SECOND_HAND && degrees[c] != 0))
                return false;
        return true;
    }
};
// Versioned single value: never publish half an XYZ/rotation record. Bounded strict decoder.
inline bool decode(const char* text, Kind kind, Pose& out)
{
    if (!text || strlen(text) > 128)
        return false;
    long values[7];
    const char* at = text;
    for (unsigned n = 0; n < 7; ++n)
    {
        while (*at == ' ' || *at == '\t')
            ++at;
        if (!*at)
            return false;
        errno = 0;
        char* end = NULL;
        const long value = strtol(at, &end, 10);
        if (errno || end == at || value < -1000 || value > 1000 || (*end && *end != ' ' && *end != '\t'))
            return false;
        values[n] = value;
        at = end;
    }
    while (*at == ' ' || *at == '\t')
        ++at;
    if (*at || values[0] != 1)
        return false;
    Pose next;
    for (unsigned c = 0; c < 3; ++c)
    {
        next.mm[c] = (int)values[c + 1];
        next.degrees[c] = (int)values[c + 4];
    }
    if (!next.valid(kind))
        return false;
    out = next;
    return true;
}
inline bool encode(const Pose& p, Kind k, char* out, size_t size)
{
    if (!out || size < 64 || !p.valid(k))
        return false;
    return sprintf_s(out, size, "1 %d %d %d %d %d %d", p.mm[0], p.mm[1], p.mm[2], p.degrees[0], p.degrees[1],
                     p.degrees[2]) > 0;
}
class Backend
{
  public:
    virtual ~Backend() {}
    virtual Loaded load(const Key& key, Pose& out) = 0;
    virtual bool write(const Key& key, const Pose* value) = 0; // NULL removes ONLY this exact profile
};
class Cache
{
    Cache(const Cache&);
    Cache& operator=(const Cache&); // reference-bound cache is not copyable
    struct Record
    {
        Key key;
        Pose pose;
        Loaded loaded;
        bool saved;
        Record(Key k, Pose p, Loaded state, bool ok = true) : key(k), pose(p), loaded(state), saved(ok) {}
    };
    Backend& backend_;
    std::vector<Record> records_;
    Key current_, fallback_;
    Pose value_;
    Layer layer_;
    bool saved_;
    size_t index(const Key& key)
    {
        for (size_t n = 0; n < records_.size(); ++n)
            if (records_[n].key.same(key))
                return n;
        Pose p;
        Loaded state = key.valid() ? backend_.load(key, p) : ABSENT;
        if (state == PRESENT && !p.valid(key.kind))
            state = INVALID;
        records_.push_back(Record(key, p, state));
        return records_.size() - 1;
    }
    void resolve()
    {
        value_ = Pose();
        layer_ = NATIVE_DEFAULT;
        saved_ = true;
        if (!current_.valid())
            return;
        const size_t n = index(current_);
        const Record exact = records_[n];
        saved_ = exact.saved;
        if (exact.loaded == PRESENT)
        {
            value_ = exact.pose;
            layer_ = EXACT_MODEL;
            return;
        }
        if (fallback_.valid())
        {
            layer_ = CLASS_DEFAULT;
            const Record base = records_[index(fallback_)];
            saved_ = saved_ && base.saved;
            if (base.loaded == PRESENT)
                value_ = base.pose;
        }
    }
    bool selectKeys(Key key, Key fallback)
    {
        if (current_.same(key) && fallback_.same(fallback))
            return false;
        current_ = key;
        fallback_ = fallback;
        resolve();
        return true;
    }
    bool writeKey(Key key, const Pose* p)
    {
        if (!key.valid() || (p && !p->valid(key.kind)))
            return false;
        const bool ok = backend_.write(key, p);
        const size_t n = index(key);
        if (p)
            records_[n].pose = *p;
        records_[n].loaded = p ? PRESENT : ABSENT;
        records_[n].saved = ok;
        resolve();
        return ok;
    }

  public:
    explicit Cache(Backend& backend) : backend_(backend), layer_(NATIVE_DEFAULT), saved_(true) {}
    bool select(Key key, VehicleClass family = NATIVE)
    {
        const Key fallback(VEHICLE_CLASS, (unsigned)family);
        return selectKeys(key, key.kind == VEHICLE_CAMERA ? fallback : Key(VEHICLE_CLASS));
    }
    bool selectSupport(unsigned weapon, unsigned family)
    {
        return selectKeys(Key(WEAPON_SUPPORT_POINT, weapon), Key(WEAPON_SUPPORT_CLASS, family));
    }
    Pose editorValue(bool family)
    {
        if (!family)
            return value_;
        if (!current_.valid() || !fallback_.valid())
            return Pose();
        const Record r = records_[index(fallback_)];
        return r.loaded == PRESENT ? r.pose : Pose();
    }
    bool editSupport(const Pose& p, bool family)
    {
        if (current_.kind != WEAPON_SUPPORT_POINT || !current_.valid())
            return false;
        return writeKey(family ? fallback_ : current_, &p);
    }
    bool resetSupport(bool family)
    {
        if (current_.kind != WEAPON_SUPPORT_POINT || !current_.valid())
            return false;
        return writeKey(family ? fallback_ : current_, NULL);
    }
    bool edit(const Pose& p)
    {
        if (!current_.valid() || !p.valid(current_.kind))
            return false;
        return writeKey(current_, &p);
    }
    bool reset()
    {
        return current_.valid() && writeKey(current_, NULL);
    }
    const Pose& value() const
    {
        return value_;
    }
    const Key& key() const
    {
        return current_;
    }
    Layer layer() const
    {
        return layer_;
    }
    bool saved() const
    {
        return saved_;
    }
    bool active() const
    {
        return current_.valid();
    }
    bool matches(Key key) const
    {
        return active() && current_.same(key);
    }
};
}
