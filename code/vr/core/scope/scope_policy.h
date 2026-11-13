#pragma once
// User's exclusions are section/origin specific. This policy never filters
// shared entity helpers merely because their original file lives in Spooner.
namespace menyoo_vr
{
enum Origin
{
    Player,
    Vehicle,
    Teleport,
    Weapons,
    Weather,
    Time,
    Bodyguards,
    Settings,
    Root
};
enum Section
{
    General,
    PlayersTab,
    SpoonerTab,
    MiscTab,
    BasicStats,
    ModelChanger,
    Wardrobe,
    PlayerAnimations,
    PlayerScenarios,
    PlayerMoods,
    PlayerMovementStyles,
    PlayerSpeech,
    PlayerVoice,
    PlayerPedFlags,
    PlayerFx,
    PlayerBreathe,
    PlayerGhostRider,
    SettingsRoot,
    ThemeEditor,
    ColourEditor,
    BasicStatsCharacters,
    CreditsTab,
    BodyguardsTab,
    WaterHackTab
};
enum Feature
{
    Ordinary,
    TotalCash,
    MaxAllStats,
    InputModel,
    FrontView,
    HeadFeatures,
    SaveOutfit,
    LoadOutfit,
    DefaultOutfitBeta,
    FlyManual,
    BurnMode,
    EntityCollision,
    EntityOpacity,
    HeightElongation,
    MovementSpeed,
    NoiseLevel,
    SweatLevel,
    Speedometers,
    PersonalVehicleOptions,
    TowMode,
    VehicleJump,
    Hydraulics,
    Digiscanner,
    Briefcase,
    RopeGun,
    MagnetGun,
    ExtinguisherFlamethrower,
    ForgeGun,
    GravityGun,
    LightGun,
    TriggerFxGun,
    KaboomGun,
    BulletGun,
    PedGun,
    ObjectGun,
    VehicleGun,
    ObjectAndVehicleGun,
    SystemClock,
    SyncSystemClock,
    Language,
    Theme,
    ColourSelector,
    ResetToggles,
    ResetAllCheats,
    StoryCashCharacter,
    StoryModeSeparator,
    NumericEditorStep,
    WaterHack
};
enum Flag
{
    KeyboardInput = 1u,
    SavedOrRemembered = 2u,
    Alpha = 4u
};
enum Decision
{
    Keep,
    ExcludeSection,
    ExcludeFeature,
    ExcludeKeyboard,
    ExcludeSaved,
    ExcludeAlpha
};
struct Item
{
    Origin origin;
    Section section;
    Feature feature;
    unsigned flags;
};
inline Decision decide(const Item& i)
{
    //2026-10-11: entire Bodyguards UI removed, including shared child selectors.
    // Preserve enum identities and reusable cloning/entity helpers, not the page.
    if (i.origin == Bodyguards || i.section == BodyguardsTab || i.section == PlayersTab ||
        i.section == SpoonerTab || i.section == MiscTab || i.section == CreditsTab)
        return ExcludeSection;
    // Water Hack explicitly discarded; other Weather controls remain retained.
    if (i.section == WaterHackTab)
        return ExcludeSection;
    if (i.feature == WaterHack)
        return ExcludeFeature;
    // Latest explicit user boundary applies to BOTH menus, regardless of origin.
    // Numeric steppers are not keyboard/text-entry actions and remain retained.
    if (i.flags & KeyboardInput)
        return ExcludeKeyboard;
    // Origin travels through shared selectors; don't delete Model Changer
    // favourites globally to enforce Vehicle/Bodyguard exclusions.
    if (i.origin == Vehicle || i.origin == Teleport || i.origin == Weather || i.origin == Time ||
        i.origin == Bodyguards)
    {
        if (i.flags & SavedOrRemembered)
            return ExcludeSaved;
        if (i.origin != Vehicle && (i.flags & Alpha))
            return ExcludeAlpha;
    }
    if (i.origin == Player)
    {
        if (i.section >= PlayerAnimations && i.section <= PlayerGhostRider)
            return ExcludeSection;
        if (i.section == BasicStatsCharacters)
            return i.feature == StoryCashCharacter || i.feature == StoryModeSeparator ||
                           i.feature == TotalCash
                       ? Keep
                       : ExcludeFeature;
        if (i.section == BasicStats)
            return i.feature == TotalCash || i.feature == NumericEditorStep ? Keep : ExcludeFeature;
        if (i.section == ModelChanger && i.feature == InputModel)
            return ExcludeFeature;
        if (i.section == Wardrobe &&
            (i.feature == FrontView || i.feature == HeadFeatures || i.feature == SaveOutfit ||
             i.feature == LoadOutfit || i.feature == DefaultOutfitBeta))
            return ExcludeFeature;
        if (i.feature == FlyManual || i.feature == BurnMode || i.feature == EntityCollision ||
            i.feature == EntityOpacity || i.feature == HeightElongation || i.feature == MovementSpeed ||
            i.feature == NoiseLevel || i.feature == SweatLevel)
            return ExcludeFeature;
    }
    if (i.origin == Vehicle &&
        (i.feature == Speedometers || i.feature == PersonalVehicleOptions || i.feature == EntityOpacity ||
         i.feature == TowMode || i.feature == VehicleJump || i.feature == Hydraulics ||
         i.feature == EntityCollision))
        return ExcludeFeature;
    if (i.origin == Weapons && i.feature >= Digiscanner && i.feature <= ObjectAndVehicleGun)
        return ExcludeFeature;
    if (i.origin == Time && (i.feature == SystemClock || i.feature == SyncSystemClock))
        return ExcludeFeature;
    if (i.origin == Settings && i.section == SettingsRoot)
    {
        return i.feature == Language || i.feature == Theme || i.feature == ColourSelector ||
                       i.feature == ResetToggles || i.feature == ResetAllCheats
                   ? Keep
                   : ExcludeFeature;
    }
    return Keep;
}
// Cloning is a separate retained action/page, NOT the Bodyguards origin.
// MaxAllStats is retained outside BasicStats by explicit user clarification.
}
