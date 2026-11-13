#pragma once
// Nonpausing menu/input ownership. Pure policy; game actions execute on update thread.
#include "../xr/locomotion.h"
#include "../core/root_policy.h"
#include "../ui/appearance_editor.h"
#include "../hands/calibration.h"
#include "../ui/wrist_calibration.h"
#include "../ui/ui_element_calibration.h"
#include "../ui/ui_visibility.h"
#include "../physics/physical_tuning.h"
#include "../xr/pacing_policy.h"
#include "../scope/sniper_eye.h"      // build141 E: Scope Eye selector policy/labels
#include "../scope/scope_mode.h"      // build143 F5: scope view / panel size
#include "../xr/quality.h"            // build141 C: optional quality page policy
#include "../weapons/reload_family.h" // build149 D/F: recoil mode + reload switch per weapon family
#include "../vehicles/vehicle_camera.h"
#include "../body/body_view_situation.h" // build167 item 5
#include "../input/button_map.h"  
namespace gx_menu
{
enum Command
{
    NONE,
    RECENTER,
    HEIGHT_UP,
    HEIGHT_DOWN,
    PAUSE_GAME,
    HEAL,
    ARMOUR,
    CLEAR_WANTED,
    SPAWN_VEHICLE,
    VIEW_GAME,
    GOD,
    STAMINA,
    NEVER_WANTED,
    WANTED_UP,
    GIVE_WEAPON,
    AMMO,
    REPAIR_VEHICLE,
    CLEAN_VEHICLE,
    SET_WEATHER,
    RESET_WEATHER,
    SET_TIME,
    TIME_UP,
    TIME_DOWN,
    TIME_FREEZE,
    REFRESH_CATALOG,
    GIVE_ALL,
    INFINITE_AMMO,
    INFINITE_CLIP,
    FIRE_AMMO,
    EXPLOSIVE_AMMO,
    PARACHUTE,
    SUPER_JUMP,
    SUPER_STRENGTH,
    EXPLOSIVE_MELEE,
    FAST_RUN,
    FAST_SWIM,
    SPECIAL_FILL,
    SPECIAL_INFINITE,
    VEHICLE_GOD,
    VEHICLE_AUTO_REPAIR,
    VEHICLE_UNLOCK,
    ENGINE_START,
    WANTED_DOWN,
    RESET_CHEATS,
    RESET_VEHICLE_PROFILE,
    RESET_SECOND_PROFILE,
    SECOND_PROFILE_SCOPE,
    SWITCH_PROTAGONIST,
    CHANGE_PLAYER_MODEL,
    RESTORE_STORY_MODEL,
    ANCHOR_CONTEXT,
    ANCHOR_SCOPE,
    ANCHOR_EDIT,
    ANCHOR_RESET,
    CHARACTER_MENU,
    MEASURE_HEIGHT,
    EYE_HEIGHT_UP,
    EYE_HEIGHT_DOWN, // build165 item 18
    PANEL_MOVE,
    PANEL_SELECT,
    PANEL_ANCHOR,
    PANEL_FACE,
    PANEL_SIZE,
    PANEL_COPY,
    PANEL_PASTE,
    PANEL_MODEL,
    PANEL_RESET,
    PANEL_RESET_ALL
}; // build167 items 1/2 (Result.vehicle 1 = right, 2 = left on value rows)
enum Page
{
    OPTIONS,
    TRAINER,
    VEHICLES,
    VEHICLE_CLASSES,
    PLAYER,
    WEAPONS,
    VEHICLE_ACTIONS,
    WEATHER,
    CLOCK_PAGE,
    WEAPON_CLASSES,
    WEAPON_ITEMS,
    ABILITIES,
    CALIBRATION,
    HAND_POSITION_L,
    HAND_POSITION_R,
    SETTINGS,
    HANDS,
    WEAPON_CALIBRATION,
    HEIGHT,
    HAPTICS,
    HUD,
    VEHICLE_CALIBRATION,
    SECOND_HAND,
    PROTAGONISTS,
    MODEL_CHANGER,
    MODEL_CLASSES,
    MODEL_ITEMS,
    ANCHOR_CALIBRATION,
    WRIST_CALIBRATION,
    WRIST_PROFILE,
    WRIST_ELEMENTS,
    WRIST_ELEMENT_PROFILE,
    PHYSICS,
    HUD_VISIBILITY,
    MENU_APPEARANCE,
    MENU_COLOURS,
    MENU_COLOUR,
    SNIPER_SCOPE,
    QUALITY,
    WEAPON_SETTINGS_PAGE,
    RELOAD_SETTINGS_PAGE,
    VEHICLE_SETTINGS,
    IMAGE,
    VEHICLE_CAMERA_SETTINGS_PAGE,
    FLIGHT,
    MOVEMENT,
    PARACHUTE_SETTINGS_PAGE,
    BUTTON_SETTINGS_PAGE,
    QUICK_ACTIONS_PAGE,
    PHYSICS_ADVANCED,
    MODS,
    PANELS,
    WRIST_BUTTONS,
    BODY_VIEW,
    GRAPHICS_NEURAL
}; // build164: the unreachable retired pages (145 +/- pages, 163 Interfaces/Performance) are gone
enum
{
    WRIST_SLOTS = 6,
    WRIST_ACTIONS = 10
}; // build167 item 3
struct Settings
{
    gx_physical_tuning::Settings physics;
    menyoo_vr::Appearance appearance;
    bool smooth, wrist, steering, drivingRumble, contextIndicators;
    int snapDegrees, smoothDegrees, handPitch[2], hapticPercent, handMm[2][3];
    int weaponMm[3], weaponDegrees[3], vehicleMm[3], secondMm[3], secondDegrees[3];
    int anchorValue[6]; // model editor, NEVER a global/copied-left setting
    GxWristProfiles wrists;
    GxUiElementProfiles elements;
    GxUiVisibility visibility;
    // build141 E: Settings > Sniper scope > Scope Eye. AUTO is an IMPLEMENTATION default only: the user has not
    // chosen a default yet (sniper brief), so it is never presented as an accepted choice.
    int scopeEye;
    int scopeView,
        scopeWidth; // build143 F5: gx_scope143::View (native both/one eye, 141 crop, off) + head-locked panel degrees
    gx_quality::Overrides quality; // build141 C: all Unchanged by default (nothing forced)
     
    bool nativeFrameLimiter;
    // build147 C (user: "quiero que arregles las radios para la 147"): keep MOTOMAMI/Kult/MLR/Still Slipping available.
    bool newRadio;
    // build147 G (user: "fija y deslizante ... POR DEFECTO DEJA DESLIZANTE"): 0 = DESLIZANTE, 1 = FIJO.
    int supportGrip;
    // build147 E (user: "volverlas a 0 y encontrar el lugar AUTOMATICO/CORRECTO"): 0 = automatic OpenXR registration,
    // 1 = previous calibration. The inactive mode keeps its own hand fine-tune values (restorable, never lost).
    int handMode;
    int otherHandMm[2][3];
    int otherHandPitch[2];
    void swapHandMode()
    {
        handMode = handMode ? 0 : 1;
        for (unsigned h = 0; h < 2; ++h)
        {
            int p = handPitch[h];
            handPitch[h] = otherHandPitch[h];
            otherHandPitch[h] = p;
            for (unsigned c = 0; c < 3; ++c)
            {
                int v = handMm[h][c];
                handMm[h][c] = otherHandMm[h][c];
                otherHandMm[h][c] = v;
            }
        }
    }
     
    bool hideHudAim;
     
    // gunStrike149 = gun-butt / pistol-whip NPC strikes; weaponWorld149 = held weapon pushes/hits world objects and
    // vehicles; reload149[family] = gx_reload149::Mode (default QUICK = today's exact reload).
     
    // no longer a test toggle; same settings key legs_soft159 and default ON (build162: corpse grab always on, row removed)
    bool softLegContactsEnabled;
    // build165 items 7/8/9/11: camera mode + follow per gx_vehicle_camera::Type (accepted defaults), item 8 control frame and
    // item 9 level hardening (switches, ON; OFF = build164), flight sensitivity % (plane, helicopter; 100 = today) and the
    // thumbstick pitch invert (OFF = today).
    int vehicleCamera[gx_vehicle_camera::TYPE_COUNT], vehicleFollow[gx_vehicle_camera::TYPE_COUNT],
        flightSensitivity[2];
     
    bool controlFrame, levelHarden, stickDrive;
    bool invertFlightPitch;
    // build165 item 5: left stick throttle / brake while aiming (driver, right grip), switch ON (OFF = 164); item 12: walk
    // direction 0 = HMD (default, today), 1 = where the left controller points
    int moveDirection;
    // build165 item 10: parachute mode 0 NORMAL (today) / 1 IMMERSIVE, camera follows the canopy, dive view, auto-open (OFF = today)
    int parachuteMode;
    bool parachuteFollow, parachuteDive, parachuteAuto;
     
    int buttonMap[gx_button_map::CONTEXT_COUNT][gx_button_map::SOURCE_COUNT];
    // build165 item 17: Imagen screen-effect switches (ON = today) and Fisica > Avanzado thresholds (30 cm / 100 % = today)
    bool screenDamage, screenFilters;
    int legsCrouchCm, strikeSpeedPercent;
    // build166 items 5/6: Mods (100 % / off = today)
    int modsPedPercent, modsTrafficPercent;
    bool modsMpMaps;
     
    bool leftRealistic, oneHandSupport;
    bool
        vehiclePanels; // build167 item 2: the vehicle layouts (ON by default; OFF = the panels of today in vehicles)
    bool wristButtons;
    int wristSlots[WRIST_SLOTS]; // build167 item 3 (OFF by default since script host v11; ON = the card)
    bool
        vrPrompts; // build167 item 7: help text / instructional buttons show the Quest controls (ON by default)
    int bodyView[gx_body_situation::COUNT]; // build167 item 5 (defaults = 166)
    bool headFade;                          // build167 item 4 (ON by default; OFF = as before)
    // 167-neural B5: NR switches (the Suite preset seeds [Graphics] neural / neural_intensity); neuralCompare = A/B test view,
     
