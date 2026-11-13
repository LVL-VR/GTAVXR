#pragma once
// Update-owned native cash and native-formatted text. No Stats/native pointers
// or gameplay decisions are read by the render consumer. VS2012 compatible.
#include <stdint.h>
#include <string.h>
namespace gx_cash_ui
{
enum
{
    TextBytes = 64,
    Width = 512,
    Height = 96,
    MaxAgeMs = 500
};
struct Snapshot
{
    int64_t value;
    uint64_t tick;
    unsigned epoch, activation, style;
    bool known;
    char text[TextBytes];
    Snapshot()
    {
        memset(this, 0, sizeof(*this));
    }
};
inline bool textValid(const char* text)
{
    if (!text || !text[0])
        return false;
    for (unsigned i = 0; i < TextBytes; ++i)
    {
        const unsigned char c = (unsigned char)text[i];
        if (!c)
            return true;
        if (c < 32 || c > 126)
            return false;
    }
    return false; // Current native formatter is ASCII. Unknown encoding fails closed.
}
struct Gate
{
    uint64_t now, frame, generation, poseFrame, poseGeneration;
    unsigned epoch, activation, handMask;
    bool ready, head, ordinary, wrist, modal;
    Gate()
        : now(0), frame(0), generation(0), poseFrame(0), poseGeneration(0), epoch(0), activation(0),
          handMask(0), ready(false), head(false), ordinary(false), wrist(false), modal(true)
    {
    }
};
class Publication
{
    Snapshot value_;

  public:
    void clear()
    {
        value_ = Snapshot();
    }
    bool publish(int64_t amount, const char* formatted, unsigned epoch, unsigned activation, uint64_t tick,
                 unsigned style = 1)
    {
        clear();
        if (!epoch || !activation || !tick || !style || !textValid(formatted))
            return false;
        value_.value = amount;
        value_.epoch = epoch;
        value_.activation = activation;
        value_.tick = tick;
        value_.style = style;
        value_.known = true;
        strcpy_s(value_.text, formatted);
        return true;
    }
    bool read(const Gate& g, Snapshot& out) const
    {
        out = Snapshot();
        if (!value_.known || !g.ready || !g.head || !g.ordinary || !g.wrist || g.modal || !(g.handMask & 2) ||
            !g.frame || !g.generation || g.poseFrame != g.frame || g.poseGeneration != g.generation ||
            !g.epoch || !g.activation || g.epoch != value_.epoch || g.activation != value_.activation ||
            g.now < value_.tick || g.now - value_.tick > MaxAgeMs)
            return false;
        out = value_;
        return true;
    }
};
struct Key
{
    int64_t value;
    unsigned epoch, activation, style;
    char text[TextBytes];
    Key()
    {
        memset(this, 0, sizeof(*this));
    }
    explicit Key(const Snapshot& s) : value(s.value), epoch(s.epoch), activation(s.activation), style(s.style)
    {
        memcpy(text, s.text, sizeof(text));
    }
    bool same(const Key& b) const
    {
        return value == b.value && epoch == b.epoch && activation == b.activation && style == b.style &&
               !memcmp(text, b.text, sizeof(text));
    }
};
inline float wristWidth(unsigned w, unsigned h, bool moneyOnly)
{
    if (!w || !h)
        return 0;
    const float aspect = float(h) / float(w);
    return moneyOnly ? .24f : (aspect > 1.3f ? .52f / aspect : .40f);
}
}
