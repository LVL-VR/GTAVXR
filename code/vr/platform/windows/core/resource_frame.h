#pragma once
#include "../../../core/core/presentation_abi.h"
#include <windows.h>
#include <string.h>
namespace menyoo_vr
{
// Source Texture uses atomic AddRef/Release. The module owns NO texture pointers;
// native UPDATE resolves them, then a retained packet crosses into rendering.
template <class Texture> struct PreparedFrame
{
    DrawFrame drawing;
    Texture* textures[MaxPrimitives];
    bool visible;
    PreparedFrame() : visible(false)
    {
        memset(&drawing, 0, sizeof(drawing));
        memset(textures, 0, sizeof(textures));
    }
    ~PreparedFrame()
    {
        clear();
    }
    void clear()
    {
        for (unsigned i = 0; i < MaxPrimitives; ++i)
        {
            if (textures[i])
                textures[i]->Release();
            textures[i] = NULL;
        }
        visible = false;
        memset(&drawing, 0, sizeof(drawing));
    }

  private:
    PreparedFrame(const PreparedFrame&);
    PreparedFrame& operator=(const PreparedFrame&);
};
template <class Texture> class ResourceFrame
{
    CRITICAL_SECTION mutex_;
    PreparedFrame<Texture> stored_;
    ResourceFrame(const ResourceFrame&);
    ResourceFrame& operator=(const ResourceFrame&);
    struct Lock
    {
        CRITICAL_SECTION* value;
        explicit Lock(CRITICAL_SECTION& section) : value(&section)
        {
            EnterCriticalSection(value);
        }
        ~Lock()
        {
            LeaveCriticalSection(value);
        }
    };

  public:
    ResourceFrame()
    {
        InitializeCriticalSection(&mutex_);
    }
    ~ResourceFrame()
    {
        stored_.clear();
        DeleteCriticalSection(&mutex_);
    }
    // Resolve returns a borrowed texture protected by the owned native script.
    // Transaction retains ALL assets before publishing; missing sprites preserve
    // the prior packet and never produce a half-drawn new menu.
    template <class Resolve> bool publish(const DrawFrame& drawing, Resolve& resolve)
    {
        if (!validDrawFrame(drawing) || !drawing.count)
            return false;
        PreparedFrame<Texture> next;
        next.drawing = drawing;
        for (unsigned i = 0; i < drawing.count; ++i)
            if (drawing.commands[i].kind == 1)
            {
                Texture* texture = resolve(drawing.commands[i]);
                if (!texture)
                    return false;
                texture->AddRef();
                next.textures[i] = texture;
            }
        PreparedFrame<Texture> previous;
        Lock lock(mutex_);
        if (stored_.visible && drawing.generation <= stored_.drawing.generation)
            return false;
        // Transfer old refs out; their destructors run AFTER the lock is released.
        // A final native texture Release can trigger engine/GPU lifetime work.
        for (unsigned i = 0; i < MaxPrimitives; ++i)
        {
            previous.textures[i] = stored_.textures[i];
            stored_.textures[i] = NULL;
        }
        stored_.drawing = drawing;
        stored_.visible = true;
        for (unsigned i = 0; i < MaxPrimitives; ++i)
        {
            stored_.textures[i] = next.textures[i];
            next.textures[i] = NULL;
        }
        return true;
    }
    bool acquire(PreparedFrame<Texture>& out)
    {
        out.clear();
        Lock lock(mutex_);
        if (!stored_.visible)
            return false;
        out.drawing = stored_.drawing;
        out.visible = true;
        for (unsigned i = 0; i < MaxPrimitives; ++i)
        {
            out.textures[i] = stored_.textures[i];
            if (out.textures[i])
                out.textures[i]->AddRef();
        }
        return true;
    }
    void hide()
    {
        PreparedFrame<Texture> previous;
        {
            Lock lock(mutex_);
            for (unsigned i = 0; i < MaxPrimitives; ++i)
            {
                previous.textures[i] = stored_.textures[i];
                stored_.textures[i] = NULL;
            }
            stored_.visible = false;
            memset(&stored_.drawing, 0, sizeof(stored_.drawing));
        }
    }
};
}
