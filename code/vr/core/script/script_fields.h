#pragma once
#include <stddef.h>
#include <string.h>
// Read-only schema traversal. Uses registered labels/types/byte offsets, not
// guessed script-global indices. Source and negative controls use this same gate.
namespace gx_script
{
template <class Schema> bool integerOffset(Schema& s, const char* path, size_t span, size_t& offset)
{
    if (!path || !span || span > 64u * 1024u * 1024u || !s.closed(0))
        return false;
    unsigned length = 0;
    while (path[length] && length < 384)
        ++length;
    if (!length || length >= 384)
        return false;
    int node = 0, visited[12] = {};
    unsigned depth = 0, at = 0;
    offset = 0;
    while (at < length)
    {
        if (depth >= 12 || !s.closed(node))
            return false;
        for (unsigned k = 0; k < depth; ++k)
            if (visited[k] == node)
                return false;
        visited[depth++] = node;
        unsigned end = at;
        while (end < length && path[end] != '/')
            ++end;
        const unsigned count = end - at;
        if (!count || count >= 96 || end + 1 == length)
            return false;
        char label[96];
        memcpy(label, path + at, count);
        label[count] = 0;
        const int members = s.members(node);
        if (members < 1 || members > 65536)
            return false;
        int found = -1;
        for (int m = 0; m < members; ++m)
        {
            const char* name = s.name(node, m);
            if (name && !strcmp(name, label))
            {
                if (found >= 0)
                    return false;
                found = m;
            }
        }
        if (found < 0)
            return false;
        const size_t relative = s.offset(node, found);
        if (offset > span || relative > span - offset)
            return false;
        offset += relative;
        const int type = s.type(node, found);
        if (end == length)
            return type == -1 && offset % 8 == 0 && offset <= span && span - offset >= 8;
        if (type <= 0 || !s.closed(type))
            return false;
        node = type;
        at = end + 1;
    }
    return false;
}
}
