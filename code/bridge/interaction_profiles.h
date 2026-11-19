#pragma once
 
// Suggested bindings for OpenXR controller profiles. Each profile is negotiated independently.
// The runtime owns the System button.
//   valve/index: A/B on both hands as X/Y and A/B, the left trackpad press opens the menu (no menu button).
//   htc/vive:    trackpad = stick; left trackpad click = stick click, right trackpad click = A, right menu = B, left menu = menu.
//   khr/simple:  select = trigger, left menu = menu, right menu = B.
// PSVR2 Sense arrives through SteamVR as oculus/touch (sony/playstation_vr2 is not offered to apps):
//   Square/Triangle = X/Y, Cross/Circle = A/B. Left Create is dropped by SteamVR's Touch->Sense remap,
//   so TOUCH_SECONDARY (left Triangle capacitive touch) is suggested for the Triangle-touch + R3 menu fallback.
//   DPAD_SHIFT is left thumbrest touch (UEVR LEFT_TOUCH); on Sense, assign Triangle touch to it in SteamVR.
namespace gx_profiles {
enum Slot { STICK, TRIGGER, GRIP, CLICK, PRIMARY, SECONDARY, MENU, GRIP_POSE, AIM_POSE, HAPTIC, TOUCH_SECONDARY, DPAD_SHIFT, SLOT_COUNT };
struct Profile { const char* path; const char* hand[2][SLOT_COUNT]; };   // relative to /user/hand/<left|right>/, "" = unbound
static const Profile kProfiles[] = {
    {"/interaction_profiles/oculus/touch_controller",
     {{"input/thumbstick", "input/trigger/value", "input/squeeze/value", "input/thumbstick/click", "input/x/click", "input/y/click", "input/menu/click",
       "input/grip/pose", "input/aim/pose", "output/haptic", "input/y/touch", "input/thumbrest/touch"},
      {"input/thumbstick", "input/trigger/value", "input/squeeze/value", "input/thumbstick/click", "input/a/click", "input/b/click", "",
       "input/grip/pose", "input/aim/pose", "output/haptic", "", ""}}},
    {"/interaction_profiles/valve/index_controller",
     {{"input/thumbstick", "input/trigger/value", "input/squeeze/value", "input/thumbstick/click", "input/a/click", "input/b/click", "input/trackpad/force",
       "input/grip/pose", "input/aim/pose", "output/haptic", "", ""},
      {"input/thumbstick", "input/trigger/value", "input/squeeze/value", "input/thumbstick/click", "input/a/click", "input/b/click", "",
       "input/grip/pose", "input/aim/pose", "output/haptic", "", ""}}},
    {"/interaction_profiles/htc/vive_controller",
     {{"input/trackpad", "input/trigger/value", "input/squeeze/click", "input/trackpad/click", "", "", "input/menu/click",
       "input/grip/pose", "input/aim/pose", "output/haptic", "", ""},
      {"input/trackpad", "input/trigger/value", "input/squeeze/click", "", "input/trackpad/click", "input/menu/click", "",
       "input/grip/pose", "input/aim/pose", "output/haptic", "", ""}}},
    {"/interaction_profiles/khr/simple_controller",
     {{"", "input/select/click", "", "", "", "", "input/menu/click", "input/grip/pose", "input/aim/pose", "output/haptic", "", ""},
      {"", "input/select/click", "", "", "", "input/menu/click", "", "input/grip/pose", "input/aim/pose", "output/haptic", "", ""}}},
};
static const unsigned kProfileCount = sizeof(kProfiles) / sizeof(kProfiles[0]);
}
