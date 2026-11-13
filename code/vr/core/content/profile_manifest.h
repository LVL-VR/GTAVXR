#pragma once
 
 
// so the CPU controls compile these exact bytes.
//   GTAVXR_CONTENT 1                         first line
//   M <mount> <folder>                       one per mount point (folder relative to the profile folder)
//   F <mount> <relative> <bytes> <rscver>    every served file (rscver 0 = not a resource)
//   S <streaming name> <served path>         streaming redirect to a listed resource of a non-DLC mount
// Tab-separated, ASCII, LF or CRLF. Anything else refuses the whole manifest (no partial load).
#include <stdlib.h>
#include <string.h>
namespace gtavxr_content
{
static const int MaxMounts = 64, MaxFiles = 1024, MaxStreams = 256;
enum Kind
{
    KIND_DATA = 0,
    KIND_DLC = 1,
    KIND_OTHER = 2
};
struct MountRow
{
    char name[64];
    char folder[64];
    char crc[72];
    int kind;
};
struct FileRow
{
    int mount;
    unsigned hash;
    unsigned bytes;
    int rscver;
    char relative[160];
};
struct StreamRow
{
    char name[96];
    char path[200];
    unsigned bytes;
    int rscver;
};
struct Table
{
    bool loaded;
    int mounts, files, streams;
    MountRow m[MaxMounts];
    FileRow f[MaxFiles];
    StreamRow s[MaxStreams];
};
// Bounded copy (lengths are checked before every call).
template <size_t N> inline void Copy(char (&d)[N], const char* s)
{
    size_t n = strlen(s);
    if (n >= N)
        n = N - 1;
    memcpy(d, s, n);
    d[n] = 0;
}
inline char Fold(char c)
{
    if (c == '\\')
        c = '/';
    if (c >= 'A' && c <= 'Z')
        c = char(c - 'A' + 'a');
    return c;
}
inline unsigned PathHash(const char* p)
{
    unsigned h = 2166136261u;
    for (; *p; ++p)
        h = (h ^ (unsigned char)Fold(*p)) * 16777619u;
    return h;
}
inline bool PathEq(const char* a, const char* b)
{
    for (; *a && *b; ++a, ++b)
        if (Fold(*a) != Fold(*b))
            return false;
    return !*a && !*b;
}
inline bool SafeName(const char* s, bool path)
{
    if (!s || !s[0] || strstr(s, "..") || s[0] == '/')
        return false;
    for (; *s; ++s)
    {
        const char c = *s;
        const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                        c == '_' || c == '-' || c == '.' || (path && c == '/');
        if (!ok)
            return false;
    }
    return true;
}
inline bool MountName(const char* s)
{
    // "common:/", "dlcMPBeach:/", "audio:/sfx/S_MINI_AMB/": a device name, one ':/', ends with '/'.
    const char* c = s ? strstr(s, ":/") : 0;
    const size_t n = s ? strlen(s) : 0;
    if (!c || c == s || n < 3 || n >= 60 || s[n - 1] != '/' || strchr(c + 1, ':'))
        return false;
    for (const char* p = s; p < c; ++p)
    {
        const char k = *p;
        if (!((k >= 'A' && k <= 'Z') || (k >= 'a' && k <= 'z') || (k >= '0' && k <= '9') || k == '_'))
            return false;
    }
    return c[2] == 0 || SafeName(c + 2, true);
}
inline int FindMount(const Table& t, const char* name)
{
    for (int i = 0; i < t.mounts; ++i)
        if (!_stricmp(t.m[i].name, name))
            return i;
    return -1;
}
inline char* Field(char*& p)
{
    char* s = p;
    while (*p && *p != '\t')
        ++p;
    if (*p)
        *p++ = 0;
    return s;
}
inline bool Number(const char* s, unsigned long& v)
{
    if (!*s)
        return false;
    unsigned long parsed = 0;
    for (const char* p = s; *p; ++p)
    {
        if (*p < '0' || *p > '9')
            return false;
        const unsigned digit = (unsigned)(*p - '0');
        // Manifest fields are 32-bit regardless of the host compiler's long width.
        if (parsed > (4294967295UL - digit) / 10UL)
            return false;
        parsed = parsed * 10UL + digit;
    }
    v = parsed;
    return true;
}
// check(ctx, folder, relative, bytes, rscver): the file exists with that size and resource version.
typedef bool (*CheckFile)(void* ctx, const char* folder, const char* relative, unsigned bytes, int rscver);
// Fills t from text (modified in place). Returns 0 on success, else the refusal reason; line = the refused line.
inline const char* Parse(Table& t, char* text, CheckFile check, void* ctx, int& line)
{
    memset(&t, 0, sizeof(t));
    line = 0;
    bool header = false;
    for (char* p = text; *p;)
    {
        char* e = p;
        while (*e && *e != '\n')
            ++e;
        char* next = *e ? e + 1 : e;
        *e = 0;
        if (e > p && e[-1] == '\r')
            e[-1] = 0;
        ++line;
        char* q = p;
        p = next;
        if (!*q)
            continue;
        if (!header)
        {
            if (strcmp(q, "GTAVXR_CONTENT 1"))
                return "GTAVXR_CONTENT_MANIFEST_HEADER";
            header = true;
            continue;
        }
        char* tag = Field(q);
        if (!strcmp(tag, "M"))
        {
            char* name = Field(q);
            char* folder = Field(q);
            if (*q || !MountName(name) || !SafeName(folder, false) ||
                strlen(folder) >= sizeof(t.m[0].folder) || FindMount(t, name) >= 0 || t.mounts >= MaxMounts)
                return "GTAVXR_CONTENT_MANIFEST_LINE";
            MountRow& m = t.m[t.mounts++];
            Copy(m.name, name);
            Copy(m.folder, folder);
            const bool data = !_stricmp(name, "common:/") || !_stricmp(name, "platform:/");
            const bool dlc = !data && !_strnicmp(name, "dlc", 3) && _stricmp(name, "dlcpacks:/");
            m.kind = data ? KIND_DATA : dlc ? KIND_DLC : KIND_OTHER;
            m.crc[0] = 0;
            if (data)
                Copy(m.crc, !_stricmp(name, "common:/") ? "commoncrc:/" : "platformcrc:/");
            // MountableContent::InitCrcDevice names it <device up to ':'>CRC:/
            if (dlc)
            {
                const size_t c = strcspn(name, ":");
                memcpy(m.crc, name, c);
                memcpy(m.crc + c, "CRC:/", 6);
            }
        }
        else if (!strcmp(tag, "F"))
        {
            char* name = Field(q);
            char* rel = Field(q);
            char* bytes = Field(q);
            char* ver = Field(q);
            unsigned long b = 0, v = 0;
            const int mi = FindMount(t, name);
            if (*q || mi < 0 || !SafeName(rel, true) || strlen(rel) >= sizeof(t.f[0].relative) ||
                !Number(bytes, b) || !Number(ver, v) || v > 255 || t.files >= MaxFiles)
                return "GTAVXR_CONTENT_MANIFEST_LINE";
            for (int i = 0; i < t.files; ++i)
                if (t.f[i].mount == mi && PathEq(t.f[i].relative, rel))
                    return "GTAVXR_CONTENT_MANIFEST_LINE";
            FileRow& f = t.f[t.files];
            f.mount = mi;
            Copy(f.relative, rel);
            f.hash = PathHash(rel);
            f.bytes = unsigned(b);
            f.rscver = int(v);
            if (!check || !check(ctx, t.m[mi].folder, rel, f.bytes, f.rscver))
                return "GTAVXR_CONTENT_FILE_REJECTED";
            ++t.files;
        }
        else if (!strcmp(tag, "S"))
        {
            char* name = Field(q);
            char* served = Field(q);
            if (*q || !SafeName(name, false) || strlen(name) >= sizeof(t.s[0].name) ||
                strlen(served) >= sizeof(t.s[0].path) || t.streams >= MaxStreams)
                return "GTAVXR_CONTENT_MANIFEST_LINE";
            for (int i = 0; i < t.streams; ++i)
                if (!_stricmp(t.s[i].name, name))
                    return "GTAVXR_CONTENT_MANIFEST_LINE";
            // The served path must be a listed resource of a layer mounted by Apply: DLC layers mount later than the
            // streaming registration.
            int found = -1;
            for (int i = 0; i < t.files && found < 0; ++i)
            {
                const MountRow& m = t.m[t.f[i].mount];
                const size_t n = strlen(m.name);
                if (t.f[i].rscver && m.kind != KIND_DLC && !_strnicmp(served, m.name, n) &&
                    PathEq(served + n, t.f[i].relative))
                    found = i;
            }
            if (found < 0)
                return "GTAVXR_CONTENT_STREAM_REJECTED";
            StreamRow& s = t.s[t.streams++];
            Copy(s.name, name);
            Copy(s.path, served);
            s.bytes = t.f[found].bytes;
            s.rscver = t.f[found].rscver;
        }
        else
            return "GTAVXR_CONTENT_MANIFEST_LINE";
    }
    if (!header)
        return "GTAVXR_CONTENT_MANIFEST_HEADER";
    line = 0;
    t.loaded = true;
    return 0;
}
// A file of mount `mount` at `rel` (the part after the mount point name) is listed.
inline bool Listed(const Table& t, int mount, const char* rel)
{
    const unsigned h = PathHash(rel);
    for (int i = 0; i < t.files; ++i)
        if (t.f[i].mount == mount && t.f[i].hash == h && PathEq(t.f[i].relative, rel))
            return true;
    return false;
}
// Streaming registration: a listed streaming name (base name + extension, the store identity) -> its served path.
inline const StreamRow* Stream(const Table& t, const char* filename)
{
    if (!t.loaded || !filename)
        return 0;
    const char* b = filename;
    for (const char* p = filename; *p; ++p)
        if (*p == '/' || *p == '\\' || *p == ':')
            b = p + 1;
    for (int i = 0; i < t.streams; ++i)
        if (!_stricmp(b, t.s[i].name))
            return &t.s[i];
    return 0;
}
}
