#include "app_settings.h"
#include "i18n.h"
#include <dpapi.h>
#include <shlobj.h>
#include <cmath>
#include <vector>

namespace {
constexpr const wchar_t* key=L"Software\\WirelessBattery";
DWORD Number(const wchar_t* n,DWORD fallback){DWORD v=fallback,sz=sizeof(v);RegGetValueW(HKEY_CURRENT_USER,key,n,RRF_RT_REG_DWORD,nullptr,&v,&sz);return v;}
std::wstring Text(const wchar_t* n,const wchar_t* fallback=L""){DWORD sz=0;if(RegGetValueW(HKEY_CURRENT_USER,key,n,RRF_RT_REG_SZ,nullptr,nullptr,&sz)!=ERROR_SUCCESS)return fallback;std::vector<wchar_t>b(sz/2+1,0);if(RegGetValueW(HKEY_CURRENT_USER,key,n,RRF_RT_REG_SZ,nullptr,b.data(),&sz)!=ERROR_SUCCESS)return fallback;return b.data();}
std::wstring Secret(const wchar_t* n){DWORD sz=0;if(RegGetValueW(HKEY_CURRENT_USER,key,n,RRF_RT_REG_BINARY,nullptr,nullptr,&sz)!=ERROR_SUCCESS||sz>65536)return L"";std::vector<BYTE>b(sz);if(RegGetValueW(HKEY_CURRENT_USER,key,n,RRF_RT_REG_BINARY,nullptr,b.data(),&sz)!=ERROR_SUCCESS)return L"";DATA_BLOB in{sz,b.data()},out{};std::wstring value;if(CryptUnprotectData(&in,nullptr,nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&out)){if(out.cbData%2==0)value.assign((wchar_t*)out.pbData,out.cbData/2);SecureZeroMemory(out.pbData,out.cbData);LocalFree(out.pbData);}return value;}
bool PutNumber(const wchar_t*n,DWORD v){return RegSetKeyValueW(HKEY_CURRENT_USER,key,n,REG_DWORD,&v,sizeof(v))==ERROR_SUCCESS;}
bool PutText(const wchar_t*n,const std::wstring&v){return RegSetKeyValueW(HKEY_CURRENT_USER,key,n,REG_SZ,v.c_str(),(DWORD)((v.size()+1)*2))==ERROR_SUCCESS;}
bool Protect(const std::wstring& value,std::vector<BYTE>& bytes){bytes.clear();if(value.empty())return true;DATA_BLOB in{(DWORD)(value.size()*2),(BYTE*)value.data()},out{};if(!CryptProtectData(&in,L"Wireless Battery provider key",nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&out))return false;bytes.assign(out.pbData,out.pbData+out.cbData);LocalFree(out.pbData);return true;}
}
namespace settings {
std::wstring DataDirectory(){PWSTR path=nullptr;std::wstring p;if(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&path))){p=path;CoTaskMemFree(path);}if(p.empty())return L"";p+=L"\\WirelessBattery";CreateDirectoryW(p.c_str(),nullptr);return p;}
bool Validate(const AppSettings&s,std::wstring&error){
    if(s.weatherSource<0||s.weatherSource>1){error=L"Выберите источник погоды.";return false;}
    if(s.batteryMinutes<1||s.batteryMinutes>60){error=L"Интервал батареи должен быть от 1 до 60 минут.";return false;}
    if(s.lockscreenScale<50||s.lockscreenScale>200){error=L"Размер должен быть от 50 до 200%.";return false;}
    if(s.city.empty()||s.city.size()>100||!std::isfinite(s.latitude)||!std::isfinite(s.longitude)||s.latitude< -90||s.latitude>90||s.longitude< -180||s.longitude>180){error=L"Выберите город или определите местоположение через Windows.";return false;}
    for(const auto* value:{&s.accuweatherKey,&s.iqairKey}){if(value->size()>512||value->find_first_of(L"\r\n\t ")!=std::wstring::npos){error=L"В API-ключе есть пробелы или недопустимые символы.";return false;}}
    return true;
}
AppSettings Load(){AppSettings s;s.language=Text(L"Language",L"system");if(!i18n::Supported(s.language))s.language=L"system";s.weatherSource=(int)Number(L"WeatherSource",0);if(s.weatherSource>1)s.weatherSource=0;s.batteryMinutes=(int)Number(L"BatteryMinutes",1);s.showDate=Number(L"ShowDate",0)!=0;s.locationFromSystem=Number(L"LocationFromSystem",0)!=0;s.showBattery=Number(L"ShowBattery",1)!=0;s.showWeather=Number(L"ShowWeather",1)!=0;s.showAir=Number(L"ShowAir",1)!=0;s.lockscreenEnabled=Number(L"LockscreenEnabled",1)!=0;s.lockscreenScale=(int)Number(L"LockscreenScale",55);s.taskbarColor=Number(L"TaskbarColor",CLR_INVALID);s.lockscreenColor=s.taskbarColor;s.city=Text(L"City",L"Ташкент");s.latitude=wcstod(Text(L"Latitude",L"41.3111").c_str(),nullptr);s.longitude=wcstod(Text(L"Longitude",L"69.2797").c_str(),nullptr);s.accuweatherKey=Secret(L"AccuWeatherKeyProtected");s.iqairKey=Secret(L"IQAirKeyProtected");s.lockscreenBackground=Text(L"LockscreenBackground");std::wstring error;if(!Validate(s,error)){s.batteryMinutes=1;s.lockscreenScale=55;s.city=L"Ташкент";s.latitude=41.3111;s.longitude=69.2797;}return s;}
bool SaveLanguage(const std::wstring& language,std::wstring&error){if(!i18n::Supported(language)||!PutText(L"Language",language)){error=L"Не удалось сохранить язык.";return false;}return true;}
bool SaveDate(bool enabled,std::wstring&error){if(!PutNumber(L"ShowDate",enabled?1:0)){error=L"Не удалось сохранить отображение даты.";return false;}return true;}
bool SaveColors(COLORREF selected,std::wstring&error){if(selected!=CLR_INVALID&&(selected&0xFF000000u)){error=L"Недопустимый цвет.";return false;}if(!PutNumber(L"TaskbarColor",selected)||!PutNumber(L"LockscreenColor",selected)){error=L"Не удалось сохранить цвет.";return false;}return true;}
bool Save(const AppSettings&s,std::wstring&error){if(!Validate(s,error))return false;std::vector<BYTE>a,b;if(!Protect(s.accuweatherKey,a)||!Protect(s.iqairKey,b)){error=L"Не удалось защитить API-ключи в Windows.";return false;}bool ok=true;auto n=[&](const wchar_t*k,DWORD v){ok=PutNumber(k,v)&&ok;};auto t=[&](const wchar_t*k,const std::wstring&v){ok=PutText(k,v)&&ok;};t(L"Language",s.language);n(L"WeatherSource",s.weatherSource);n(L"BatteryMinutes",s.batteryMinutes);n(L"ShowDate",s.showDate);n(L"LocationFromSystem",s.locationFromSystem);n(L"ShowBattery",s.showBattery);n(L"ShowWeather",s.showWeather);n(L"ShowAir",s.showAir);n(L"LockscreenEnabled",s.lockscreenEnabled);n(L"LockscreenScale",s.lockscreenScale);n(L"TaskbarColor",s.taskbarColor);n(L"LockscreenColor",s.taskbarColor);t(L"City",s.city);t(L"Latitude",std::to_wstring(s.latitude));t(L"Longitude",std::to_wstring(s.longitude));t(L"LockscreenBackground",s.lockscreenBackground);ok=RegSetKeyValueW(HKEY_CURRENT_USER,key,L"AccuWeatherKeyProtected",REG_BINARY,a.data(),(DWORD)a.size())==ERROR_SUCCESS&&ok;ok=RegSetKeyValueW(HKEY_CURRENT_USER,key,L"IQAirKeyProtected",REG_BINARY,b.data(),(DWORD)b.size())==ERROR_SUCCESS&&ok;if(!ok)error=L"Не удалось сохранить настройки.";return ok;}
}
