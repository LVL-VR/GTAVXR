#pragma once
// Owned metadata only. Caller serializes access. This records FIRST COPY
// interpretation, not readiness, memory ownership, a job fence or a lens pose.
#include <stdint.h>
namespace gx_scope_copy
{
enum Mode
{
    Unknown,
    NormalRows,
    ModelRelativeRows
};
struct Key
{
    uintptr_t buffer;
    uint32_t stamp;
    int offset;
    unsigned count;
    Key() : buffer(0), stamp(0), offset(-1), count(0) {}
    bool valid() const
    {
        return buffer && stamp && offset >= 0 && count > 0 && count <= 256;
    }
    bool same(const Key& v) const
    {
        return valid() && v.valid() && buffer == v.buffer && stamp == v.stamp && offset == v.offset &&
               count == v.count;
    }
};
class Ledger
{
    struct Entry
    {
        Key key;
        Mode mode;
        Entry() : mode(Unknown) {}
    };
    Entry entries_[128];
    unsigned count_;
    uintptr_t buffer_;
    uint32_t stamp_;

  public:
    Ledger() : count_(0), buffer_(0), stamp_(0) {}
    void clear()
    {
        count_ = 0;
        buffer_ = 0;
        stamp_ = 0;
    }
    Mode first(const Key& key, Mode mode)
    {
        if (!key.valid())
            return Unknown;
        if (buffer_ != key.buffer || stamp_ != key.stamp)
        {
            count_ = 0;
            buffer_ = key.buffer;
            stamp_ = key.stamp;
        }
        if (mode != NormalRows && mode != ModelRelativeRows)
            mode = Unknown;
        for (unsigned i = 0; i < count_; ++i)
            if (entries_[i].key.same(key))
            {
                if (entries_[i].mode != mode)
                    entries_[i].mode = Unknown;
                return entries_[i].mode;
            }
        if (count_ == 128)
            return Unknown;
        entries_[count_].key = key;
        entries_[count_++].mode = mode;
        return mode;
    }
    Mode cached(const Key& key) const
    {
        if (!key.valid() || buffer_ != key.buffer || stamp_ != key.stamp)
            return Unknown;
        for (unsigned i = 0; i < count_; ++i)
            if (entries_[i].key.same(key))
                return entries_[i].mode;
        return Unknown;
    }
};
}
