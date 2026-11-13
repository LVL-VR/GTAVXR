#pragma once
// OWN price rasterization, not native glyph/style parity. No desktop/window DC.
#include <windows.h>
#include "../../../core/ui/cash_ui_policy.h"
namespace gx_cash_pixels
{
class Bitmap
{
    HDC dc_;
    HBITMAP dib_;
    HGDIOBJ previous_;
    HFONT fonts_[3];
    unsigned char* bits_;
    Bitmap(const Bitmap&);
    Bitmap& operator=(const Bitmap&);

  public:
    Bitmap() : dc_(NULL), dib_(NULL), previous_(NULL), bits_(NULL)
    {
        memset(fonts_, 0, sizeof(fonts_));
    }
    ~Bitmap()
    {
        clear();
    }
    void clear()
    {
        if (dc_ && previous_)
            SelectObject(dc_, previous_);
        for (unsigned i = 0; i < 3; ++i)
            if (fonts_[i])
                DeleteObject(fonts_[i]);
        if (dib_)
            DeleteObject(dib_);
        if (dc_)
            DeleteDC(dc_);
        dc_ = NULL;
        dib_ = NULL;
        previous_ = NULL;
        bits_ = NULL;
        memset(fonts_, 0, sizeof(fonts_));
    }
    bool prepare()
    {
        if (dc_ && dib_ && bits_ && fonts_[0] && fonts_[1] && fonts_[2])
            return true;
        clear();
        dc_ = CreateCompatibleDC(NULL);
        if (!dc_)
            return false;
        BITMAPINFO bi = {};
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = gx_cash_ui::Width;
        bi.bmiHeader.biHeight = -gx_cash_ui::Height;
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
        const int sizes[3] = {42, 32, 24};
        for (unsigned i = 0; i < 3; ++i)
        {
            fonts_[i] = CreateFontW(-sizes[i], 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0,
                                    ANTIALIASED_QUALITY, 0, L"Segoe UI");
            if (!fonts_[i])
            {
                clear();
                return false;
            }
        }
        return true;
    }
    bool paint(const gx_cash_ui::Snapshot& s)
    {
        if (!s.known || !gx_cash_ui::textValid(s.text) || !prepare())
            return false;
        memset(bits_, 0, gx_cash_ui::Width * gx_cash_ui::Height * 4);
        HGDIOBJ old = SelectObject(dc_, fonts_[0]);
        if (!old || old == HGDI_ERROR)
            return false;
        bool fit = false;
        for (unsigned i = 0; i < 3; ++i)
        {
            SelectObject(dc_, fonts_[i]);
            SIZE extent = {};
            if (!GetTextExtentPoint32A(dc_, s.text, (int)strlen(s.text), &extent))
            {
                SelectObject(dc_, old);
                return false;
            }
            if (extent.cx <= gx_cash_ui::Width - 48 && extent.cy <= gx_cash_ui::Height - 24)
            {
                fit = true;
                break;
            }
        }
        if (!fit)
        {
            SelectObject(dc_, old);
            return false;
        } // Never ellipsize a monetary value.
        // No full rectangle: only native-formatted green monetary glyphs carry alpha.
        const COLORREF color = SetTextColor(dc_, RGB(172, 240, 178));
        const int mode = SetBkMode(dc_, TRANSPARENT);
        RECT label = {24, 12, gx_cash_ui::Width - 24, gx_cash_ui::Height - 12};
        const bool drawn =
            DrawTextA(dc_, s.text, -1, &label, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX) > 0;
        SetBkMode(dc_, mode);
        SetTextColor(dc_, color);
        SelectObject(dc_, old);
        if (!GdiFlush() || !drawn)
            return false;
        for (unsigned i = 0; i < gx_cash_ui::Width * gx_cash_ui::Height * 4; i += 4)
        {
            const unsigned coverage = (unsigned(bits_[i + 1]) * 255 + 120) / 240;
            bits_[i + 3] = (unsigned char)(coverage > 255 ? 255 : coverage);
            // GDI rasterized antialiasing on black. OpenXR carrier uses straight
            // alpha: restore full ink RGB so edge coverage is not multiplied twice.
            if (bits_[i + 3])
            {
                bits_[i] = 178;
                bits_[i + 1] = 240;
                bits_[i + 2] = 172;
            }
        }
        return true;
    }
    const unsigned char* pixels() const
    {
        return bits_;
    }
};
}
