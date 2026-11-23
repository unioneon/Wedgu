#pragma once
#include <windows.h>
#include <string>

struct AppSettings {
    std::wstring language=L"system";
    int batteryMinutes=1;
    int weatherSource=0; // 0 = Open-Meteo, 1 = optional AccuWeather
    bool showBattery=true,showWeather=true,showAir=true;
    bool showDate=false;
    bool locationFromSystem=false;
    bool lockscreenEnabled=true;
    int lockscreenScale=55;
    COLORREF taskbarColor=CLR_INVALID,lockscreenColor=CLR_INVALID;
    std::wstring city=L"Ташкент";
    double latitude=41.3111,longitude=69.2797;
    std::wstring accuweatherKey,iqairKey;
    std::wstring lockscreenBackground;
};
namespace settings {
AppSettings Load();
bool Save(const AppSettings& s,std::wstring& error);
// An immediate colour change must not save unrelated, unfinished form edits.
bool SaveColors(COLORREF selected,std::wstring& error);
bool SaveLanguage(const std::wstring&,std::wstring& error);
bool SaveDate(bool enabled,std::wstring& error);
bool Validate(const AppSettings& s,std::wstring& error);
std::wstring DataDirectory();
}