    bool neural, neuralCompare;
    int neuralIntensity;
    void defaultBodyView()
    {
        for (unsigned s = 0; s < gx_body_situation::COUNT; ++s)
            bodyView[s] = gx_body_situation::defaultMode(s);
    }
    static bool sameBodyViewSettings(const int* a, const int* b)
    {
        for (unsigned i = 0; i < gx_body_situation::COUNT; ++i)
            if (a[i] != b[i])
                return false;
        return true;
    }
    static bool sameSlots(const int* a, const int* b)
    {
        for (unsigned i = 0; i < WRIST_SLOTS; ++i)
            if (a[i] != b[i])
                return false;
        return true;
    }
    void resetExtendedSettings()
    {
        for (unsigned t = 0; t < gx_vehicle_camera::TYPE_COUNT; ++t)
        {
            vehicleCamera[t] = gx_vehicle_camera::defaultMode(t);
            vehicleFollow[t] = gx_vehicle_camera::DIRECT;
        }
        flightSensitivity[0] = flightSensitivity[1] = 100;
        controlFrame = levelHarden = true;
        invertFlightPitch = false;
        stickDrive = true;
        moveDirection = 0;
        parachuteMode = 0;
        parachuteFollow = parachuteDive = parachuteAuto = false;
        for (unsigned c = 0; c < gx_button_map::CONTEXT_COUNT; ++c)
            for (unsigned b = 0; b < gx_button_map::SOURCE_COUNT; ++b)
                buttonMap[c][b] = gx_button_map::SAME;
        screenDamage = screenFilters = true;
        legsCrouchCm = 30;
        strikeSpeedPercent = 100;
        modsPedPercent = modsTrafficPercent = 100;
        modsMpMaps = false;
        leftRealistic = oneHandSupport = true;
        vehiclePanels = true;
        defaultBodyView();
        headFade = true;
        neural = neuralCompare = false;
        neuralIntensity = 50; // 167-neural B5
        wristButtons = false;
        vrPrompts = true;
        const int slots[WRIST_SLOTS] = {1, 2, 4, 5, 0, 0}; // phone, map / pause, recenter, VR menu
        for (unsigned i = 0; i < WRIST_SLOTS; ++i)
            wristSlots[i] = slots[i];
    }
    bool sameExtendedSettings(const Settings& o) const
    {
        for (unsigned t = 0; t < gx_vehicle_camera::TYPE_COUNT; ++t)
            if (vehicleCamera[t] != o.vehicleCamera[t] || vehicleFollow[t] != o.vehicleFollow[t])
                return false;
        return flightSensitivity[0] == o.flightSensitivity[0] &&
               flightSensitivity[1] == o.flightSensitivity[1] && controlFrame == o.controlFrame &&
               levelHarden == o.levelHarden && invertFlightPitch == o.invertFlightPitch &&
               stickDrive == o.stickDrive && moveDirection == o.moveDirection &&
               parachuteMode == o.parachuteMode && parachuteFollow == o.parachuteFollow &&
               parachuteDive == o.parachuteDive && parachuteAuto == o.parachuteAuto && sameButtons(o) &&
               screenDamage == o.screenDamage && screenFilters == o.screenFilters &&
               legsCrouchCm == o.legsCrouchCm && strikeSpeedPercent == o.strikeSpeedPercent &&
               modsPedPercent == o.modsPedPercent && modsTrafficPercent == o.modsTrafficPercent &&
               modsMpMaps == o.modsMpMaps && leftRealistic == o.leftRealistic &&
               oneHandSupport == o.oneHandSupport && vehiclePanels == o.vehiclePanels &&
               wristButtons == o.wristButtons && sameSlots(wristSlots, o.wristSlots) &&
               vrPrompts == o.vrPrompts && sameBodyViewSettings(bodyView, o.bodyView) &&
               headFade == o.headFade && neural == o.neural && neuralIntensity == o.neuralIntensity;
    }
    bool sameButtons(const Settings& o) const
    {
        for (unsigned c = 0; c < gx_button_map::CONTEXT_COUNT; ++c)
            for (unsigned b = 0; b < gx_button_map::SOURCE_COUNT; ++b)
                if (buttonMap[c][b] != o.buttonMap[c][b])
                    return false;
        return true;
    }
    int recoil;
    bool gunStrike, weaponWorld, helpGlow;
    int reloadModes[gx_reload::FAMILY_COUNT];  
     
