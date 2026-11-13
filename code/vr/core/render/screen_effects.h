#pragma once
 
 
 
namespace GTAVXRNativePair
{
bool ScreenDamageFlash();                   // false = the red damage-direction overlay is not registered
bool ScreenScriptFilter(const char* name);  // false = this script full-screen post-FX is not started
void ScreenTimecycleSeen(const char* name); // measurement only (script timecycle modifier names for 166)
}
