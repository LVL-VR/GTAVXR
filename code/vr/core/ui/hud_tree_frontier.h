#pragma once
#include "hud_isolation_policy.h"
#include "hud_inspection.h"
// Actual native mirror indexer shared with its deterministic regression fixture.
namespace gx_hud_frontier
{
struct Tree
{
    void* characters[gx_hud_isolation::MaxNodes];
    gx_hud_isolation::Node nodes[gx_hud_isolation::MaxNodes];
    char urls[gx_hud_isolation::MaxNodes][512], names[gx_hud_isolation::MaxNodes][96];
    unsigned flags[gx_hud_isolation::MaxNodes], children[gx_hud_isolation::MaxNodes], count;
    bool failed;
    void* rootParent;
    const char* failure;
    unsigned failureDepth;
    void* failureNode;
    void* failureActual;
    void* failureExpected;
    Tree()
        : count(0), failed(false), rootParent(0), failure("ready"), failureDepth(0), failureNode(0),
          failureActual(0), failureExpected(0)
    {
    }
    void reset()
    {
        count = 0;
        failed = false;
        rootParent = 0;
        failure = "ready";
        failureDepth = 0;
        failureNode = failureActual = failureExpected = 0;
    }
    void fail(const char* reason, void* node, unsigned depth, void* actual = 0, void* expected = 0)
    {
        failed = true;
        failure = reason;
        failureNode = node;
        failureDepth = depth;
        failureActual = actual;
        failureExpected = expected;
    }
    void add(void* ch, int parent, unsigned depth)
    {
        if (failed)
            return;
        if (!ch)
        {
            fail("null_child", ch, depth);
            return;
        }
        if (depth > gx_hud_isolation::MaxDepth)
        {
            fail("depth_limit", ch, depth);
            return;
        }
        if (count == gx_hud_isolation::MaxNodes)
        {
            fail("node_limit", ch, depth);
            return;
        }
        for (unsigned i = 0; i < count; ++i)
            if (characters[i] == ch)
            {
                fail("duplicate_identity", ch, depth);
                return;
            }
        gx_hud_observer::CharacterInfo info;
        if (!gx_hud_observer::InspectCharacter(ch, info))
        {
            fail("metadata_read", ch, depth);
            return;
        }
        if (parent >= 0 && info.parent != characters[parent])
        {
            fail("parent_mismatch", ch, depth, info.parent, characters[parent]);
            return;
        }
        const char* url = info.url ? info.url : "";
        // Internal/unloaded children need not have a file URL. They cannot match
        // a product, but are still checked for topology, flags, alpha and liveness.
        if (strlen(url) >= sizeof(urls[0]))
        {
            fail("url_length", ch, depth);
            return;
        }
        const unsigned index = count++;
        characters[index] = ch;
        nodes[index].parent = parent;
        flags[index] = info.flags;
        children[index] = info.children;
        names[index][0] = 0;
        if (parent < 0)
            rootParent = info.parent;
        nodes[index].visible = (info.flags & gx_hud_observer::CharacterVisible) != 0;
        nodes[index].alpha = info.alpha;
        strcpy_s(urls[index], url);
        nodes[index].url = urls[index];
        const char* name = gx_hud_observer::InspectInstanceName(ch);
        if (name && strlen(name) >= sizeof(names[0]))
        {
            fail("instance_name_length", ch, depth);
            return;
        }
        strcpy_s(names[index], name ? name : "");
        nodes[index].name = names[index];
        // Native HUD BaseContainer's COMP_<id> roots are direct children, as
        // recorded in diagnostic85. Journal observes their ORIGINAL subtree
        // draws; own cached bounds are read AFTER that draw. Enumerating their
        // descendants is neither required for root identity nor safe as a
        // shared quota: the hidden COMP_19 wheel alone exceeds256 nodes.
        // Index the root and its complete child frontier, not unrelated internals.
        if (parent >= 0)
            return;
        if (info.children > gx_hud_isolation::MaxNodes - count)
        {
            fail("child_capacity", ch, depth);
            return;
        }
        for (unsigned i = 0; i < info.children && !failed; ++i)
            add(gx_hud_observer::InspectChild(ch, i), (int)index, depth + 1);
    }
    // AFTER original Draw: walk current live nodes, never dereference a stale
    // stored pointer. Root/frontier identity changes reject publication. Internal
    // callbacks/pass/query hazards remain guarded by the original draw Journal.
    bool sameLive(void* ch, int parent, unsigned depth, unsigned& cursor) const
    {
        if (!ch || depth > gx_hud_isolation::MaxDepth || cursor >= count || characters[cursor] != ch)
            return false;
        const unsigned index = cursor++;
        gx_hud_observer::CharacterInfo info;
        if (!gx_hud_observer::InspectCharacter(ch, info) || nodes[index].parent != parent ||
            info.parent != (parent < 0 ? rootParent : characters[parent]) || info.flags != flags[index] ||
            !_finite(info.alpha) || info.alpha != nodes[index].alpha ||
            strcmp(info.url ? info.url : "", urls[index]) || info.children != children[index])
            return false;
        const char* name = gx_hud_observer::InspectInstanceName(ch);
        if (strcmp(name ? name : "", names[index]))
            return false;
        if (parent >= 0)
            return true;
        if (info.children > count - cursor)
            return false;
        for (unsigned i = 0; i < info.children; ++i)
            if (!sameLive(gx_hud_observer::InspectChild(ch, i), (int)index, depth + 1, cursor))
                return false;
        return true;
    }
    bool sameLive(void* root) const
    {
        unsigned cursor = 0;
        return !failed && count && sameLive(root, -1, 0, cursor) && cursor == count;
    }
};
}
