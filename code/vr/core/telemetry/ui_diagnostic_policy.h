#pragma once
#include <stdint.h>
#include <string.h>
namespace gx_ui_diag
{
enum Domain
{
    TreeDomain,
    GateDomain,
    MethodDomain,
    ContextDomain,
    PoseDomain,
    Domains
};
inline unsigned hash(const char* s)
{
    unsigned h = 2166136261u;
    if (s)
        for (unsigned n = 0; s[n] && n < 512; ++n)
            h = (h ^ (unsigned char)s[n]) * 16777619u;
    return h;
}
class Budget
{
    uint64_t key_[Domains], manual_[Domains];
    unsigned automatic_[Domains];
    bool seen_[Domains];

  public:
    Budget()
    {
        memset(key_, 0, sizeof(key_));
        memset(manual_, 0, sizeof(manual_));
        memset(automatic_, 0, sizeof(automatic_));
        memset(seen_, 0, sizeof(seen_));
    }
    bool sample(unsigned domain, uint64_t key, uint64_t request)
    {
        if (domain >= Domains)
            return false;
        const bool changed = !seen_[domain] || key_[domain] != key;
        seen_[domain] = true;
        key_[domain] = key;
        if (request && request != manual_[domain])
        {
            manual_[domain] = request;
            return true;
        }
        if (changed && automatic_[domain] < 4)
        {
            ++automatic_[domain];
            return true;
        }
        return false;
    }
};
inline void escape(const char* input, char* out, unsigned capacity, unsigned maximum)
{
    unsigned used = 0;
    if (!out || !capacity)
        return;
    const char hex[] = "0123456789ABCDEF";
    if (input)
        for (unsigned n = 0; n < maximum && input[n]; ++n)
        {
            const unsigned c = (unsigned char)input[n];
            if (c >= 32 && c <= 126 && c != '"' && c != '\\')
            {
                if (used + 1 >= capacity)
                    break;
                out[used++] = (char)c;
            }
            else
            {
                if (used + 4 >= capacity)
                    break;
                out[used++] = '\\';
                out[used++] = 'x';
                out[used++] = hex[c >> 4];
                out[used++] = hex[c & 15];
            }
        }
    out[used] = 0;
}
}
