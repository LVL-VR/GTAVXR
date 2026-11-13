#pragma once
namespace menyoo_vr {
struct WeatherName {const char*label;const char*internal;};
static const WeatherName WeatherCatalogue[]={
{"ExtraSunny","ExtraSunny"},
{"Clear","Clear"},
{"Clouds","Clouds"},
{"Smog","Smog"},
{"Foggy","Foggy"},
{"Overcast","Overcast"},
{"Rain","Rain"},
{"Thunder","Thunder"},
{"Clearing","Clearing"},
{"Neutral","Neutral"},
{"Snow","Snow"},
{"Blizzard","Blizzard"},
{"SnowLight","SnowLight"},
{"Halloween","Halloween"},
{"Halloween Snow","SNOW_HALLOWEEN"},
{"Halloween Rain","RAIN_HALLOWEEN"}
};
static const char*CloudCatalogue[]={"Altostratus","Cirrocumulus","Cirrus","Clear 01","Cloudy 01","Contrails","Horizonband1","Horizonband2","Horizonband3","Horsey","Nimbus","NoClouds","Puffs","Rain","Shower","Snowy 01","Stormy 01","Stratoscumulus","Stripey","Wispy"};
struct GravityName {float value;const char*label;};
static const GravityName GravityCatalogue[]={{0.0f,"Zero 0.0"},{0.6f,"Pluto 0.6"},{1.6f,"Earth's Moon 1.6"},{3.7f,"Mercury/Mars 3.7"},{8.9f,"Venus/Uranus 8.9"},{9.8f,"Earth 9.8"},{10.4f,"Saturn 10.4"},{11.2f,"Neptune 11.2"},{24.9f,"Jupiter 24.9"},{274.0f,"Sun 274.0"}};
static const unsigned WeatherCatalogueCount=sizeof(WeatherCatalogue)/sizeof(WeatherCatalogue[0]);
static const unsigned CloudCatalogueCount=sizeof(CloudCatalogue)/sizeof(CloudCatalogue[0]);
static const unsigned GravityCatalogueCount=sizeof(GravityCatalogue)/sizeof(GravityCatalogue[0]);
}
