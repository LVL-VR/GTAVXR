#pragma once
 
// o "Quick" (actual)." + "Empezaremos con la recarga fisica unicamente para pistolas en esta 149 y luego en 150 ampliaremos a
// todas las familias"). One switch per family; Quick = today's exact reload. In 149 only PISTOL can be Realista; the other
// switches are shown and stay Quick until build150 implements them. Pure: shared by the VR menu and the CPU controls.
namespace gx_reload
{
enum Family
{
    PISTOL,
    REVOLVER,
    SMG,
    RIFLE,
    SHOTGUN,
    SNIPER,
    MG,
    LAUNCHER,
    FAMILY_COUNT
};
enum Mode
{
    QUICK = 0,
    REALISTIC = 1
};
// build149 pistols; build151 P3 pump shotguns; build152 ("APRUEBO EL PLAN 152"): every pistol, revolver and shotgun mechanism;
// build153 ("tambien quiero ampliar a subfusiles y fusiles"): every SMG and rifle (magazine + charging handle);
 
inline bool realisticAvailable(unsigned family)
{
    return family < FAMILY_COUNT;
}
inline int sanitize(unsigned family, int mode)
{
    return mode == REALISTIC && realisticAvailable(family) ? REALISTIC : QUICK;
}
inline const char* key(unsigned family)
{
    static const char* keys[FAMILY_COUNT] = {"reload_pistol149", "reload_revolver149", "reload_smg149",
                                             "reload_rifle149",  "reload_shotgun149",  "reload_sniper149",
                                             "reload_mg149",     "reload_launcher149"};
    return family < FAMILY_COUNT ? keys[family] : "";
}
 
// = no magazine family (stun gun, flare gun, up-n-atomizer, ...): always Quick (native).
inline unsigned familyOf(unsigned groupHash, unsigned weaponHash)
{
    switch (weaponHash)
    {
    case 0xC1B3C3D1u:
    case 0xCB96392Fu:
    case 0x97EA20B8u:
    case 0x917F6C8Cu:
        return REVOLVER; // revolvers (Revolver, Mk II, Double Action, Navy)
    case 0xDC4DB296u:
    case 0x47757124u:
        return PISTOL; // build152: Marksman, flare gun = break-action pistols
    case 0x3656C8C1u:
    case 0x45CD9CF3u:
    case 0xAF3696A1u:
        return FAMILY_COUNT; // stun guns, ray pistol: they recharge, no reload exists
    // build161 E (worn 160: the Perico pistol "no me deja recargarla bajo ningun movimiento ... ni siquiera manteniendo el boton de
     
    case 0x57A4368Cu:
        return FAMILY_COUNT;
    default:
        break;
    }
    switch (groupHash)
    {
    case 0x18D5FA97u:
        return PISTOL;
    case 0xC6E9A5C5u:
        return SMG;
    case 0x39D5C192u:
        return RIFLE;
    case 0x33431399u:
        return SHOTGUN;
    case 0xB7BBD827u:
        return SNIPER;
    case 0x451B04BCu:
        return MG;
    case 0xA27A4F9Fu:
        return LAUNCHER;
    default:
        return FAMILY_COUNT;
    }
}
inline const char* name(unsigned family, bool spanish)
{
    static const char* es[FAMILY_COUNT] = {"Pistolas",  "Rev\303\263lveres", "Subfusiles",     "Rifles",
                                           "Escopetas", "Francotirador",     "Ametralladoras", "Lanzadores"};
    static const char* en[FAMILY_COUNT] = {"Pistols",  "Revolvers",     "SMGs",         "Rifles",
                                           "Shotguns", "Sniper rifles", "Machine guns", "Launchers"};
    return family < FAMILY_COUNT ? (spanish ? es[family] : en[family]) : "";
}
}
 
namespace gx_recoil
{
enum Mode
{
    REDUCED = 0,
    REALISTIC = 1
};
inline int sanitize(int m)
{
    return m == REALISTIC ? REALISTIC : REDUCED;
}
}
