#pragma once
// Own-memory GDI bitmap for NEW contextual cues only. Never paints/recreates
 
// v110-compatible; allocation failures return false and retain no selected GDI
// object. Pixels are straight-alpha BGRA, like the existing native XR menu.
#include <windows.h>
#include <string.h>
#include "../../../core/ui/hand_context.h"
namespace gx_hand_pixels
{
enum
{
    Width = 640,
    Height = 128
};
struct Key
{
    unsigned epoch;
    gx_hand_context::Owner owner;
    gx_hand_context::Action action;
    gx_hand_context::Input input;
    char label[64], binding[96];
    Key()
    {
        memset(this, 0, sizeof(*this));
    }
    explicit Key(const gx_hand_context::Candidate& c)
        : epoch(c.identityEpoch), owner(c.owner), action(c.action), input(c.input)
    {
        memcpy(label, c.label, sizeof(label));
        memcpy(binding, c.binding, sizeof(binding));
    }
    bool same(const Key& b) const
    {
        return epoch == b.epoch && owner == b.owner && action == b.action && input == b.input &&
               !memcmp(label, b.label, sizeof(label)) && !memcmp(binding, b.binding, sizeof(binding));
    }
};
inline bool textContract(const gx_hand_context::Candidate& c)
{
    if (!c.identityEpoch || !c.label[0] || !c.binding[0] || !memchr(c.label, 0, sizeof(c.label)) ||
        !memchr(c.binding, 0, sizeof(c.binding)))
        return false;
    const char* strings[2] = {c.label, c.binding};
    for (unsigned s = 0; s < 2; ++s)
        for (const char* p = strings[s]; *p; ++p)
            if ((unsigned char)*p < 32 || (unsigned char)*p > 126)
                return false;
    return gx_hand_context::knownAction(c.owner, c.action, c.input, c.hand);
}
class Bitmap
{
    HDC dc_;
    HBITMAP dib_;
    HGDIOBJ previous_;
    HFONT title_, binding_;
    unsigned char* bits_;
    Bitmap(const Bitmap&);
    Bitmap& operator=(const Bitmap&);
    bool rectangle(const RECT& r, COLORREF color)
    {
        HBRUSH brush = CreateSolidBrush(color);
        if (!brush)
            return false;
        const bool ok = FillRect(dc_, &r, brush) != 0;
        DeleteObject(brush);
        return ok;
    }
    bool text(const RECT& r, const char* s, HFONT font, COLORREF color, bool& clipped)
    {
        HGDIOBJ old = SelectObject(dc_, font);
        if (!old || old == HGDI_ERROR)
            return false;
        SIZE extent;
        const bool measured = GetTextExtentPoint32A(dc_, s, (int)strlen(s), &extent) != 0;
        if (measured && extent.cx > r.right - r.left)
            clipped = true;
        const COLORREF previousColor = SetTextColor(dc_, color);
        const int previousMode = SetBkMode(dc_, TRANSPARENT);
        RECT actual = r;
        const bool ok = DrawTextA(dc_, s, -1, &actual,
                                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX) > 0;
        SetBkMode(dc_, previousMode);
        SetTextColor(dc_, previousColor);
        SelectObject(dc_, old);
        return measured && ok;
    }

  public:
    Bitmap() : dc_(NULL), dib_(NULL), previous_(NULL), title_(NULL), binding_(NULL), bits_(NULL) {}
    ~Bitmap()
    {
        clear();
    }
    void clear()
    {
        if (dc_ && previous_)
            SelectObject(dc_, previous_);
        if (title_)
            DeleteObject(title_);
        if (binding_)
            DeleteObject(binding_);
        if (dib_)
            DeleteObject(dib_);
        if (dc_)
            DeleteDC(dc_);
        dc_ = NULL;
        dib_ = NULL;
        previous_ = NULL;
        title_ = binding_ = NULL;
        bits_ = NULL;
    }
    bool prepare()
    {
        if (dc_ && dib_ && title_ && binding_ && bits_)
            return true;
        clear();
        dc_ = CreateCompatibleDC(NULL);
        if (!dc_)
            return false;
        BITMAPINFO bi;
        memset(&bi, 0, sizeof(bi));
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = Width;
        bi.bmiHeader.biHeight = -Height;
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        dib_ = CreateDIBSection(dc_, &bi, DIB_RGB_COLORS, (void**)&bits_, NULL, 0);
        if (!dib_ || !bits_)
        {
            clear();
            return false;
        }
        previous_ = SelectObject(dc_, dib_);
        if (!previous_ || previous_ == HGDI_ERROR)
        {
            previous_ = NULL;
            clear();
            return false;
        }
        title_ = CreateFontW(-32, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0,
                             ANTIALIASED_QUALITY, 0, L"Segoe UI");
        binding_ = CreateFontW(-24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0,
                               ANTIALIASED_QUALITY, 0, L"Segoe UI");
        if (!title_ || !binding_)
        {
            clear();
            return false;
        }
        return true;
    }
    bool paint(const gx_hand_context::Candidate& c, bool& clipped)
    {
        clipped = false;
        if (!textContract(c) || !prepare())
            return false;
        memset(bits_, 0, Width * Height * 4);
        const RECT background = {8, 8, Width - 8, Height - 8}, stripe = {8, 8, Width - 8, 12};
        const RECT label = {24, 19, Width - 24, 62}, binding = {24, 66, Width - 24, 106};
        const bool ok = rectangle(background, RGB(18, 24, 23)) && rectangle(stripe, RGB(106, 207, 130)) &&
                        text(label, c.label, title_, RGB(242, 245, 242), clipped) &&
                        text(binding, c.binding, binding_, RGB(207, 214, 209), clipped);
        // Synchronize GDI before reading a DIB section. No screen/user input is involved.
        if (!GdiFlush() || !ok)
            return false;
        for (unsigned n = 0; n < Width * Height * 4; n += 4)
        {
            const bool ink = bits_[n] || bits_[n + 1] || bits_[n + 2];
            const bool bg = bits_[n] == 23 && bits_[n + 1] == 24 && bits_[n + 2] == 18;
            bits_[n + 3] = ink ? (bg ? 208 : 255) : 0;
        }
        return !clipped; // Never advertise an unreadable/ellipsized required input.
    }
    const unsigned char* pixels() const
    {
        return bits_;
    }
};
}
