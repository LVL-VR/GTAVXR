#pragma once
#include <stdint.h>
namespace menyoo_vr {
// Explicit SOURCE HOST operations, NOT GTA registry/retail native hashes.
// Dispatch before the registry allowlist; never look these up as scrCmds.
// Read through fwTimer's typed Script channel, not HMD/UI/ability clocks.
static const uint64_t SourceHostReadScriptTimeScale=UINT64_C(0xfffffffffffff001);
enum WeatherProperty { WeatherGravity,WeatherWindOverride,WeatherWindMaximum,
 WeatherOceanStrength,WeatherRainOverride,WeatherTypeOverride,WeatherCurrentType,
 WeatherTerrainSnow,WeatherScriptCloud,WeatherWindCurrent,WeatherPropertyCount };
static const uint64_t SourceHostReadWeather=UINT64_C(0xfffffffffffff100);
static const uint64_t SourceHostWriteWeather=UINT64_C(0xfffffffffffff120);
static const uint64_t SourceHostResolveWeather=UINT64_C(0xfffffffffffff140);
static const uint64_t SourceHostResolveCloud=UINT64_C(0xfffffffffffff141);
inline bool sourceHostOperation(uint64_t op){return op==SourceHostReadScriptTimeScale||
 (op>=SourceHostReadWeather&&op<SourceHostReadWeather+WeatherPropertyCount)||
 (op>=SourceHostWriteWeather&&op<SourceHostWriteWeather+WeatherPropertyCount)||
 op==SourceHostResolveWeather||op==SourceHostResolveCloud;}
}
