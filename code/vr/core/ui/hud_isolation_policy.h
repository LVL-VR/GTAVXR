#pragma once
// Bounded read-only plan for an existing AS2 loaded display tree. No native
// pointers, movie update, visibility mutation or GPU call in this policy.
#include <math.h>
#include <float.h>
#include <string.h>
namespace gx_hud_isolation
{
enum
{
    MaxNodes = 256,
    MaxDepth = 12
};
struct Node
{
    int parent;
    bool visible;
    double alpha;
    const char* url;
    const char* name;
    Node() : parent(-1), visible(false), alpha(0), url(0), name(0) {}
};
enum Status
{
    Ready,
    Invisible,
    Missing,
    Ambiguous,
    InvalidTree,
    Budget
};
inline bool productGap(Status s)
{
    return s == Missing || s == Ambiguous;
}
struct Plan
{
    Status status;
    int target;
    unsigned hiddenCount, hidden[MaxNodes];
    Plan() : status(Missing), target(-1), hiddenCount(0) {}
};
inline unsigned lower(unsigned c)
{
    return c >= 'A' && c <= 'Z' ? c + 32 : c;
}
inline bool token(const char* path, char* out)
{
    if (!path)
        return false;
    const char* base = path;
    for (const char* p = path; *p; ++p)
        if (*p == '/' || *p == '\\')
            base = p + 1;
    unsigned n = 0;
    for (const char* p = base; *p && *p != '.'; ++p)
    {
        const unsigned c = lower((unsigned char)*p);
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_') || n == 95)
            return false;
        out[n++] = (char)c;
    }
    if (!n)
        return false;
    out[n] = 0;
    const char* ext = base + n;
    return !*ext || !_stricmp(ext, ".gfx") || !_stricmp(ext, ".swf");
}
inline bool loadedName(const char* url, const char* filename)
{
    char a[96], b[96];
    return token(url, a) && token(filename, b) && !strcmp(a, b);
}
inline bool ancestor(const Node* n, int a, int b)
{
    for (int p = b; p >= 0; p = n[p].parent)
        if (p == a)
            return true;
    return false;
}
inline bool componentName(const char* name, unsigned id)
{
    if (!name || !id || id > 150 || strncmp(name, "COMP_", 5))
        return false;
    const char* p = name + 5;
    unsigned value = 0;
    if (!*p || (*p == '0' && p[1]))
        return false;
    for (; *p; ++p)
    {
        if (*p < '0' || *p > '9' || value > 150)
            return false;
        value = value * 10 + (*p - '0');
    }
    return value == id;
}
inline Plan resolve(const Node* n, unsigned count, const char* filename, unsigned component = 0)
{
    Plan p;
    if (!n || !count || count > MaxNodes)
    {
        p.status = Budget;
        return p;
    }
    for (unsigned i = 0; i < count; ++i)
    {
        if ((i == 0 && n[i].parent != -1) || (i && (n[i].parent < 0 || n[i].parent >= (int)i)) ||
            !_finite(n[i].alpha))
        {
            p.status = InvalidTree;
            return p;
        }
        unsigned depth = 0;
        for (int j = (int)i; j >= 0; j = n[j].parent)
            if (++depth > MaxDepth + 1)
            {
                p.status = Budget;
                return p;
            }
    }
    // _url comes from GetResourceMovieDef()->GetFileURL(), not the instance's
    // arbitrary name. Descendants of one loaded clip may inherit its URL;
    // choose its topmost load root, rejecting two independent matching clips.
    for (unsigned i = 1; i < count; ++i)
    {
        if (!loadedName(n[i].url, filename))
            continue;
        bool inherited = false;
        for (int j = n[i].parent; j >= 0; j = n[j].parent)
            if (loadedName(n[j].url, filename))
            {
                inherited = true;
                break;
            }
        if (inherited)
            continue;
        if (p.target >= 0)
        {
            p.status = Ambiguous;
            return p;
        }
        p.target = (int)i;
    }
    // Native BaseContainer embeds HELP/Wanted via attachMovie in hud.gfx.
    // Their resource URL is HUD, not HUD_HELP_TEXT/HUD_WANTED_STARS. Resolve
    // the unique LIVE native component instance, never a rectangular HUD area.
    if (p.target < 0 && component)
    {
        for (unsigned i = 1; i < count; ++i)
            if (componentName(n[i].name, component))
            {
                if (p.target >= 0)
                {
                    p.status = Ambiguous;
                    return p;
                }
                p.target = (int)i;
            }
    }
    if (p.target < 0)
        return p;
    for (int j = p.target; j >= 0; j = n[j].parent)
        if (!n[j].visible || n[j].alpha <= 0)
        {
            p.status = Invisible;
            return p;
        }
    // Hide only sibling frontiers along the exact root->target path. Internal
    // target alpha, clipping/mask flags, transforms and timing are untouched.
    for (unsigned i = 1; i < count; ++i)
        if (!ancestor(n, (int)i, p.target) && !ancestor(n, p.target, (int)i) &&
            ancestor(n, n[i].parent, p.target) && n[i].visible)
            p.hidden[p.hiddenCount++] = i;
    p.status = Ready;
    return p;
}
template <class Access> class VisibilityScope
{
    Access& access;
    const Plan& plan;
    unsigned attempted;
    bool restored;

  public:
    VisibilityScope(Access& a, const Plan& p) : access(a), plan(p), attempted(0), restored(false) {}
    bool hide()
    {
        if (plan.status != Ready)
            return false;
        while (attempted < plan.hiddenCount)
        {
            const unsigned index = plan.hidden[attempted++];
            // Include the attempted write in rollback even if its return is false.
            if (!access.visible(index, false))
            {
                restore();
                return false;
            }
        }
        return true;
    }
    bool restore()
    {
        if (restored)
            return true;
        bool good = true;
        while (attempted)
            if (!access.visible(plan.hidden[--attempted], true))
                good = false;
        for (unsigned i = 0; i < plan.hiddenCount; ++i)
            if (!access.isVisible(plan.hidden[i], true))
                good = false;
        restored = true;
        if (!good)
            access.restoreFailed();
        return good;
    }
    ~VisibilityScope()
    {
        restore();
    }

  private:
    VisibilityScope(const VisibilityScope&);
    VisibilityScope& operator=(const VisibilityScope&);
};
}
