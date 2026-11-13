#pragma once
#include "large_notice_policy.h"
// Presentation metadata, not separate implementations for each activity. All
// declared screen apps use the same native draw/crop/carrier/input pipeline.
// A filename alone NEVER carries an unrelated INSTRUCTIONAL_BUTTONS instance.
namespace gx_screen_ui
{
inline bool companion(const char* file)
{
    return gx_large_notice::movie(file, "INSTRUCTIONAL_BUTTONS");
}
inline bool physicalOrSpecial(const char* file)
{
    const char* names[] = {"CELLPHONE_BADGER",       "CELLPHONE_IFRUIT", "CELLPHONE_FACADE",
                           "CELLPHONE_PROLOGUE",     "PLAYER_SWITCH",    "PLAYER_SWITCH_STATS_PANEL",
                           "PLAYER_SWITCH_PROLOGUE", "HUD_WEAPON_WHEEL", "HUD_RADIO_WHEEL"};
    for (unsigned n = 0; n < sizeof(names) / sizeof(names[0]); ++n)
        if (gx_large_notice::movie(file, names[n]))
            return true;
    return false;
}
inline bool screenApp(const char* file)
{
    // Source inventory-v3: native web/finance/mission and interactive activity
    // movies. World render targets are excluded by the caller before routing.
    const char* names[] = {"ATM",
                           "WEB_BROWSER",
                           "HACKING_PC",
                           "HACKING_MESSAGE",
                           "MORGUE_LAPTOP",
                           "BANK_JOB_LOGIN",
                           "MPH_BRUTEFORCE_LAPTOP",
                           "SUB_PC",
                           "PSYCHOLOGY_REPORT",
                           "MISSION_QUIT",
                           "MISSION_COMPLETE",
                           "HEIST_ENDSCREEN",
                           "POPUP_WARNING",
                           "SC_LEADERBOARD",
                           "GOLF",
                           "TENNIS",
                           "DARTS_SCOREBOARD",
                           "DARTS_SCOREBOARD_BIKER",
                           "DIGITAL_CAMERA",
                           "CAMERA_GALLERY",
                           "PAUSE_MP_MENU_SIMPLE_LIST",
                           "PAUSE_MP_MENU_VERTICAL_LIST",
                           "PAUSE_MENU_PAGES_WEAPONS",
                           "LETTER_SCRAPS",
                           "FRONTEND_LANDING",
                           "STARTER_PACK_BROWSER"};
    for (unsigned n = 0; n < sizeof(names) / sizeof(names[0]); ++n)
        if (gx_large_notice::movie(file, names[n]))
            return true;
    return false;
}
inline bool interactiveMethod(const char* method)
{
    return gx_large_notice::method(method, "SET_INPUT_EVENT") ||
           gx_large_notice::method(method, "SET_INPUT_EVENT_SELECT") ||
           gx_large_notice::method(method, "SET_INPUT_EVENT_BACK") ||
           gx_large_notice::method(method, "SET_CURSOR_POSITION");
}
inline bool recent(unsigned current, unsigned drawn)
{
    return drawn && current >= drawn && current - drawn <= 1;
}
inline unsigned companionRoute(uint64_t owner, uint64_t primaryOwner, unsigned primaryRoute, bool live)
{
    return owner && owner == primaryOwner && live &&
                   (primaryRoute == GX_UI_MENU_PANEL || primaryRoute == GX_UI_PANEL)
               ? primaryRoute
               : GX_UI_RIGHT_WRIST;
}
}
