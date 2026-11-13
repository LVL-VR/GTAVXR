#pragma once
// build167 item 3 (Visor V3, plan accepted 2026-10-27): small buttons on a wrist panel, poked with the other hand's finger.
 
// phone. Slots / order are chosen in Settings > HUD > Botones de muneca; labels follow the VR menu language (ES / EN).
 
 
// Card width: gx_panel::BUTTONS_WIDTH * the left wrist scale (* the panel scale on a moved card), passed by the glue.
#include <windows.h>
#include <string.h>
#include <math.h>
namespace gx_wrist_buttons
{
enum Action
{
    NONE,
    PHONE,
    PAUSE_MAP,
    CAMERA,
    RECENTER,
    VR_MENU,
    QUICK,
    REPAIR,
    CLEAR_WANTED,
    HEAL,
    ACTION_COUNT
};
enum
{
    SLOTS = 6,
    COLUMNS = 2,
    CELL_W = 256,
    CELL_H = 112,
    Width = COLUMNS * CELL_W,
    Height = 3 * CELL_H
};
inline const wchar_t* label(unsigned a, bool es)
{
    static const wchar_t* const en[ACTION_COUNT] = {
        L"",        L"Phone",         L"Map / pause", L"Camera",       L"Recenter",
        L"VR menu", L"Quick actions", L"Repair",      L"Clear wanted", L"Heal"};
    static const wchar_t* const sp[ACTION_COUNT] = {L"",
                                                    L"Tel\u00e9fono",
                                                    L"Mapa / pausa",
                                                    L"C\u00e1mara",
                                                    L"Recentrar",
                                                    L"Men\u00fa VR",
                                                    L"Acciones r\u00e1pidas",
                                                    L"Reparar",
                                                    L"Quitar b\u00fasqueda",
                                                    L"Curar"};
    return a < ACTION_COUNT ? (es ? sp[a] : en[a]) : L"";
}
inline const char* labelUtf8(unsigned a, bool es)
{
    static const char* const en[ACTION_COUNT] = {"(none)",       "Phone",   "Map / pause",   "Camera",
                                                 "Recenter",     "VR menu", "Quick actions", "Repair",
                                                 "Clear wanted", "Heal"};
    static const char* const sp[ACTION_COUNT] = {
        "(ninguno)",      "Tel\303\251fono",         "Mapa / pausa", "C\303\241mara",          "Recentrar",
        "Men\303\272 VR", "Acciones r\303\241pidas", "Reparar",      "Quitar b\303\272squeda", "Curar"};
    return a < ACTION_COUNT ? (es ? sp[a] : en[a]) : "";
}
struct Config
{
    int slot[SLOTS];
};
inline Config defaults()
{
    Config c;
    const int d[SLOTS] = {PHONE, PAUSE_MAP, RECENTER, VR_MENU, NONE, NONE};
    for (unsigned i = 0; i < SLOTS; ++i)
        c.slot[i] = d[i];
    return c;
}
inline int sanitize(int a)
{
    return a >= 0 && a < ACTION_COUNT ? a : NONE;
}
// the shown buttons in slot order (empty slots skipped); returns how many
inline unsigned shown(const Config& c, int* out)
{
    unsigned n = 0;
    for (unsigned i = 0; i < SLOTS; ++i)
        if (sanitize(c.slot[i]) != NONE)
            out[n++] = c.slot[i];
    return n;
}
inline unsigned rows(unsigned n)
{
    return (n + COLUMNS - 1) / COLUMNS;
}
// quad height for n buttons on a card w metres wide (the bitmap keeps 3 rows; only the used rows are sent)
inline double heightOf(unsigned n, double w)
{
    return w * double(rows(n) * CELL_H) / double(Width);
}
// panel-local point (x right, y up, centred on the shown rows) -> button index, -1 = none
inline int hit(unsigned n, double w, double x, double y)
{
    if (!n || !(w > 0))
        return -1;
    const double h = heightOf(n, w);
    if (x < -w / 2 || x > w / 2 || y < -h / 2 || y > h / 2)
        return -1;
    const unsigned col = (unsigned)((x + w / 2) / w * COLUMNS), row = (unsigned)((h / 2 - y) / h * rows(n));
    const unsigned i = (row < rows(n) ? row : rows(n) - 1) * COLUMNS + (col < COLUMNS ? col : COLUMNS - 1);
    return i < n ? (int)i : -1;
}
// A poke: the fingertip crosses the panel plane (z from in front to behind) inside a button; re-armed 2 cm in front.
struct Poke
{
    bool armed;
    int hover;
    Poke() : armed(true), hover(-1) {}
    void reset()
    {
        armed = true;
        hover = -1;
    }
    int update(unsigned n, double w, double x, double y, double z)
    {
        const int at = fabs(z) <= .04 ? hit(n, w, x, y) : -1;
        hover = at;
        if (z > .02)
            armed = true;
        if (armed && at >= 0 && z <= 0 && z > -.04)
        {
            armed = false;
            return at;
        }
        if (at < 0 && z <= 0)
            armed = false; // came in from the side / behind: no press until it goes in front again
        return -1;
    }
};
class Bitmap
{
    HDC dc_;
    HBITMAP dib_;
    HGDIOBJ previous_;
    HFONT font_;
    unsigned char* bits_;
    Bitmap(const Bitmap&);
    Bitmap& operator=(const Bitmap&);
    bool fill(const RECT& r, COLORREF color)
    {
        HBRUSH b = CreateSolidBrush(color);
        if (!b)
            return false;
        const bool ok = FillRect(dc_, &r, b) != 0;
        DeleteObject(b);
        return ok;
    }

