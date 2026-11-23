#include "../src/i18n.h"
#include "../src/app_settings.h"
#include <cassert>
#include <cstdio>
int main(){
 assert(AppSettings{}.language==L"system");assert(i18n::Languages().size()==14);
 assert(i18n::Resolve(L"system",L"zh-TW")==L"zh-CN");assert(i18n::Resolve(L"system",L"pt-BR")==L"pt");assert(i18n::Resolve(L"system",L"ru-RU")==L"ru");assert(i18n::Resolve(L"system",L"nl-NL")==L"en");assert(i18n::Resolve(L"invalid")==L"en");
 for(auto& language:i18n::Languages()){if(std::wstring(language.code)==L"system")continue;i18n::Configure(language.code);assert(i18n::Code()==language.code);assert(!i18n::T(L"Настройки").empty());assert(i18n::T(L"No devices with battery info found").size()>0);assert(i18n::Detail(L"Погода: Облачно · Open-Meteo").find(L"Open-Meteo")!=std::wstring::npos);if(std::wstring(language.code)!=L"ru"){assert(i18n::T(L"Настройки")!=L"Настройки");assert(i18n::Detail(L"Погода: Облачно · Open-Meteo").find(L"Облачно")==std::wstring::npos);}assert(i18n::T(L"user-city-api-key") == L"user-city-api-key");assert(i18n::RTL()==(std::wstring(language.code)==L"ar"));}
 i18n::Configure(L"ru");assert(i18n::T(L"unknown")==L"неизвестно");i18n::Configure(L"en");assert(i18n::T(L"Настройки")==L"Settings");
 puts("13 embedded locales, system mapping, fallback, RTL and composed status translations passed.");
}