    // eyes, support grip FIJO, recoil and every reload REALISTA. Hands stay automatic OpenXR (handMode147 0).
    Settings()
        : appearance(menyoo_vr::defaultAppearance()), smooth(true), wrist(true), steering(true),
          drivingRumble(true), contextIndicators(false), snapDegrees(30), smoothDegrees(120),
          hapticPercent(150), wrists(gx_wrist::defaults()), elements(gx_ui_element::defaults()),
          visibility(gx_ui_visibility::defaults()), scopeEye(gx_sniper_eye::SCOPE_EYE_AUTO),
          scopeView(gx_scope::NATIVE_BOTH), scopeWidth(80), nativeFrameLimiter(false), newRadio(true),
          supportGrip(1), handMode(0), hideHudAim(true), recoil(gx_recoil::REALISTIC), gunStrike(true),
          weaponWorld(true), helpGlow(true), softLegContactsEnabled(true)
    {
        for (unsigned f = 0; f < gx_reload::FAMILY_COUNT; ++f)
            reloadModes[f] = gx_reload::REALISTIC;
        for (unsigned h = 0; h < 2; ++h)
        {
            otherHandPitch[h] = 0;
            for (unsigned c = 0; c < 3; ++c)
                otherHandMm[h][c] = 0;
        }
        for (unsigned p = 0; p < 6; ++p)
            anchorValue[p] = 0;
        handPitch[0] = handPitch[1] = 0;
        for (unsigned h = 0; h < 2; ++h)
            for (unsigned c = 0; c < 3; ++c)
                handMm[h][c] = 0;
        for (unsigned c = 0; c < 3; ++c)
            weaponMm[c] = weaponDegrees[c] = vehicleMm[c] = secondMm[c] = secondDegrees[c] = 0;
        resetExtendedSettings();
    }
    // Model editors are NOT global settings; context changes must never save them as such.
    bool sameGlobal(const Settings& o) const
    {
        return menyoo_vr::sameAppearance(appearance, o.appearance) && visibility.mask == o.visibility.mask &&
               visibility.pausePercent == o.visibility.pausePercent && gx_wrist::same(wrists, o.wrists) &&
               gx_ui_element::same(elements, o.elements) && smooth == o.smooth && wrist == o.wrist &&
               contextIndicators == o.contextIndicators && steering == o.steering &&
               snapDegrees == o.snapDegrees && smoothDegrees == o.smoothDegrees &&
               handPitch[0] == o.handPitch[0] && handPitch[1] == o.handPitch[1] &&
               hapticPercent == o.hapticPercent && drivingRumble == o.drivingRumble &&
               handMm[0][0] == o.handMm[0][0] && handMm[0][1] == o.handMm[0][1] &&
               handMm[0][2] == o.handMm[0][2] && handMm[1][0] == o.handMm[1][0] &&
               handMm[1][1] == o.handMm[1][1] && handMm[1][2] == o.handMm[1][2] &&
               weaponMm[0] == o.weaponMm[0] && weaponMm[1] == o.weaponMm[1] && weaponMm[2] == o.weaponMm[2] &&
               weaponDegrees[0] == o.weaponDegrees[0] && weaponDegrees[1] == o.weaponDegrees[1] &&
               weaponDegrees[2] == o.weaponDegrees[2] && physics.same(o.physics) && scopeEye == o.scopeEye &&
               scopeView == o.scopeView && scopeWidth == o.scopeWidth && quality.same(o.quality) &&
               nativeFrameLimiter == o.nativeFrameLimiter && elements.menuPercent == o.elements.menuPercent &&
               newRadio == o.newRadio && supportGrip == o.supportGrip && handMode == o.handMode &&
               hideHudAim == o.hideHudAim && recoil == o.recoil && gunStrike == o.gunStrike &&
               weaponWorld == o.weaponWorld && helpGlow == o.helpGlow &&
               softLegContactsEnabled == o.softLegContactsEnabled && sameReload(o) &&
               sameExtendedSettings(o) && otherHandPitch[0] == o.otherHandPitch[0] &&
               otherHandPitch[1] == o.otherHandPitch[1] && otherHandMm[0][0] == o.otherHandMm[0][0] &&
               otherHandMm[0][1] == o.otherHandMm[0][1] && otherHandMm[0][2] == o.otherHandMm[0][2] &&
               otherHandMm[1][0] == o.otherHandMm[1][0] && otherHandMm[1][1] == o.otherHandMm[1][1] &&
               otherHandMm[1][2] == o.otherHandMm[1][2];
    }
    bool sameReload(const Settings& o) const
    {
        for (unsigned f = 0; f < gx_reload::FAMILY_COUNT; ++f)
            if (reloadModes[f] != o.reloadModes[f])
                return false;
        return true;
    }
    bool same(const Settings& o) const
    {
        if (!sameGlobal(o))
            return false;
        for (unsigned p = 0; p < 6; ++p)
            if (anchorValue[p] != o.anchorValue[p])
                return false;
        for (unsigned c = 0; c < 3; ++c)
            if (vehicleMm[c] != o.vehicleMm[c] || secondMm[c] != o.secondMm[c] ||
                secondDegrees[c] != o.secondDegrees[c])
                return false;
        return true;
    }
};
 
// presenter (menu_transport Kind: Action 0, Toggle 1, Number 2, Choice 3). Numbers/choices change with stick Left/Right
// (repeat while held); Accept toggles/cycles; the old "+X / -X / Restablecer" sub-pages are gone (one row per value).
enum RowKind
{
    ROW_ACTION = 0,
    ROW_TOGGLE = 1,
    ROW_NUMBER = 2,
    ROW_CHOICE = 3
};
 
 
// one sub-page per category (each option in ONE place), camera, back LAST; 12 rows = one screen without scrolling.
 
enum SettingsRow
{
    S_CALIBRATION,
    S_MOVEMENT,
    S_BODY,
    S_WEAPONS,
    S_PHYSICS,
    S_VEHICLES,
    S_BUTTONS,
    S_HUD,
    S_HAPTICS,
    S_IMAGE,
    S_APPEARANCE,
    S_CAMERA,
    S_BACK,
    S_COUNT
};
enum MovementRow
{
    MV_TURN,
    MV_TURN_RATE,
    MV_DIRECTION,
    MV_PARACHUTE,
    MV_HEAD_FADE,
    MV_BACK,
    MV_COUNT
}; // build165: turn mode, turn rate, walk direction, item 10
enum ParachuteRow
{
    PA_MODE,
    PA_FOLLOW,
    PA_DIVE,
    PA_AUTO,
    PA_BACK,
    PA_COUNT
}; // build165 item 10
enum ButtonMapRow
{
    MAP_CONTEXT,
    MAP_A,
    MAP_B,
    MAP_X,
    MAP_Y,
    MAP_LCLICK,
    MAP_RCLICK,
    MAP_CAPTURE,
    MAP_COPY,
    MAP_RESET,
    MAP_BACK,
    MAP_COUNT
}; // build165 item 13
// build165 item 16: one-press rows, each an existing Command
enum QuickRow
{
    QK_RECENTER,
    QK_HEIGHT_UP,
    QK_HEIGHT_DOWN,
    QK_NOON,
    QK_TIME_UP,
    QK_WEATHER_AUTO,
    QK_REPAIR,
    QK_CLEAN,
    QK_WANTED_CLEAR,
    QK_WANTED_DOWN,
    QK_WANTED_UP,
    QK_HEAL,
    QK_ARMOUR,
    QK_BACK,
    QK_COUNT
};
enum ImageRow
{
    IM_PACING,
    IM_LIMITER,
    IM_QUALITY,
    IM_DAMAGE,
    IM_FILTERS,
    IM_NEURAL,
    IM_BACK,
    IM_COUNT
}; // build163: Performance rows + Calidad grafica sub-page
enum GraphicsRow
{
    GR_NEURAL,
    GR_INTENSITY,
    GR_COMPARE,
    GR_STATUS,
    GR_BACK,
    GR_COUNT
}; // 167-neural B5: Imagen y rendimiento > Graficos neurales
// build163: tracked steering + new radio stations. build165: camera per vehicle type and planes/helicopters sub-pages, the
// item 8 control frame and item 9 level hardening switches (OFF = build164).
enum VehicleSettingsRow
{
    VS_STEERING,
    VS_RADIO,
    VS_CAMERA,
    VS_FLIGHT,
    VS_CONTROL,
    VS_HARDEN,
    VS_STICK,
    VS_BACK,
    VS_COUNT
}; // build165 item 5: VS_STICK165
enum VehicleCameraRow
{
    VC_BACK = 2 * gx_vehicle_camera::TYPE_COUNT,
    VC_COUNT
}; // row 2t = camera mode, 2t+1 = follow of type t
enum FlightRow
{
    FL_PLANE,
    FL_HELICOPTER,
    FL_INVERT,
    FL_BACK,
    FL_COUNT
}; // build165 item 11
enum ScopeRow
{
    SC_VIEW,
    SC_EYE,
    SC_WIDTH,
    SC_BACK,
    SC_COUNT
}; // build163: the help row repeated the eye help (removed)
// build149 Settings > Armas (+150 Help Glow). build163: support grip and sniper scope moved in from Settings, the legs row moved
// to Fisica; reload with its help glow next to it.
enum WeaponsRow
{
    W_SUPPORT_GRIP,
    W_ONE_HAND,
    W_RECOIL,
    W_RELOAD,
    W_LEFT_REALISTIC,
    W_HELP_GLOW,
    W_GUN_STRIKE,
    W_WORLD,
    W_SCOPE,
    W_BACK,
    W_COUNT
};
enum ReloadRow
{
    RL_BACK = gx_reload::FAMILY_COUNT,
    RL_COUNT
}; // rows 0..7 = one switch per family
// build163 HUD e interfaces: the informational layout row (repeated the wrist help) is gone; wrist panels and the native menu
// size moved in from Calibracion (the pause size was in both places, now here only).
enum HudRow
{
    H_WRIST,
    H_HIDE_AIM,
    H_VISIBILITY,
    H_WRIST_CALIBRATION,
    H_PANELS,
    H_BUTTONS,
    H_VRPROMPTS,
    H_INDICATORS,
    H_MENU_SIZE,
    H_PAUSE_SIZE,
    H_BACK,
    H_COUNT
};
enum PhysicsRow
{
    PH_GRAB,
    PH_PUSH,
    PH_BODY,
    PH_LEGS,
    PH_RESET,
    PH_ADVANCED,
    PH_BACK,
    PH_COUNT
}; // build163: Piernas suaves (permanent)
enum AdvancedRow
{
    AD_CROUCH,
    AD_STRIKE,
    AD_RESET,
    AD_BACK,
    AD_COUNT
}; // build165 item 17: Fisica > Avanzado
enum ModsRow
{
    MO_PEDS,
    MO_TRAFFIC,
    MO_MP_MAPS,
    MO_RESET,
    MO_BACK,
    MO_COUNT
}; // build166 items 5/6: Root > Mods
enum PanelRow
{
    PN_MOVE,
    PN_PANEL,
    PN_ANCHOR,
    PN_FACE,
    PN_SIZE,
    PN_COPY,
    PN_PASTE,
    PN_MODEL,
    PN_VEHICLE,
    PN_RESET,
    PN_RESET_ALL,
    PN_BACK,
    PN_COUNT
}; // build167 items 1/2
// build167 item 3: Settings > HUD > Botones de muneca (actions = gx_wrist_buttons::Action, checked in its glue)
enum WristButtonRow
{
    WB_ON,
    WB_SLOT1,
    WB_SLOT2,
    WB_SLOT3,
    WB_SLOT4,
    WB_SLOT5,
    WB_SLOT6,
    WB_BACK,
    WB_COUNT
};
// build167 item 5: Settings > Vista del cuerpo (one row per gx_body_situation::Situation, then apply to all / reset)
enum BodyViewRow
{
    BV_ALL = gx_body_situation::COUNT,
    BV_RESET,
    BV_BACK,
    BV_COUNT
};
enum HapticsRow
{
    HA_PERCENT,
    HA_DRIVING,
    HA_BACK,
    HA_COUNT
};
enum HandsRow
{
    HN_MODE,
    HN_PITCH_L,
    HN_PITCH_R,
    HN_RESET,
    HN_POSITION_L,
    HN_POSITION_R,
    HN_BACK,
    HN_COUNT
};
enum AxisRow
{
    AX_X,
    AX_Y,
    AX_Z,
    AX_RESET,
    AX_BACK,
    AX_COUNT
}; // hand position / vehicle seat
enum HeightRow
{
    HT_RECENTER,
    HT_VALUE,
    HT_MEASURE,
    HT_EYE,
    HT_BACK,
    HT_COUNT
}; // build165 item 18: Medir altura, Altura de ojos
enum ColourRow
{
    CO_R,
    CO_G,
    CO_B,
    CO_A,
    CO_RESET,
    CO_BACK,
    CO_COUNT
};
enum AnchorRow
{
    AN_SCOPE,
    AN_FIRST_VALUE,
    AN_RESET = 7,
    AN_BACK = 8,
    AN_COUNT = 9
}; // rows 1..6 = position XYZ, rotation XYZ
enum WristRow
{
    WR_FIRST_VALUE,
    WR_RESET = 7,
    WR_ELEMENTS = 8,
    WR_BACK = 9,
    WR_COUNT = 10
}; // rows 0..6 = pos XYZ, rot XYZ, size
enum ElementRow
{
    EL_MODE,
    EL_FIRST_VALUE,
    EL_RESET = 8,
    EL_BACK = 9,
    EL_COUNT = 10
}; // rows 1..7 = pos XYZ, rot XYZ, size
const int HAPTIC_VALUES[3] = {100, 150, 200}, SNAP_VALUES[3] = {15, 30, 45}, SMOOTH_VALUES[3] = {60, 90, 120};
// Ordered list step: Left/Right clamp at the ends (slider feel); Accept wraps (cycle). Unknown value -> first entry.
inline int listStep(int value, const int* list, unsigned n, int direction, bool wrap)
{
    unsigned k = 0;
    bool found = false;
    for (unsigned i = 0; i < n; ++i)
        if (list[i] == value)
        {
            k = i;
            found = true;
        }
    if (!found)
        return list[0];
    if (direction > 0)
        k = k + 1 < n ? k + 1 : (wrap ? 0 : n - 1);
    else if (direction < 0)
        k = k > 0 ? k - 1 : (wrap ? n - 1 : 0);
    return list[k];
}
inline int clampStep(int value, int direction, int step, int lo, int hi)
{
    const int v = value + direction * step;
    return v < lo ? lo : (v > hi ? hi : v);
}
// Previous value of a "next" cycle function (all cycles here have <= 32 states).
template <class Next> inline int previousOf(int value, Next next)
{
    int candidate = value;
    for (unsigned guard = 0; guard < 32; ++guard)
    {
        const int n = next(candidate);
        if (n == value)
            return candidate;
        candidate = n;
    }
    return value;
}
inline int nextPacingInt(int p)
{
    return (int)gx_pacing::nextPacing((unsigned)p);
}
inline int nextViewInt(int v)
{
    return gx_scope::nextView(v);
}
inline int nextEyeInt(int e)
{
    return e == gx_sniper_eye::SCOPE_EYE_RIGHT
               ? gx_sniper_eye::SCOPE_EYE_LEFT
               : (e == gx_sniper_eye::SCOPE_EYE_LEFT ? gx_sniper_eye::SCOPE_EYE_AUTO
                                                     : gx_sniper_eye::SCOPE_EYE_RIGHT);
}
struct Result
{
    bool consume, changed;
    Command command;
    unsigned vehicle;
    Result() : consume(false), changed(false), command(NONE), vehicle(0) {}
};
class Menu
{
    bool opened, releaseBarrier, stickLatch, stickXLatch;
    unsigned previous;
    double lastTime, repeatAt, repeatXAt;
    int directionX_;
    Page page_;
    unsigned selected_, offset_, category_, catalogCount_;
    int direction_;
    unsigned buttonContext_;
    bool buttonCapture_; // build165 item 13: context shown, waiting for a button
    void go(Page p)
    {
        page_ = p;
        selected_ = offset_ = catalogCount_ = 0;
        buttonCapture_ = false;
        if (p == OPTIONS)
            legacyTrainer_ = false;
    }
    static unsigned buttons(const GxInput& i)
    {
        return i.buttons & (((i.active_hands & 1) ? (GX_X | GX_Y | GX_LCLICK | GX_MENU) : 0) |
                            ((i.active_hands & 2) ? (GX_A | GX_B | GX_RCLICK) : 0));
    }