  public:
    Bitmap() : dc_(NULL), dib_(NULL), previous_(NULL), font_(NULL), bits_(NULL) {}
    ~Bitmap()
    {
        clear();
    }
    void clear()
    {
        if (dc_ && previous_)
            SelectObject(dc_, previous_);
        if (font_)
            DeleteObject(font_);
        if (dib_)
            DeleteObject(dib_);
        if (dc_)
            DeleteDC(dc_);
        dc_ = NULL;
        dib_ = NULL;
        previous_ = NULL;
        font_ = NULL;
        bits_ = NULL;
    }
    bool prepare()
    {
        if (dc_ && dib_ && font_ && bits_)
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
        font_ = CreateFontW(-34, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0,
                            ANTIALIASED_QUALITY, 0, L"Segoe UI");
        if (!font_)
        {
            clear();
            return false;
        }
        return true;
    }
    // straight-alpha BGRA, transparent outside the buttons; hover = lighter button
    bool paint(const int* actions, unsigned n, int hover, bool es)
    {
        if (!prepare() || n > SLOTS)
            return false;
        memset(bits_, 0, Width * Height * 4);
        bool ok = true;
        HGDIOBJ old = SelectObject(dc_, font_);
        if (!old || old == HGDI_ERROR)
            return false;
        const COLORREF previousColor = SetTextColor(dc_, RGB(242, 245, 242));
        const int previousMode = SetBkMode(dc_, TRANSPARENT);
        for (unsigned i = 0; i < n && ok; ++i)
        {
            const int col = (int)(i % COLUMNS), row = (int)(i / COLUMNS);
            const RECT cell = {col * CELL_W + 6, row * CELL_H + 6, (col + 1) * CELL_W - 6,
                               (row + 1) * CELL_H - 6};
            const RECT stripe = {cell.left, cell.top, cell.right, cell.top + 5};
            ok = fill(cell, (int)i == hover ? RGB(52, 78, 64) : RGB(18, 24, 23)) &&
                 fill(stripe, RGB(106, 207, 130));
            RECT text = cell;
            text.top += 5;
            if (ok)
                ok = DrawTextW(dc_, label((unsigned)actions[i], es), -1, &text,
                               DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX) > 0;
        }
        SetBkMode(dc_, previousMode);
        SetTextColor(dc_, previousColor);
        SelectObject(dc_, old);
        if (!GdiFlush() || !ok)
            return false;
        for (unsigned p = 0; p < Width * Height * 4; p += 4)
        {
            const bool ink = bits_[p] || bits_[p + 1] || bits_[p + 2];
            bits_[p + 3] = ink ? 230 : 0;
        }
        return true;
    }
    const unsigned char* pixels() const
    {
        return bits_;
    }
};
} // namespace gx_wrist_buttons
