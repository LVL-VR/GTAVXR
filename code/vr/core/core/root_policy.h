#pragma once
#include "../ui/appearance.h"
namespace menyoo_vr
{
enum RootRow
{
    RootTrainer,
    RootGameMenu,
    RootQuick,
    RootSettings,
    RootMods,
    RootStoryCharacter,
    RootCharacterMenu,
    RootReturn,
    RootRecenter,
    RootRows
};
inline const char* rootLabel(unsigned row, unsigned language)
{
    static const char* const english[RootRows] = {"Trainer  >",     "Game Menu",      "Quick actions  >",
                                                  "Settings  >",    "Mods  >",        "Story Character  >",
                                                  "Character Menu", "Return to Game", "Recenter"};
    static const char* const spanish[RootRows] = {
        "Trainer  >", "Men\xc3\xba del juego", "Acciones r\xc3\xa1pidas  >", "Ajustes  >",
        "Mods  >",    "Personaje Story  >",    "Men\xc3\xba de personaje",   "Volver al juego",
        "Recentrar"};
    // "Quick actions" and "Story Character" have no Portuguese text yet: English.
    static const char* const portuguese[RootRows] = {
        "Trainer  >", "Menu do jogo",       "Quick actions  >", "Ajustes  >",
        "Mods  >",    "Story Character  >", "Menu do personagem", "Voltar ao jogo",
        "Recentralizar"};
    return row < RootRows ? (language == Spanish ? spanish[row] : (language == Portuguese ? portuguese[row] : english[row])) : "";
}
}
