#pragma once
 
// User play derives the install root from the launcher's GTAVXR_BRIDGE_PATH
// (<root>\local-cache\play\releases\<package>\gtavxr_native_openxr.dll); without it (developer runs) the root is GX_DEV_ROOT.
 
// Pure C++ (modern MSVC + VS2012 v110): no OS call, no allocation, ASCII path rules.
#ifndef GX_DEV_ROOT
#define GX_DEV_ROOT ""
#endif
#ifndef GX_DEV_ROOT_W
#define GX_DEV_ROOT_W L""
#endif

namespace gx_paths
{

inline char Lower(char c)
{
    return c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c;
}
inline bool Separator(char c)
{
    return c == '/' || c == '\\';
}

 
// `root` receives the root with its trailing separator, spelled as in `bridge`.
inline bool RootFromBridge(const char* bridge, char* root, unsigned size)
{
    static const char tail[] = "local-cache/play/releases/";
    static const char dll[] = "gtavxr_native_openxr.dll";
    if (!bridge || !root || !size)
        return false;
    unsigned n = 0;
    while (bridge[n])
        ++n;
    const unsigned dn = sizeof(dll) - 1, tn = sizeof(tail) - 1;
    if (n < dn + 2)
        return false;
    for (unsigned k = 0; k < dn; ++k)
        if (Lower(bridge[n - dn + k]) != dll[k])
            return false;
    if (!Separator(bridge[n - dn - 1]) || Separator(bridge[n - dn - 2]))
        return false;
    unsigned p = n - dn - 2;
    while (p > 0 && !Separator(bridge[p]))
        --p;
    if (!Separator(bridge[p]) || p + 1 < tn + 1)
        return false;
    const unsigned start = p + 1 - tn;
    for (unsigned k = 0; k < tn; ++k)
    {
        const char c = bridge[start + k], t = tail[k];
        if (t == '/' ? !Separator(c) : Lower(c) != t)
            return false;
    }
    if (!Separator(bridge[start - 1]) || start >= size)
        return false;
    for (unsigned k = 0; k < start; ++k)
        root[k] = bridge[k];
    root[start] = 0;
    return true;
}

// out = root + rel, every separator written as `sep`; false (out = "") when it does not fit
inline bool Join(const char* root, const char* rel, char sep, char* out, unsigned size)
{
    if (!root || !rel || !out || !size)
        return false;
    unsigned w = 0;
    for (const char* s = root; *s; ++s)
    {
        if (w + 1 >= size)
        {
            out[0] = 0;
            return false;
        }
        out[w++] = Separator(*s) ? sep : *s;
    }
    for (const char* s = rel; *s; ++s)
    {
        if (w + 1 >= size)
        {
            out[0] = 0;
            return false;
        }
        out[w++] = Separator(*s) ? sep : *s;
    }
    out[w] = 0;
    return true;
}

// the install root of this process: from the bridge path when it has the package layout, else the explicitly configured developer root
inline void InstallRoot(const char* bridge, char* root, unsigned size)
{
    if (RootFromBridge(bridge, root, size))
        return;
    static const char dev[] = GX_DEV_ROOT;
    if (size < sizeof(dev))
    {
        if (size)
            root[0] = 0;
        return;
    }
    for (unsigned k = 0; k < sizeof(dev); ++k)
        root[k] = dev[k];
}

// 167-neural A1: `given` (the launcher's explicit path, written with `sep`; `directory` adds a final separator), else
// <package root>/rel when `bridge` has the package layout, else - developer runs only - GX_DEV_ROOT/rel.
// User play never falls back to the developer root: false, out = "".
inline bool Resolve(const char* given, const char* bridge, bool userPlay, const char* rel, char sep,
                    bool directory, char* out, unsigned size)
{
    if (!out || !size)
        return false;
    out[0] = 0;
    if (given && given[0])
    {
        if (!Join(given, "", sep, out, size))
            return false;
        unsigned n = 0;
        while (out[n])
            ++n;
        if (directory && !Separator(out[n - 1]))
        {
            if (n + 1 >= size)
            {
                out[0] = 0;
                return false;
            }
            out[n] = sep;
            out[n + 1] = 0;
        }
        return true;
    }
    char root[400];
    if (RootFromBridge(bridge, root, sizeof(root)))
        return Join(root, rel, sep, out, size);
    if (userPlay)
        return false;
    return GX_DEV_ROOT[0] && Join(GX_DEV_ROOT, rel, sep, out, size);
}

}
