#pragma once
#include "app_settings.h"
#include <string>
#include <cstdint>
#include <vector>

namespace environment {
struct Reading {
    bool valid=false, stale=false;
    int value=0, icon=0;
    std::wstring detail, observed;
};
struct Snapshot { Reading weather, air; };
struct Place {std::wstring name,detail;double latitude=0,longitude=0;};
bool ParseCities(const std::string&,std::vector<Place>&);
bool SearchCities(const std::wstring&,std::vector<Place>&,std::wstring& error);
// Parsing is independent of network and credentials so recorded responses can be tested safely.
bool ParseAir(const std::string&,Reading&);
bool ParseWeather(const std::string&,Reading&);
bool ParseOpenMeteo(const std::string&,Reading&);
int AirIcon(int aqi);
class Client {
    Snapshot data;
    uint64_t airDue=0,weatherDue=0;
    std::wstring airIdentity,weatherIdentity,location;
public:
    Snapshot Poll(const AppSettings&);
};
}