  public:
    Settings settings;
    unsigned vehicleModel, weaponModel, weaponGroup;
    int vehicleSeat;
    bool secondFamily;
    unsigned anchorHand, anchorParameter, anchorFollow;
    bool anchorSupport, anchorFamily, anchorSaved;
    unsigned wristHand, wristParameter, wristElement, wristElementParameter;
    unsigned physicsParameter;
    unsigned menuColour;
    bool legacyTrainer_;
    bool protagonistsFromRoot_;
    unsigned pacing;
    bool pacingForced;
    double perfFrameMs, perfRenderMs, perfGpuMs[2]; // build139 F0/F1 (session only)
    Menu()
        : opened(false), releaseBarrier(false), stickLatch(true), stickXLatch(true), previous(0), lastTime(0),
          repeatAt(0), repeatXAt(0), directionX_(0), page_(OPTIONS), selected_(0), offset_(0), category_(0),
          catalogCount_(0), direction_(0), vehicleModel(0), weaponModel(0), weaponGroup(0), vehicleSeat(-1),
          secondFamily(false), anchorHand(1), anchorParameter(0), anchorFollow(0), anchorSupport(false),
          anchorFamily(false), anchorSaved(true), wristHand(0), wristParameter(0), wristElement(0),
          wristElementParameter(0), physicsParameter(0), menuColour(0), legacyTrainer_(false),
          protagonistsFromRoot_(false), pacing(0), pacingForced(false), perfFrameMs(0), perfRenderMs(0)
    {
        perfGpuMs[0] = perfGpuMs[1] = 0;
        buttonContext_ = 0;
        buttonCapture_ = false;
    }
    void returnFromExternal(Page target, const GxInput& input)
    {
        go(target);
        previous = buttons(input);
        stickLatch = stickXLatch = true;
    }
    bool legacyTrainer() const
    {
        return legacyTrainer_;
    }
    void openLegacyTrainer(const GxInput& input)
    {
        legacyTrainer_ = true;
        returnFromExternal(TRAINER, input);
    }
    bool visible() const
    {
        return opened;
    }
    Page page() const
    {
        return page_;
    }
    unsigned selected() const
    {
        return selected_;
    }
    unsigned buttonContext() const
    {
        return buttonContext_;
    }
    bool buttonCapture() const
    {
        return buttonCapture_;
    }
    bool paged() const
    {
        return page_ == VEHICLES || page_ == VEHICLE_CLASSES || page_ == WEAPON_CLASSES ||
               page_ == WEAPON_ITEMS || page_ == WEATHER || page_ == MODEL_CLASSES || page_ == MODEL_ITEMS;
    }
    unsigned rows() const
    {
        if (page_ == IMAGE)
            return IM_COUNT; // build163
        if (page_ == GRAPHICS_NEURAL)
            return GR_COUNT; // 167-neural B5
        if (page_ == VEHICLE_SETTINGS)
            return VS_COUNT;
        if (page_ == VEHICLE_CAMERA_SETTINGS_PAGE)
            return VC_COUNT;
        if (page_ == FLIGHT)
            return FL_COUNT; // build165
        if (page_ == MOVEMENT)
            return MV_COUNT; // build165 item 12
        if (page_ == PARACHUTE_SETTINGS_PAGE)
            return PA_COUNT; // build165 item 10
        if (page_ == BUTTON_SETTINGS_PAGE)
            return MAP_COUNT; // build165 item 13
        if (page_ == QUICK_ACTIONS_PAGE)
            return QK_COUNT; // build165 item 16
        if (page_ == PHYSICS_ADVANCED)
            return AD_COUNT; // build165 item 17
        if (page_ == MODS)
            return MO_COUNT; // build166 items 5/6
        if (page_ == PANELS)
            return PN_COUNT; // build167 items 1/2
        if (page_ == WRIST_BUTTONS)
            return WB_COUNT; // build167 item 3
        if (page_ == BODY_VIEW)
            return BV_COUNT; // build167 item 5
        if (page_ == SNIPER_SCOPE)
            return SC_COUNT; // build143: view, eye, panel size, help (informational), back
        if (page_ == QUALITY)
            return gx_quality::ROW_COUNT; // build141 C
        if (page_ == WEAPON_SETTINGS_PAGE)
            return W_COUNT;
        if (page_ == RELOAD_SETTINGS_PAGE)
            return RL_COUNT;
        if (page_ == MENU_APPEARANCE)
            return 4;
        if (page_ == MENU_COLOURS)
            return menyoo_vr::ColourRoleCount + 1;
        if (page_ == MENU_COLOUR)
            return CO_COUNT; // build145: R/G/B/Alpha number rows + reset + back
        if (page_ == CALIBRATION)
            return 8; // build163: hands, 4 weapon anchors, height, vehicle, back
        if (page_ == HUD_VISIBILITY)
            return gx_ui_visibility::Count + 2;
        if (paged())
            return catalogCount_ ? catalogCount_ : 1;
        if (page_ == WRIST_PROFILE)
            return WR_COUNT;
        if (page_ == WRIST_ELEMENT_PROFILE)
            return wristElement == gx_ui_element::Phone ? 2 : EL_COUNT;
        switch (page_)
        {
        case PHYSICS:
            return PH_COUNT;
        case HUD:
            return H_COUNT;
        case OPTIONS:
            return menyoo_vr::RootRows;
        case SETTINGS:
            return S_COUNT;
        case ANCHOR_CALIBRATION:
            return AN_COUNT;
        case WRIST_CALIBRATION:
            return 3;
        case WRIST_ELEMENTS:
            return wristHand ? gx_ui_element::RightCount + 2 : 3;
        case WEAPON_CALIBRATION:
            return 5;
        case SECOND_HAND:
            return 4;
        case CLOCK_PAGE:
        case ABILITIES:
            return 8;
        case HANDS:
            return HN_COUNT;
        case HAND_POSITION_L:
        case HAND_POSITION_R:
        case VEHICLE_CALIBRATION:
            return AX_COUNT;
        case HEIGHT:
            return HT_COUNT;
        case PROTAGONISTS:
            return 4;
        case HAPTICS:
            return HA_COUNT;
        case MODEL_CHANGER:
            return 3;
        case TRAINER:
        case WEAPONS:
            return 9;
        case PLAYER:
            return 12;
        default:
            return 7;
        }
    }
    unsigned items() const
    {
        const unsigned n = rows() > offset_ ? rows() - offset_ : 0;
        return n > 12 ? 12 : n;
    }
    void ensureVisible()
    {
        if (selected_ >= rows())
            selected_ = rows() - 1;
        if (offset_ > selected_)
            offset_ = selected_;
        if (selected_ >= offset_ + 12)
            offset_ = selected_ - 11;
    }
    unsigned offset() const
    {
        return offset_;
    }
    unsigned category() const
    {
        return category_;
    }
    void catalogCount(unsigned count)
    {
        catalogCount_ = count;
        ensureVisible();
    }
    void close()
    {
        opened = false;
        releaseBarrier = true;
    }
    // build167 item 3: a wrist button opens the menu on a page (same latches as the menu button)
    bool openAt(Page p)
    {
        if (opened || releaseBarrier)
            return false;
        opened = true;
        go(p);
        stickLatch = stickXLatch = true;
        return true;
    }
    static const char* vehicle(unsigned n)
    {
        const char* names[] = {"adder", "sultan", "blista", "banshee", "buffalo", "baller"};
        return n < 6 ? names[n] : NULL;
    }
    // build145: stick Left/Right on the selected row. Returns true when a value changed (or a command was emitted).
    bool adjust(int d, Result& out)
    {
        if (!d)
            return false;
        switch (page_)
        {
        case MOVEMENT: // build165 item 12: the turning rows (moved from Settings) + the walk direction
            if (selected_ == MV_TURN)
            {
                settings.smooth = !settings.smooth;
                return true;
            }
            if (selected_ == MV_DIRECTION)
            {
                settings.moveDirection = settings.moveDirection ? 0 : 1;
                return true;
            }
            if (selected_ == MV_HEAD_FADE)
            {
                settings.headFade = !settings.headFade;
                return true;
            } // build167 item 4
            if (selected_ == MV_TURN_RATE)
            {
                if (settings.smooth)
                    settings.smoothDegrees = listStep(settings.smoothDegrees, SMOOTH_VALUES, 3, d, false);
                else
                    settings.snapDegrees = listStep(settings.snapDegrees, SNAP_VALUES, 3, d, false);
                return true;
            }
            return false;
        case BUTTON_SETTINGS_PAGE:  
            if (selected_ == MAP_CONTEXT)
            {
                buttonContext_ = (buttonContext_ + (d > 0 ? 1 : gx_button_map::CONTEXT_COUNT - 1)) %
                                 gx_button_map::CONTEXT_COUNT;
                return true;
            }
            if (selected_ >= MAP_A && selected_ <= MAP_RCLICK)
            {
                int& t = settings.buttonMap[buttonContext_][selected_ - MAP_A];
                t = (gx_button_map::sanitize(t) + (d > 0 ? 1 : gx_button_map::TARGET_COUNT - 1)) %
                    gx_button_map::TARGET_COUNT;
                return true;
            }
            return false;
        case PARACHUTE_SETTINGS_PAGE: // build165 item 10: Left/Right switches the selected option
            if (selected_ == PA_MODE)
            {
                settings.parachuteMode = settings.parachuteMode ? 0 : 1;
                return true;
            }
            if (selected_ == PA_FOLLOW)
            {
                settings.parachuteFollow = !settings.parachuteFollow;
                return true;
            }
            if (selected_ == PA_DIVE)
            {
                settings.parachuteDive = !settings.parachuteDive;
                return true;
            }
            if (selected_ == PA_AUTO)
            {
                settings.parachuteAuto = !settings.parachuteAuto;
                return true;
            }
            return false;
        case VEHICLE_SETTINGS:
            if (selected_ == VS_STEERING)
            {
                settings.steering = !settings.steering;
                return true;
            }
            if (selected_ == VS_RADIO)
            {
                settings.newRadio = !settings.newRadio;
                return true;
            }
            if (selected_ == VS_CONTROL)
            {
                settings.controlFrame = !settings.controlFrame;
                return true;
            } // build165
            if (selected_ == VS_HARDEN)
            {
                settings.levelHarden = !settings.levelHarden;
                return true;
            }
            if (selected_ == VS_STICK)
            {
                settings.stickDrive = !settings.stickDrive;
                return true;
            }
            return false;
        case VEHICLE_CAMERA_SETTINGS_PAGE: // build165 item 7: Left/Right cycles the mode of the selected row
            if (selected_ < VC_BACK)
            {
                const unsigned t = selected_ / 2;
                int& v = selected_ % 2 ? settings.vehicleFollow[t] : settings.vehicleCamera[t];
                const int n =
                    selected_ % 2 ? (int)gx_vehicle_camera::FOLLOW_COUNT : (int)gx_vehicle_camera::MODE_COUNT;
                v = (v + (d > 0 ? 1 : n - 1)) % n;
                return true;
            }
            return false;
        case FLIGHT: // build165 item 11
            if (selected_ <= FL_HELICOPTER)
            {
                int& v = settings.flightSensitivity[selected_];
                v = clampStep(v, d, 25, 25, 200);
                return true;
            }
            if (selected_ == FL_INVERT)
            {
                settings.invertFlightPitch = !settings.invertFlightPitch;
                return true;
            }
            return false;
        case IMAGE:
            if (selected_ == IM_PACING)
            {
                pacing =
                    (unsigned)(d > 0 ? nextPacingInt((int)pacing) : previousOf((int)pacing, nextPacingInt));
                return true;
            }
            if (selected_ == IM_LIMITER)
            {
                settings.nativeFrameLimiter = !settings.nativeFrameLimiter;
                return true;
            }
            if (selected_ == IM_DAMAGE)
            {
                settings.screenDamage = !settings.screenDamage;
                return true;
            } // build165 item 17
            if (selected_ == IM_FILTERS)
            {
                settings.screenFilters = !settings.screenFilters;
                return true;
            }
            return false;
        case GRAPHICS_NEURAL: // 167-neural B5: Left/Right
            if (selected_ == GR_NEURAL)
            {
                settings.neural = !settings.neural;
                return true;
            }
            if (selected_ == GR_INTENSITY)
            {
                settings.neuralIntensity = clampStep(settings.neuralIntensity, d, 10, 0, 100);
                return true;
            }
            if (selected_ == GR_COMPARE)
            {
                settings.neuralCompare = !settings.neuralCompare;
                return true;
            }
            return false;
        case SNIPER_SCOPE:
            if (selected_ == SC_VIEW)
            {
                settings.scopeView = d > 0 ? nextViewInt(settings.scopeView)
                                           : previousOf(gx_scope::parseView(settings.scopeView), nextViewInt);
                return true;
            }
            if (selected_ == SC_EYE)
            {
                settings.scopeEye =
                    d > 0 ? nextEyeInt(settings.scopeEye) : previousOf(settings.scopeEye, nextEyeInt);
                return true;
            }
            if (selected_ == SC_WIDTH)
            {
                settings.scopeWidth = clampStep(gx_scope::parseWidth(settings.scopeWidth), d, 10, 60, 110);
                return true;
            }
            return false;
        case QUALITY:
            if (selected_ == gx_quality::FXAA)
            {
                settings.quality.fxaa = d > 0 ? gx_quality::nextFxaa(settings.quality.fxaa)
                                              : previousOf(settings.quality.fxaa, gx_quality::nextFxaa);
                return true;
            }
            if (selected_ == gx_quality::TESSELLATION)
            {
                settings.quality.tessellation =
                    d > 0 ? gx_quality::nextTessellation(settings.quality.tessellation)
                          : previousOf(settings.quality.tessellation, gx_quality::nextTessellation);
                return true;
            }
            if (selected_ == gx_quality::VR_SCALE)
            {
                settings.quality.vrScale =
                    d > 0 ? gx_quality::nextVrScale(settings.quality.vrScale)
                          : previousOf(settings.quality.vrScale, gx_quality::nextVrScale);
                return true;
            }
            return false;
        case WEAPON_SETTINGS_PAGE:
            if (selected_ == W_SUPPORT_GRIP)
            {
                settings.supportGrip = settings.supportGrip ? 0 : 1;
                return true;
            }
            if (selected_ == W_RECOIL)
            {
                settings.recoil =
                    settings.recoil == gx_recoil::REALISTIC ? gx_recoil::REDUCED : gx_recoil::REALISTIC;
                return true;
            }
            if (selected_ == W_GUN_STRIKE)
            {
                settings.gunStrike = !settings.gunStrike;
                return true;
            }
            if (selected_ == W_WORLD)
            {
                settings.weaponWorld = !settings.weaponWorld;
                return true;
            }
            if (selected_ == W_LEFT_REALISTIC)
            {
                settings.leftRealistic = !settings.leftRealistic;
                return true;
            } // build167 item 8
            if (selected_ == W_ONE_HAND)
            {
                settings.oneHandSupport = !settings.oneHandSupport;
                return true;
            } // build167 item 9
            if (selected_ == W_HELP_GLOW)
            {
                settings.helpGlow = !settings.helpGlow;
                return true;
            }
            return false;
        case RELOAD_SETTINGS_PAGE:
            if (selected_ < gx_reload::FAMILY_COUNT && gx_reload::realisticAvailable(selected_))
            {
                settings.reloadModes[selected_] = settings.reloadModes[selected_] == gx_reload::REALISTIC
                                                      ? gx_reload::QUICK
                                                      : gx_reload::REALISTIC;
                return true;
            }
            return false;
        case MENU_APPEARANCE:
            if (selected_ == 0)
            {
                settings.appearance.language = settings.appearance.language == menyoo_vr::English
                                                   ? menyoo_vr::Spanish
                                                   : menyoo_vr::English;
                return true;
            }
            return false;
        case MENU_COLOUR:
            if (selected_ <= CO_A)
                return menyoo_vr::editColour(settings.appearance, menuColour, selected_, d * 5);
            return false;
        case HUD:
            if (selected_ == H_WRIST)
            {
                settings.wrist = !settings.wrist;
                return true;
            }
            if (selected_ == H_INDICATORS)
            {
                settings.contextIndicators = !settings.contextIndicators;
                return true;
            }
            if (selected_ == H_VRPROMPTS)
            {
                settings.vrPrompts = !settings.vrPrompts;
                return true;
            } // build167 item 7
            if (selected_ == H_HIDE_AIM)
            {
                settings.hideHudAim = !settings.hideHudAim;
                return true;
            }
            if (selected_ == H_PAUSE_SIZE)
            {
                settings.visibility.pausePercent = clampStep(settings.visibility.pausePercent, d, 5, 40, 200);
                return true;
            }
            if (selected_ == H_MENU_SIZE)
            {
                settings.elements.menuPercent = clampStep(settings.elements.menuPercent, d, 5, 40, 200);
                return true;
            }
            return false;
        case HUD_VISIBILITY:
            if (selected_ < gx_ui_visibility::Count)
            {
                gx_ui_visibility::toggle(settings.visibility, selected_);
                return true;
            }
            return false;
        case PHYSICS:
        {
            settings.physics = gx_physical_tuning::checked(settings.physics);
            if (selected_ == PH_GRAB)
            {
                settings.physics.grabPercent =
                    gx_physical_tuning::adjustPercent(settings.physics.grabPercent, d);
                return true;
            }
            if (selected_ == PH_PUSH)
            {
                settings.physics.pushPercent =
                    gx_physical_tuning::adjustPercent(settings.physics.pushPercent, d);
                return true;
            }
            if (selected_ == PH_BODY)
            {
                settings.physics.bodyPercent =
                    gx_physical_tuning::adjustBodyPercent(settings.physics.bodyPercent, d);
                return true;
            }
            if (selected_ == PH_LEGS)
            {
                settings.softLegContactsEnabled = !settings.softLegContactsEnabled;
                return true;
            }
            return false;
        }
        case PHYSICS_ADVANCED: // build165 item 17
            if (selected_ == AD_CROUCH)
            {
                settings.legsCrouchCm = clampStep(settings.legsCrouchCm, d, 5, 15, 50);
                return true;
            }
            if (selected_ == AD_STRIKE)
            {
                settings.strikeSpeedPercent = clampStep(settings.strikeSpeedPercent, d, 10, 50, 150);
                return true;
            }
            return false;
        case PANELS: // build167 items 1/2: Left/Right on a value row = a command for the panel glue
            if (selected_ == PN_PANEL)
            {
                out.command = PANEL_SELECT;
                out.vehicle = d > 0 ? 1 : 2;
                return true;
            }
            if (selected_ == PN_ANCHOR)
            {
                out.command = PANEL_ANCHOR;
                out.vehicle = d > 0 ? 1 : 2;
                return true;
            }
            if (selected_ == PN_SIZE)
            {
                out.command = PANEL_SIZE;
                out.vehicle = d > 0 ? 1 : 2;
                return true;
            }
            if (selected_ == PN_FACE)
            {
                out.command = PANEL_FACE;
                return true;
            }
            if (selected_ == PN_VEHICLE)
            {
                settings.vehiclePanels = !settings.vehiclePanels;
                return true;
            }
            return false;
        case BODY_VIEW: // build167 item 5: Left/Right cycles the mode of a situation (apply to all: every row)
            if (selected_ < gx_body_situation::COUNT)
            {
                settings.bodyView[selected_] =
                    gx_body_situation::step(selected_, settings.bodyView[selected_], d);
                return true;
            }
            if (selected_ == BV_ALL)
            {
                const int m = gx_body_situation::step(gx_body_situation::FOOT,
                                                      settings.bodyView[gx_body_situation::FOOT], d);
                for (unsigned s = 0; s < gx_body_situation::COUNT; ++s)
                    settings.bodyView[s] = gx_body_situation::sanitize(s, m);
                return true;
            }
            return false;
        case WRIST_BUTTONS: // build167 item 3: Left/Right cycles the action of a button
            if (selected_ == WB_ON)
            {
                settings.wristButtons = !settings.wristButtons;
                return true;
            }
            if (selected_ >= WB_SLOT1 && selected_ <= WB_SLOT6)
            {
                int& v = settings.wristSlots[selected_ - WB_SLOT1];
                v = ((v < 0 || v >= WRIST_ACTIONS ? 0 : v) + (d > 0 ? 1 : WRIST_ACTIONS - 1)) % WRIST_ACTIONS;
                return true;
            }
            return false;
        case MODS: // build166 items 5/6
            if (selected_ == MO_PEDS)
            {
                settings.modsPedPercent = clampStep(settings.modsPedPercent, d, 10, 0, 100);
                return true;
            }
            if (selected_ == MO_TRAFFIC)
            {
                settings.modsTrafficPercent = clampStep(settings.modsTrafficPercent, d, 10, 0, 100);
                return true;
            }
            if (selected_ == MO_MP_MAPS)
            {
                settings.modsMpMaps = !settings.modsMpMaps;
                return true;
            }
            return false;
        case HAPTICS:
            if (selected_ == HA_PERCENT)
            {
                settings.hapticPercent = listStep(settings.hapticPercent, HAPTIC_VALUES, 3, d, false);
                return true;
            }
            if (selected_ == HA_DRIVING)
            {
                settings.drivingRumble = !settings.drivingRumble;
                return true;
            }
            return false;
        case HANDS:
            if (selected_ == HN_MODE)
            {
                settings.swapHandMode();
                return true;
            }
            if (selected_ == HN_PITCH_L || selected_ == HN_PITCH_R)
            {
                int& v = settings.handPitch[selected_ == HN_PITCH_R ? 1 : 0];
                v = clampStep(v, d, 2, -50, 50);
                return true;
            }
            return false;
        case HAND_POSITION_L:
        case HAND_POSITION_R:
            if (selected_ <= AX_Z)
            {
                int& v = settings.handMm[page_ == HAND_POSITION_R ? 1 : 0][selected_];
                v = clampStep(v, d, 2, -150, 150);
                return true;
            }
            return false;
        case VEHICLE_CALIBRATION:
            if (selected_ <= AX_Z && vehicleModel)
            {
                int& v = settings.vehicleMm[selected_];
                v = clampStep(v, d, 2, -500, 500);
                return true;
            }
            return false;
        case HEIGHT:
            if (selected_ == HT_VALUE)
            {
                out.command = d > 0 ? HEIGHT_UP : HEIGHT_DOWN;
                return true;
            }
            if (selected_ == HT_EYE)
            {
                out.command = d > 0 ? EYE_HEIGHT_UP : EYE_HEIGHT_DOWN;
                return true;
            } // build165 item 18
            return false;
        case ANCHOR_CALIBRATION:
            if (selected_ == AN_SCOPE)
            {
                if (weaponModel && weaponGroup)
                {
                    anchorFamily = !anchorFamily;
                    out.command = ANCHOR_SCOPE;
                    return true;
                }
                return false;
            }
            if (selected_ >= AN_FIRST_VALUE && selected_ < AN_FIRST_VALUE + 6 && weaponModel)
            {
                const unsigned p = selected_ - AN_FIRST_VALUE;
                const int limit = p < 3 ? 150 : 180;
                int& v = settings.anchorValue[p];
                v = clampStep(v, d, 2, -limit, limit);
                out.command = ANCHOR_EDIT;
                out.vehicle = p;
                return true;
            }
            return false;
        case WRIST_PROFILE:
            if (selected_ < 7 && wristHand < 2)
            {
                GxWristProfile& p = settings.wrists.hand[wristHand];
                if (selected_ < 3)
                    p.mm[selected_] = clampStep(p.mm[selected_], d, 2, -300, 300);
                else if (selected_ < 6)
                    p.degrees[selected_ - 3] = clampStep(p.degrees[selected_ - 3], d, 2, -180, 180);
                else
                    p.percent = clampStep(p.percent, d, 5, 40, 200);
                return true;
            }
            return false;
        case WRIST_ELEMENT_PROFILE:
        {
            if (wristElement >= gx_ui_element::Count || wristElement == gx_ui_element::Phone)
                return false;
            GxUiElementProfile& p = settings.elements.element[wristElement];
            if (selected_ == EL_MODE)
            {
                p.individual = p.individual ? 0u : 1u;
                return true;
            }
            if (selected_ >= EL_FIRST_VALUE && selected_ < EL_FIRST_VALUE + 7)
            {
                const unsigned n = selected_ - EL_FIRST_VALUE;
                if (n < 3)
                    p.mm[n] = clampStep(p.mm[n], d, 2, -300, 300);
                else if (n < 6)
                    p.degrees[n - 3] = clampStep(p.degrees[n - 3], d, 2, -180, 180);
                else
                    p.percent = clampStep(p.percent, d, 5, 40, 200);
                p.individual = 1;
                return true;
            } // first edit switches the element to Individual (as the old +/- page did)
            return false;
        }
        default:
            return false;
        }
    }
    Result update(const GxInput& i, double now, bool nativePause)
    {
        Result out;
        if (i.size != sizeof(i) || i.version != 1 || !i.active_hands || !gx_loco::finite(now) ||
            !gx_loco::finite(i.stick[0][1]) || !gx_loco::finite(i.stick[1][1]) ||
            !gx_loco::finite(i.stick[0][0]) || !gx_loco::finite(i.stick[1][0]))
        {
            opened = false;
            releaseBarrier = true;
            previous = GX_MENU | GX_A | GX_B;
            stickLatch = stickXLatch = true;
            return out;
        }
        const unsigned b = buttons(i), pressed = b & ~previous;
        previous = b;
        bool neutral = b == 0;
        for (int h = 0; h < 2; ++h)
            if (i.active_hands & (1u << h))
                neutral = neutral && fabs(i.stick[h][0]) < .25 && fabs(i.stick[h][1]) < .25 &&
                          i.trigger[h] < .2 && i.grip[h] < .2;
        if (nativePause)
        {
            opened = false;
            releaseBarrier = false;
            return out;
        } // native pause retains conventional navigation/back
        if (now < lastTime)
        {
            releaseBarrier = true;
            stickLatch = stickXLatch = true;
        }
        lastTime = now;
        if (pressed & GX_MENU)
        {
            if (opened)
                close();
            else if (!releaseBarrier)
            {
                opened = true;
                go(OPTIONS);
                stickLatch = stickXLatch = true;
                out.changed = out.consume = true;
                return out;
            }
            out.changed = true;
        }
        if (releaseBarrier)
        {
            out.consume = true;
            if (neutral)
                releaseBarrier = false;
            return out;
        }
        if (!opened)
            return out;
        out.consume = true;
        // build165 item 13 capture: the next Quest button (A and B included) selects its row, nothing else happens
        if (buttonCapture_)
        {
            if (page_ != BUTTON_SETTINGS_PAGE)
            {
                buttonCapture_ = false;
                return out;
            }
            for (unsigned s = 0; s < gx_button_map::SOURCE_COUNT; ++s)
                if (pressed & gx_button_map::sourceBit(s))
                {
                    selected_ = MAP_A + s;
                    buttonCapture_ = false;
                    out.changed = true;
                    break;
                }
            return out;
        }
        if (pressed & GX_B)
        {
            if (page_ == MENU_COLOUR)
                go(MENU_COLOURS);
            else if (page_ == MENU_COLOURS)
                go(MENU_APPEARANCE);
            else if (page_ == MENU_APPEARANCE)
                go(SETTINGS);
            else if (page_ == WRIST_ELEMENT_PROFILE)
                go(WRIST_ELEMENTS);
            else if (page_ == WRIST_ELEMENTS)
                go(WRIST_PROFILE);
            else if (page_ == WRIST_PROFILE)
                go(WRIST_CALIBRATION);
            else if (page_ == WRIST_CALIBRATION)
                go(HUD);
            else if (page_ == ANCHOR_CALIBRATION)
                go(CALIBRATION);
            else if (page_ == HAND_POSITION_L || page_ == HAND_POSITION_R)
                go(HANDS);
            else if (page_ == SECOND_HAND)
                go(WEAPON_CALIBRATION);
            else if (page_ == HANDS || page_ == HEIGHT || page_ == WEAPON_CALIBRATION ||
                     page_ == VEHICLE_CALIBRATION)
                go(CALIBRATION);
            else if (page_ == HUD_VISIBILITY)
                go(HUD);
            else if (page_ == QUICK_ACTIONS_PAGE)
                go(OPTIONS); // build165 item 16
            else if (page_ == PHYSICS_ADVANCED)
                go(PHYSICS); // build165 item 17
            else if (page_ == MODS)
                go(OPTIONS); // build166 items 5/6
            else if (page_ == PANELS)
                go(HUD); // build167 items 1/2
            else if (page_ == WRIST_BUTTONS)
                go(HUD); // build167 item 3
            else if (page_ == BODY_VIEW)
                go(SETTINGS); // build167 item 5
            else if (page_ == BUTTON_SETTINGS_PAGE)
                go(SETTINGS); // build165 item 13
            else if (page_ == PARACHUTE_SETTINGS_PAGE)
                go(MOVEMENT); // build165 item 10
            else if (page_ == VEHICLE_CAMERA_SETTINGS_PAGE || page_ == FLIGHT)
                go(VEHICLE_SETTINGS); // build165
            else if (page_ == CALIBRATION || page_ == HAPTICS || page_ == HUD || page_ == PHYSICS ||
                     page_ == VEHICLE_SETTINGS || page_ == IMAGE || page_ == MOVEMENT)
                go(SETTINGS);
            else if (page_ == SETTINGS)
                go(OPTIONS);
            else if (page_ == SNIPER_SCOPE)
                go(WEAPON_SETTINGS_PAGE);
            else if (page_ == QUALITY || page_ == GRAPHICS_NEURAL)
                go(IMAGE); // build163; 167-neural B5
            else if (page_ == WEAPON_SETTINGS_PAGE)
                go(SETTINGS);
            else if (page_ == RELOAD_SETTINGS_PAGE)
                go(WEAPON_SETTINGS_PAGE); // build149
            else if (page_ == VEHICLES)
                go(VEHICLE_CLASSES);
            else if (page_ == WEAPON_ITEMS)
                go(WEAPON_CLASSES);
            else if (page_ == WEAPON_CLASSES)
                go(WEAPONS);
            else if (page_ == MODEL_ITEMS)
                go(MODEL_CLASSES);
            else if (page_ == MODEL_CLASSES)
                go(MODEL_CHANGER);
            else if (page_ == PROTAGONISTS)
                go(protagonistsFromRoot_ ? OPTIONS : PLAYER);
            else if (page_ == MODEL_CHANGER || page_ == ABILITIES)
                go(PLAYER);
            else if (page_ != OPTIONS && page_ != TRAINER)
                go(TRAINER);
            else if (page_ == TRAINER)
                go(OPTIONS);
            else
                close();
            out.changed = true;
            return out;
        }
        // Active-hand stick (right if active). Vertical = row navigation, horizontal = value (build145). The dominant
        // axis wins so a diagonal push never both moves the row and changes a value.
        const double stick = (i.active_hands & 2) ? i.stick[1][1] : i.stick[0][1];
        const double stickX = (i.active_hands & 2) ? i.stick[1][0] : i.stick[0][0];
        if (fabs(stick) < .3)
        {
            stickLatch = false;
            repeatAt = 0;
            direction_ = 0;
        }
        const int direction = stick > 0 ? -1 : 1;
        if (fabs(stick) > .7 && fabs(stick) >= fabs(stickX) &&
            (!stickLatch || (repeatAt && now >= repeatAt) || (direction_ && direction_ != direction)))
        {
            selected_ = (selected_ + rows() + direction) % rows();
            ensureVisible();
            repeatAt = now + (stickLatch ? .10 : .4);
            stickLatch = true;
            direction_ = direction;
            out.changed = true;
        }
        if (fabs(stickX) < .3)
        {
            stickXLatch = false;
            repeatXAt = 0;
            directionX_ = 0;
        }
        const int directionX = stickX > 0 ? 1 : -1;
        if (fabs(stickX) > .7 && fabs(stickX) > fabs(stick) &&
            (!stickXLatch || (repeatXAt && now >= repeatXAt) || (directionX_ && directionX_ != directionX)))
        {
            repeatXAt = now + (stickXLatch ? .10 : .4);
            stickXLatch = true;
            directionX_ = directionX;
            if (adjust(directionX, out))
                out.changed = true;
        }
        if (!(pressed & GX_A))
            return out;
        out.changed = true;
        if (page_ == OPTIONS)
        {
            switch (selected_)
            {
            case menyoo_vr::RootTrainer:
                go(TRAINER);
                break;
            case menyoo_vr::RootGameMenu:
                close();
                out.command = PAUSE_GAME;
                break;
            case menyoo_vr::RootQuick:
                go(QUICK_ACTIONS_PAGE);
                break; // build165 item 16
            case menyoo_vr::RootSettings:
                go(SETTINGS);
                break;
            case menyoo_vr::RootMods:
                go(MODS);
                break; // build166 items 5/6
            case menyoo_vr::RootStoryCharacter:
                protagonistsFromRoot_ = true;
                go(PROTAGONISTS);
                break; // native Story selector
            case menyoo_vr::RootCharacterMenu:
                close();
                out.command = CHARACTER_MENU;
                break;
            case menyoo_vr::RootReturn:
                close();
                break;
            case menyoo_vr::RootRecenter:
                out.command = RECENTER;
                break;
            }
        }
        else if (page_ == SETTINGS)
        {
            switch (selected_)
            { // build145 order (SettingsRow)
            case S_CALIBRATION:
                go(CALIBRATION);
                break;
            case S_MOVEMENT:
                go(MOVEMENT);
                break; // build165 item 12
            case S_BODY:
                go(BODY_VIEW);
                break; // build167 item 5
            case S_BUTTONS:
                go(BUTTON_SETTINGS_PAGE);
                break; // build165 item 13
            case S_HUD:
                go(HUD);
                break;
            case S_PHYSICS:
                go(PHYSICS);
                break;
            case S_HAPTICS:
                go(HAPTICS);
                break;
            case S_WEAPONS:
                go(WEAPON_SETTINGS_PAGE);
                break;
            case S_VEHICLES:
                go(VEHICLE_SETTINGS);
                break;
            case S_IMAGE:
                go(IMAGE);
                break;
            case S_APPEARANCE:
                go(MENU_APPEARANCE);
                break;
            case S_CAMERA:
                out.command = VIEW_GAME;
                break;
            case S_BACK:
                go(OPTIONS);
                break;
            }
        }
        else if (page_ == MOVEMENT)
        { // build165 item 12: same turning behaviour as the old Settings rows
            if (selected_ == MV_TURN)
                settings.smooth = !settings.smooth;
            else if (selected_ == MV_TURN_RATE)
            {
                if (settings.smooth)
                    settings.smoothDegrees = listStep(settings.smoothDegrees, SMOOTH_VALUES, 3, 1, true);
                else
                    settings.snapDegrees = listStep(settings.snapDegrees, SNAP_VALUES, 3, 1, true);
            }
            else if (selected_ == MV_DIRECTION)
                settings.moveDirection = settings.moveDirection ? 0 : 1;
            else if (selected_ == MV_PARACHUTE)
                go(PARACHUTE_SETTINGS_PAGE);
            else if (selected_ == MV_HEAD_FADE)
                settings.headFade = !settings.headFade; // build167 item 4
            else
                go(SETTINGS);
        }
        else if (page_ == BUTTON_SETTINGS_PAGE)
        { // build165 item 13
            if (selected_ == MAP_CONTEXT)
                buttonContext_ = (buttonContext_ + 1) % gx_button_map::CONTEXT_COUNT;
            else if (selected_ >= MAP_A && selected_ <= MAP_RCLICK)
            {
                int& t = settings.buttonMap[buttonContext_][selected_ - MAP_A];
                t = (gx_button_map::sanitize(t) + 1) % gx_button_map::TARGET_COUNT;
            }
            else if (selected_ == MAP_CAPTURE)
                buttonCapture_ = true;
            else if (selected_ == MAP_COPY)
            {
                for (unsigned c = 0; c < gx_button_map::CONTEXT_COUNT; ++c)
                    if (c != buttonContext_)
                        for (unsigned k = 0; k < gx_button_map::SOURCE_COUNT; ++k)
                            settings.buttonMap[c][k] = settings.buttonMap[buttonContext_][k];
            }
            else if (selected_ == MAP_RESET)
            {
                for (unsigned k = 0; k < gx_button_map::SOURCE_COUNT; ++k)
                    settings.buttonMap[buttonContext_][k] = gx_button_map::SAME;
            }
            else
                go(SETTINGS);
        }
        else if (page_ == PARACHUTE_SETTINGS_PAGE)
        { // build165 item 10: A switches the selected option
            if (selected_ == PA_MODE)
                settings.parachuteMode = settings.parachuteMode ? 0 : 1;
            else if (selected_ == PA_FOLLOW)
                settings.parachuteFollow = !settings.parachuteFollow;
            else if (selected_ == PA_DIVE)
                settings.parachuteDive = !settings.parachuteDive;
            else if (selected_ == PA_AUTO)
                settings.parachuteAuto = !settings.parachuteAuto;
            else
                go(MOVEMENT);
        }
        else if (page_ == IMAGE)
        { // build163: Performance rows + Calidad grafica
            if (selected_ == IM_PACING)
                pacing = gx_pacing::nextPacing(pacing);
            else if (selected_ == IM_LIMITER)
                settings.nativeFrameLimiter = !settings.nativeFrameLimiter;
            else if (selected_ == IM_QUALITY)
                go(QUALITY);
            else if (selected_ == IM_NEURAL)
                go(GRAPHICS_NEURAL); // 167-neural B5
            else if (selected_ == IM_DAMAGE)
                settings.screenDamage = !settings.screenDamage; // build165 item 17
            else if (selected_ == IM_FILTERS)
                settings.screenFilters = !settings.screenFilters;
            else
                go(SETTINGS);
        }
        else if (page_ == GRAPHICS_NEURAL)
        { // 167-neural B5: A switches / steps (the intensity wraps 0..100 by 10)
            if (selected_ == GR_NEURAL)
                settings.neural = !settings.neural;
            else if (selected_ == GR_INTENSITY)
                settings.neuralIntensity =
                    settings.neuralIntensity >= 100 ? 0 : settings.neuralIntensity + 10;
            else if (selected_ == GR_COMPARE)
                settings.neuralCompare = !settings.neuralCompare;
            else if (selected_ == GR_BACK)
                go(IMAGE);
        }
        else if (page_ == VEHICLE_SETTINGS)
        { // build163
            if (selected_ == VS_STEERING)
                settings.steering = !settings.steering;
            else if (selected_ == VS_RADIO)
                settings.newRadio = !settings.newRadio;
            else if (selected_ == VS_CAMERA)
                go(VEHICLE_CAMERA_SETTINGS_PAGE);
            else if (selected_ == VS_FLIGHT)
                go(FLIGHT); // build165
            else if (selected_ == VS_CONTROL)
                settings.controlFrame = !settings.controlFrame;
            else if (selected_ == VS_HARDEN)
                settings.levelHarden = !settings.levelHarden;
            else if (selected_ == VS_STICK)
                settings.stickDrive = !settings.stickDrive;
            else
                go(SETTINGS);
        }
        else if (page_ == VEHICLE_CAMERA_SETTINGS_PAGE)
        { // build165 item 7: A cycles the selected mode
            if (selected_ < VC_BACK)
            {
                Result ignored;
                adjust(1, ignored);
            }
            else
                go(VEHICLE_SETTINGS);
        }
        else if (page_ == FLIGHT)
        { // build165 item 11: A wraps the sensitivity
            if (selected_ <= FL_HELICOPTER)
            {
                int& v = settings.flightSensitivity[selected_];
                v = v >= 200 ? 25 : v + 25;
            }
            else if (selected_ == FL_INVERT)
                settings.invertFlightPitch = !settings.invertFlightPitch;
            else
                go(VEHICLE_SETTINGS);
        }
        else if (page_ == SNIPER_SCOPE)
        { // view, eye RIGHT -> LEFT -> AUTO, panel size (wraps on Accept), help, back
            if (selected_ == SC_VIEW)
                settings.scopeView = gx_scope::nextView(settings.scopeView);
            else if (selected_ == SC_EYE)
                settings.scopeEye = nextEyeInt(settings.scopeEye);
            else if (selected_ == SC_WIDTH)
                settings.scopeWidth = gx_scope::nextWidth(settings.scopeWidth);
            else if (selected_ == SC_BACK)
                go(WEAPON_SETTINGS_PAGE);
        }
        else if (page_ == QUALITY)
        { // build141 C: only the 3 editable rows change; read-only rows ignore A
            if (selected_ == gx_quality::FXAA)
                settings.quality.fxaa = gx_quality::nextFxaa(settings.quality.fxaa);
            else if (selected_ == gx_quality::TESSELLATION)
                settings.quality.tessellation = gx_quality::nextTessellation(settings.quality.tessellation);
            else if (selected_ == gx_quality::VR_SCALE)
                settings.quality.vrScale = gx_quality::nextVrScale(settings.quality.vrScale);
            else if (selected_ == gx_quality::BACK)
                go(IMAGE);
        }
        else if (page_ == WEAPON_SETTINGS_PAGE)
        { // build149: A toggles/cycles too (Left/Right in adjust)
            if (selected_ == W_SUPPORT_GRIP)
                settings.supportGrip = settings.supportGrip ? 0 : 1;
            else if (selected_ == W_RECOIL)
                settings.recoil =
                    settings.recoil == gx_recoil::REALISTIC ? gx_recoil::REDUCED : gx_recoil::REALISTIC;
            else if (selected_ == W_GUN_STRIKE)
                settings.gunStrike = !settings.gunStrike;
            else if (selected_ == W_WORLD)
                settings.weaponWorld = !settings.weaponWorld;
            else if (selected_ == W_LEFT_REALISTIC)
                settings.leftRealistic = !settings.leftRealistic;
            else if (selected_ == W_ONE_HAND)
                settings.oneHandSupport = !settings.oneHandSupport;
            else if (selected_ == W_HELP_GLOW)
                settings.helpGlow = !settings.helpGlow;
            else if (selected_ == W_RELOAD)
                go(RELOAD_SETTINGS_PAGE);
            else if (selected_ == W_SCOPE)
                go(SNIPER_SCOPE);
            else
                go(SETTINGS);
        }
        else if (page_ == RELOAD_SETTINGS_PAGE)
        { // build149: a family without a 149 realistic reload stays Quick
            if (selected_ < gx_reload::FAMILY_COUNT)
            {
                if (gx_reload::realisticAvailable(selected_))
                    settings.reloadModes[selected_] = settings.reloadModes[selected_] == gx_reload::REALISTIC
                                                          ? gx_reload::QUICK
                                                          : gx_reload::REALISTIC;
            }
            else
                go(WEAPON_SETTINGS_PAGE);
        }
        else if (page_ == MENU_APPEARANCE)
        {
            if (selected_ == 0)
                settings.appearance.language = settings.appearance.language == menyoo_vr::English
                                                   ? menyoo_vr::Spanish
                                                   : menyoo_vr::English;
            else if (selected_ == 1)
                go(MENU_COLOURS);
            else if (selected_ == 2)
                menyoo_vr::resetAppearance(settings.appearance);
            else
                go(SETTINGS);
        }
        else if (page_ == MENU_COLOURS)
        {
            if (selected_ < menyoo_vr::ColourRoleCount)
            {
                menuColour = selected_;
                go(MENU_COLOUR);
            }
            else
                go(MENU_APPEARANCE);
        }
        else if (page_ == MENU_COLOUR)
        {
            if (selected_ == CO_RESET)
                settings.appearance.colour[menuColour] = menyoo_vr::defaultAppearance().colour[menuColour];
            else if (selected_ == CO_BACK)
                go(MENU_COLOURS); // R/G/B/Alpha: Left/Right
        }
        else if (page_ == HUD)
        {
            if (selected_ == H_WRIST)
                settings.wrist = !settings.wrist;
            else if (selected_ == H_INDICATORS)
                settings.contextIndicators = !settings.contextIndicators;
            else if (selected_ == H_VRPROMPTS)
                settings.vrPrompts = !settings.vrPrompts; // build167 item 7
            else if (selected_ == H_HIDE_AIM)
                settings.hideHudAim = !settings.hideHudAim;
            else if (selected_ == H_WRIST_CALIBRATION)
                go(WRIST_CALIBRATION);
            else if (selected_ == H_VISIBILITY)
                go(HUD_VISIBILITY);
            else if (selected_ == H_PANELS)
                go(PANELS); // build167 items 1/2
            else if (selected_ == H_BUTTONS)
                go(WRIST_BUTTONS); // build167 item 3
            else if (selected_ == H_BACK)
                go(SETTINGS); // H_MENU_SIZE163 / H_PAUSE_SIZE use Left/Right
        }
        else if (page_ == HUD_VISIBILITY)
        {
            if (selected_ < gx_ui_visibility::Count)
                gx_ui_visibility::toggle(settings.visibility, selected_);
            else if (selected_ == gx_ui_visibility::Count)
                settings.visibility.mask = gx_ui_visibility::All;
            else
                go(HUD);
        }
        else if (page_ == WRIST_CALIBRATION)
        {
            if (selected_ < 2)
            {
                wristHand = selected_;
                go(WRIST_PROFILE);
            }
            else
                go(HUD);
        }
        else if (page_ == WRIST_PROFILE)
        {
            if (selected_ == WR_RESET)
                gx_wrist::reset(settings.wrists, wristHand);
            else if (selected_ == WR_ELEMENTS)
                go(WRIST_ELEMENTS);
            else if (selected_ == WR_BACK)
                go(WRIST_CALIBRATION); // 0..6: Left/Right
        }
        else if (page_ == WRIST_ELEMENTS)
        {
            const unsigned n = wristHand ? gx_ui_element::RightCount : 1;
            if (selected_ < n)
            {
                wristElement = wristHand ? selected_ : gx_ui_element::Wanted;
                wristElementParameter = 0;
                go(WRIST_ELEMENT_PROFILE);
            }
            else if (selected_ == n)
            {
                if (wristHand)
                    gx_ui_element::resetRight(settings.elements);
                else
                    gx_ui_element::reset(settings.elements, gx_ui_element::Wanted);
            }
            else
                go(WRIST_PROFILE);
        }
        else if (page_ == WRIST_ELEMENT_PROFILE)
        {
            if (wristElement >= gx_ui_element::Count)
                return out;
            if (wristElement == gx_ui_element::Phone)
            {
                if (selected_ == 1)
                    go(WRIST_ELEMENTS);
            }
            else if (selected_ == EL_MODE)
                settings.elements.element[wristElement].individual =
                    settings.elements.element[wristElement].individual ? 0u : 1u;
            else if (selected_ == EL_RESET)
                gx_ui_element::reset(settings.elements, wristElement);
            else if (selected_ == EL_BACK)
                go(WRIST_ELEMENTS); // 1..7: Left/Right
        }
        else if (page_ == HEIGHT)
        {
            switch (selected_)
            {
            case HT_RECENTER:
                out.command = RECENTER;
                break;
            case HT_MEASURE:
                out.command = MEASURE_HEIGHT;
                break; // build165 item 18
            case HT_BACK:
                go(CALIBRATION);
                break; // HT_VALUE: Left/Right = down/up 2 cm
            }
        }
        else if (page_ == PHYSICS)
        {
            settings.physics = gx_physical_tuning::checked(settings.physics);
            if (selected_ == PH_LEGS)
                settings.softLegContactsEnabled = !settings.softLegContactsEnabled;
            else if (selected_ == PH_RESET)
                settings.physics = gx_physical_tuning::Settings();
            else if (selected_ == PH_ADVANCED)
                go(PHYSICS_ADVANCED);
            else if (selected_ == PH_BACK)
                go(SETTINGS); // 0..2: Left/Right
        }
        else if (page_ == PHYSICS_ADVANCED)
        { // build165 item 17: Left/Right changes a value row
            if (selected_ == AD_RESET)
            {
                settings.legsCrouchCm = 30;
                settings.strikeSpeedPercent = 100;
            }
            else if (selected_ == AD_BACK)
                go(PHYSICS);
        }
        else if (page_ == MODS)
        { // build166 items 5/6: Left/Right changes a value row
            if (selected_ == MO_MP_MAPS)
                settings.modsMpMaps = !settings.modsMpMaps;
            else if (selected_ == MO_RESET)
            {
                settings.modsPedPercent = settings.modsTrafficPercent = 100;
                settings.modsMpMaps = false;
            }
            else if (selected_ == MO_BACK)
                go(OPTIONS);
        }
        else if (page_ == PANELS)
        { // build167 items 1/2: actions are commands for the panel glue
            const Command cmds[PN_COUNT] = {
                PANEL_MOVE,  NONE, NONE,        PANEL_FACE,      NONE, PANEL_COPY, PANEL_PASTE,
                PANEL_MODEL, NONE, PANEL_RESET, PANEL_RESET_ALL, NONE};
            if (selected_ == PN_VEHICLE)
                settings.vehiclePanels = !settings.vehiclePanels;
            else if (selected_ == PN_BACK)
                go(HUD);
            else if (selected_ < PN_COUNT)
                out.command = cmds[selected_];
        }
        else if (page_ == BODY_VIEW)
        { // build167 item 5
            if (selected_ < gx_body_situation::COUNT)
                settings.bodyView[selected_] =
                    gx_body_situation::step(selected_, settings.bodyView[selected_], 1);
            else if (selected_ == BV_ALL)
            {
                const int m = gx_body_situation::step(gx_body_situation::FOOT,
                                                      settings.bodyView[gx_body_situation::FOOT], 1);
                for (unsigned s = 0; s < gx_body_situation::COUNT; ++s)
                    settings.bodyView[s] = gx_body_situation::sanitize(s, m);
            }
            else if (selected_ == BV_RESET)
                settings.defaultBodyView();
            else
                go(SETTINGS);
        }
        else if (page_ == WRIST_BUTTONS)
        { // build167 item 3
            if (selected_ == WB_ON)
                settings.wristButtons = !settings.wristButtons;
            else if (selected_ >= WB_SLOT1 && selected_ <= WB_SLOT6)
            {
                int& v = settings.wristSlots[selected_ - WB_SLOT1];
                v = ((v < 0 || v >= WRIST_ACTIONS ? 0 : v) + 1) % WRIST_ACTIONS;
            }
            else if (selected_ == WB_BACK)
                go(HUD);
        }
        else if (page_ == HAPTICS)
        {
            if (selected_ == HA_PERCENT)
                settings.hapticPercent = listStep(settings.hapticPercent, HAPTIC_VALUES, 3, 1, true);
            else if (selected_ == HA_DRIVING)
                settings.drivingRumble = !settings.drivingRumble;
            else
                go(SETTINGS);
        }
        else if (page_ == ANCHOR_CALIBRATION)
        {
            if (selected_ == AN_SCOPE)
            {
                if (weaponModel && weaponGroup)
                {
                    anchorFamily = !anchorFamily;
                    out.command = ANCHOR_SCOPE;
                }
            }
            // Accept on a value row = reset THAT parameter (inherit / FOLLOW RIGHT), as the old per-parameter reset did.
            else if (selected_ >= AN_FIRST_VALUE && selected_ < AN_FIRST_VALUE + 6)
            {
                if (weaponModel)
                {
                    out.command = ANCHOR_RESET;
                    out.vehicle = selected_ - AN_FIRST_VALUE;
                }
            }
            else if (selected_ == AN_RESET && weaponModel)
            {
                out.command = ANCHOR_RESET;
                out.vehicle = 6;
            }
            else if (selected_ == AN_BACK)
                go(CALIBRATION);
        }
        else if (page_ == VEHICLE_CALIBRATION)
        {
            if (selected_ == AX_BACK)
            {
                go(CALIBRATION);
                return out;
            }
            if (!vehicleModel)
                return out;
            if (selected_ == AX_RESET)
                out.command = RESET_VEHICLE_PROFILE; // X/Y/Z: Left/Right
        }
        else if (page_ == CALIBRATION)
        {
            if (selected_ == 0)
                go(HANDS);
            else if (selected_ <= 4)
            {
                anchorHand = (selected_ == 1 || selected_ == 3) ? 1 : 0;
                anchorSupport = selected_ >= 3;
                go(ANCHOR_CALIBRATION);
                out.command = ANCHOR_CONTEXT;
            }
            else if (selected_ == 5)
                go(HEIGHT);
            else if (selected_ == 6)
                go(VEHICLE_CALIBRATION);
            else
                go(SETTINGS); // build163: wrists / sizes -> HUD
        }
        else if (page_ == TRAINER)
        {
            switch (selected_)
            {
            case 0:
                go(PLAYER);
                break;
            case 1:
                go(WEAPONS);
                break;
            case 2:
                go(VEHICLE_CLASSES);
                break;
            case 3:
                go(VEHICLE_ACTIONS);
                break;
            case 4:
                go(WEATHER);
                break;
            case 5:
                go(CLOCK_PAGE);
                break;
            case 6:
                out.command = RESET_CHEATS;
                break;
            case 7:
                out.command = REFRESH_CATALOG;
                break;
            case 8:
                go(OPTIONS);
                break;
            }
        }
        else if (paged())
        {
            if (selected_ < catalogCount_)
            {
                out.vehicle = selected_;
                if (page_ == VEHICLE_CLASSES)
                {
                    category_ = out.vehicle;
                    go(VEHICLES);
                }
                else if (page_ == WEAPON_CLASSES)
                {
                    category_ = out.vehicle;
                    go(WEAPON_ITEMS);
                }
                else if (page_ == MODEL_CLASSES)
                {
                    category_ = out.vehicle;
                    go(MODEL_ITEMS);
                }
                else if (page_ == MODEL_ITEMS)
                {
                    out.command = CHANGE_PLAYER_MODEL;
                    close();
                }
                else
                    out.command = page_ == VEHICLES
                                      ? SPAWN_VEHICLE
                                      : (page_ == WEAPON_ITEMS ? GIVE_WEAPON
                                                               : (out.vehicle ? SET_WEATHER : RESET_WEATHER));
            }
        }
        else if (page_ == PLAYER)
        {
            const Command cmds[] = {HEAL,         ARMOUR,    GOD,         STAMINA,
                                    NEVER_WANTED, WANTED_UP, WANTED_DOWN, CLEAR_WANTED};
            if (selected_ < 8)
                out.command = cmds[selected_];
            else if (selected_ == 8)
                go(ABILITIES);
            else if (selected_ == 9)
            {
                protagonistsFromRoot_ = false;
                go(PROTAGONISTS);
            }
            else if (selected_ == 10)
                go(MODEL_CHANGER);
            else
                go(TRAINER);
        }
        else if (page_ == PROTAGONISTS)
        {
            if (selected_ < 3)
            {
                out.command = SWITCH_PROTAGONIST;
                out.vehicle = selected_;
                close();
            }
            else
                go(protagonistsFromRoot_ ? OPTIONS : PLAYER);
        }
        else if (page_ == MODEL_CHANGER)
        {
            if (selected_ == 0)
                go(MODEL_CLASSES);
            else if (selected_ == 1)
            {
                out.command = RESTORE_STORY_MODEL;
                close();
            }
            else
                go(PLAYER);
        }
        else if (page_ == WEAPONS)
        {
            const Command cmds[] = {GIVE_ALL,       AMMO,      INFINITE_AMMO, INFINITE_CLIP,
                                    EXPLOSIVE_AMMO, FIRE_AMMO, PARACHUTE};
            if (selected_ == 0)
                go(WEAPON_CLASSES);
            else if (selected_ < 8)
                out.command = cmds[selected_ - 1];
            else
                go(TRAINER);
        }
        else if (page_ == ABILITIES)
        {
            const Command cmds[] = {SUPER_JUMP, SUPER_STRENGTH, EXPLOSIVE_MELEE, FAST_RUN,
                                    FAST_SWIM,  SPECIAL_FILL,   SPECIAL_INFINITE};
            if (selected_ < 7)
                out.command = cmds[selected_];
            else
                go(PLAYER);
        }
        else if (page_ == HANDS)
        {
            if (selected_ == HN_MODE)
                settings.swapHandMode();
            else if (selected_ == HN_RESET)
                settings.handPitch[0] = settings.handPitch[1] = 0;
            else if (selected_ == HN_POSITION_L)
                go(HAND_POSITION_L);
            else if (selected_ == HN_POSITION_R)
                go(HAND_POSITION_R);
            else if (selected_ == HN_BACK)
                go(CALIBRATION); // pitch rows: Left/Right
        }
        else if (page_ == HAND_POSITION_L || page_ == HAND_POSITION_R)
        {
            const unsigned h = page_ == HAND_POSITION_L ? 0 : 1;
            if (selected_ == AX_RESET)
            {
                for (unsigned c = 0; c < 3; ++c)
                    settings.handMm[h][c] = 0;
            }
            else if (selected_ == AX_BACK)
                go(HANDS); // X/Y/Z: Left/Right
        }
        else if (page_ == CLOCK_PAGE)
        {
            if (selected_ < 4)
            {
                out.command = SET_TIME;
                out.vehicle = selected_;
            }
            else if (selected_ == 4)
                out.command = TIME_UP;
            else if (selected_ == 5)
                out.command = TIME_DOWN;
            else if (selected_ == 6)
                out.command = TIME_FREEZE;
            else
                go(TRAINER);
        }
        else if (page_ == QUICK_ACTIONS_PAGE)
        { // build165 item 16: the same commands as the trainer / height / recenter rows
            const Command cmds[QK_BACK] = {
                RECENTER,      HEIGHT_UP,    HEIGHT_DOWN, SET_TIME,  TIME_UP, RESET_WEATHER, REPAIR_VEHICLE,
                CLEAN_VEHICLE, CLEAR_WANTED, WANTED_DOWN, WANTED_UP, HEAL,    ARMOUR};
            if (selected_ < QK_BACK)
            {
                out.command = cmds[selected_];
                if (selected_ == QK_NOON)
                    out.vehicle = 1;
            } // SET_TIME 1 = 12:00 (clock page)
            else
                go(OPTIONS);
        }
        else if (page_ == VEHICLE_ACTIONS)
        {
            const Command cmds[] = {REPAIR_VEHICLE,      CLEAN_VEHICLE,  VEHICLE_GOD,
                                    VEHICLE_AUTO_REPAIR, VEHICLE_UNLOCK, ENGINE_START};
            if (selected_ < 6)
                out.command = cmds[selected_];
            else
                go(TRAINER);
        }
        return out;
    }
};
inline double turn(const Settings& s, double x, double dt, double snap, bool eligible)
{
    if (!eligible || !gx_loco::finite(x) || !gx_loco::finite(dt) || dt <= 0 || dt > .1)
        return 0;
    const double pi = 3.14159265358979323846;
    if (!s.smooth)
        return snap * (s.snapDegrees / 30.0);
    if (fabs(x) < .25)
        return 0;
    return -gx_loco::clamp(x, -1, 1) * s.smoothDegrees * pi / 180 * dt;
}
}
