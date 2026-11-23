#include "environment.h"
#include "i18n.h"
#include "../vendor/json.hpp"
#include <winhttp.h>
#include <cmath>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <locale>

namespace {
using nlohmann::json;
std::wstring Wide(const std::string& s){if(s.empty())return {};int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),(int)s.size(),nullptr,0);if(!n)return {};std::wstring v(n,0);MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),v.data(),n);return v;}
struct Handle {HINTERNET p;~Handle(){if(p)WinHttpCloseHandle(p);}};
struct Response {DWORD status=0;std::string body;};
Response Get(const wchar_t* host,const std::wstring& path,const std::wstring& bearer=L"") {
    Response r;Handle session{WinHttpOpen(L"WirelessBattery/1.1-preview",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0)};
    if(!session.p)return r;
    WinHttpSetTimeouts(session.p,4000,4000,6000,6000);
    Handle connection{WinHttpConnect(session.p,host,INTERNET_DEFAULT_HTTPS_PORT,0)};if(!connection.p)return r;
    Handle request{WinHttpOpenRequest(connection.p,L"GET",path.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE)};if(!request.p)return r;
    DWORD redirect=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;WinHttpSetOption(request.p,WINHTTP_OPTION_REDIRECT_POLICY,&redirect,sizeof(redirect));
    std::wstring headers=L"Accept: application/json\r\n";if(!bearer.empty())headers+=L"Authorization: Bearer "+bearer+L"\r\n";
    if(!WinHttpSendRequest(request.p,headers.c_str(),(DWORD)headers.size(),WINHTTP_NO_REQUEST_DATA,0,0,0)||!WinHttpReceiveResponse(request.p,nullptr))return r;
    DWORD size=sizeof(r.status);WinHttpQueryHeaders(request.p,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,nullptr,&r.status,&size,nullptr);
    for(;;){DWORD available=0;if(!WinHttpQueryDataAvailable(request.p,&available)){r.status=0;break;}if(!available)break;if(r.body.size()+available>2*1024*1024){r.status=0;break;}size_t offset=r.body.size();r.body.resize(offset+available);DWORD read=0;if(!WinHttpReadData(request.p,r.body.data()+offset,available,&read)){r.status=0;break;}r.body.resize(offset+read);if(!read)break;}
    return r;
}
std::wstring Encode(const std::wstring&s){int n=WideCharToMultiByte(CP_UTF8,0,s.data(),(int)s.size(),nullptr,0,nullptr,nullptr);std::string u(n,0);WideCharToMultiByte(CP_UTF8,0,s.data(),(int)s.size(),u.data(),n,nullptr,nullptr);std::wstring out;const wchar_t* hex=L"0123456789ABCDEF";for(unsigned char c:u){if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='.'||c=='_'||c=='~')out+=(wchar_t)c;else{out+=L'%';out+=hex[c>>4];out+=hex[c&15];}}return out;}
std::wstring Coordinates(const AppSettings&s){std::wostringstream out;out.imbue(std::locale::classic());out<<std::fixed<<std::setprecision(5)<<s.latitude<<L","<<s.longitude;return out.str();}
bool Date(const std::string&s){return s.size()>=19&&s[4]=='-'&&s[7]=='-'&&s[10]=='T'&&s[13]==':'&&s[16]==':';}
bool Old(const std::string&s,int hours){std::tm t{};std::istringstream in(s);in>>std::get_time(&t,"%Y-%m-%dT%H:%M:%S");if(in.fail())return true;auto seconds=_mkgmtime(&t);return seconds<0||std::time(nullptr)-seconds>hours*3600;}
void Failed(environment::Reading&r,DWORD status){r.stale=r.valid;r.detail=status==401||status==403?L"Проверьте API-ключ и тариф":status==429?L"Лимит запросов — повтор позже":status==200?L"Некорректный ответ сервиса":L"Нет связи с сервисом";}
}
namespace environment {
bool ParseCities(const std::string& body,std::vector<Place>& out){try{auto j=json::parse(body);std::vector<Place> places;if(!j.contains("results")){if(j.value("error",false))return false;out.clear();return true;}for(const auto& item:j.at("results")){Place p;p.name=Wide(item.at("name").get<std::string>());p.latitude=item.at("latitude").get<double>();p.longitude=item.at("longitude").get<double>();if(p.name.empty()||!std::isfinite(p.latitude)||!std::isfinite(p.longitude)||p.latitude< -90||p.latitude>90||p.longitude< -180||p.longitude>180)continue;p.detail=Wide(item.value("country",std::string{}));auto region=Wide(item.value("admin1",std::string{}));if(!region.empty()&&region!=p.name){if(!p.detail.empty())p.detail+=L" · ";p.detail+=region;}places.push_back(std::move(p));if(places.size()==6)break;}out=std::move(places);return true;}catch(...){return false;}}
bool SearchCities(const std::wstring& query,std::vector<Place>& places,std::wstring& error){if(query.size()<2||query.size()>100){error=L"Введите название города (от 2 букв).";return false;}auto response=Get(L"geocoding-api.open-meteo.com",L"/v1/search?name="+Encode(query)+L"&count=6&language="+i18n::Code()+L"&format=json");if(response.status!=200||!ParseCities(response.body,places)){error=L"Не удалось найти город. Проверьте подключение и повторите.";return false;}if(places.empty()){error=L"Город не найден. Уточните название, например: Париж, Франция.";return false;}return true;}
int AirIcon(int aqi){return aqi<=50?0:aqi<=70?1:2;}
bool ParseAir(const std::string&body,Reading&out){try{auto j=json::parse(body);if(j.at("status")!="success")return false;auto p=j.at("data").at("current").at("pollution");if(!p.at("aqius").is_number_integer())return false;int aqi=p.at("aqius").get<int>();auto ts=p.at("ts").get<std::string>();if(aqi<0||aqi>1000||!Date(ts))return false;Reading r;r.valid=true;r.value=aqi;r.icon=AirIcon(aqi);r.observed=Wide(ts);r.stale=Old(ts,4);r.detail=L"US AQI · IQAir · "+Wide(j.at("data").at("city").get<std::string>());out=std::move(r);return true;}catch(...){return false;}}
bool ParseWeather(const std::string&body,Reading&out){try{auto j=json::parse(body);auto p=j.at(0);double temp=p.at("Temperature").at("Metric").at("Value").get<double>();int icon=p.at("WeatherIcon").get<int>();auto ts=p.at("LocalObservationDateTime").get<std::string>();if(!std::isfinite(temp)||temp< -100||temp>70||icon<1||icon>44||!Date(ts))return false;Reading r;r.valid=true;r.value=(int)std::round(temp);r.icon=icon;r.observed=Wide(ts);auto epoch=p.at("EpochTime").get<int64_t>();r.stale=std::time(nullptr)-epoch>4*3600;r.detail=Wide(p.at("WeatherText").get<std::string>())+L" · AccuWeather";out=std::move(r);return true;}catch(...){return false;}}
bool ParseOpenMeteo(const std::string&body,Reading&out){try{auto p=json::parse(body).at("current");double temp=p.at("temperature_2m").get<double>();int code=p.at("weather_code").get<int>();int day=p.at("is_day").get<int>();auto ts=p.at("time").get<std::string>();if(ts.size()==16)ts+=":00";if(!std::isfinite(temp)||temp< -100||temp>70||!Date(ts)||(day!=0&&day!=1))return false;
 int icon=0;std::wstring name;
 switch(code){case 0:icon=day?1:33;name=L"Ясно";break;case 1:case 2:icon=day?3:35;name=L"Переменная облачность";break;case 3:icon=7;name=L"Облачно";break;case 45:case 48:icon=11;name=L"Туман";break;case 51:case 53:case 55:icon=12;name=L"Морось";break;case 56:case 57:case 66:case 67:icon=26;name=L"Ледяной дождь";break;case 61:case 63:case 65:icon=18;name=L"Дождь";break;case 71:case 73:case 75:case 77:icon=22;name=L"Снег";break;case 80:case 81:case 82:icon=12;name=L"Ливень";break;case 85:case 86:icon=23;name=L"Снежный ливень";break;case 95:case 96:case 99:icon=15;name=L"Гроза";break;default:return false;}
 Reading r;r.valid=true;r.value=(int)std::round(temp);r.icon=icon;r.observed=Wide(ts+"Z");r.stale=Old(ts,4);r.detail=name+L" · Open-Meteo (CC BY 4.0)";out=std::move(r);return true;}catch(...){return false;}}
Snapshot Client::Poll(const AppSettings&s){
    uint64_t now=GetTickCount64();auto coords=Coordinates(s);
    // Credentials are kept only in memory here; never written to caches, logs, URLs in errors.
    auto ai=s.iqairKey+coords,wi=std::to_wstring(s.weatherSource)+s.accuweatherKey+coords+i18n::Code();
    if(ai!=airIdentity){airIdentity=ai;airDue=0;data.air={};}if(wi!=weatherIdentity){weatherIdentity=wi;weatherDue=0;location.clear();data.weather={};}
    if(s.iqairKey.empty()){data.air={};data.air.detail=L"Добавьте API-ключ IQAir в настройках";}
    else if(s.showAir&&now>=airDue){airDue=now+15*60*1000;auto comma=coords.find(L',');auto r=Get(L"api.airvisual.com",L"/v2/nearest_city?lat="+coords.substr(0,comma)+L"&lon="+coords.substr(comma+1)+L"&key="+Encode(s.iqairKey));if(r.status!=200||!ParseAir(r.body,data.air))Failed(data.air,r.status);if(r.status==429||r.status==401||r.status==403)airDue=now+60*60*1000;}
    if(s.weatherSource==0){if(s.showWeather&&now>=weatherDue){weatherDue=now+60*60*1000;auto comma=coords.find(L',');auto r=Get(L"api.open-meteo.com",L"/v1/forecast?latitude="+coords.substr(0,comma)+L"&longitude="+coords.substr(comma+1)+L"&current=temperature_2m,weather_code,is_day&timezone=GMT&forecast_days=1");if(r.status!=200||!ParseOpenMeteo(r.body,data.weather))Failed(data.weather,r.status);}}
    else if(s.accuweatherKey.empty()){data.weather={};data.weather.detail=L"Добавьте API-ключ AccuWeather в настройках";}
    else if(s.showWeather&&now>=weatherDue){weatherDue=now+60*60*1000;DWORD status=200;
        if(location.empty()){auto r=Get(L"dataservice.accuweather.com",L"/locations/v1/cities/geoposition/search?q="+Encode(coords)+L"&language="+i18n::Code(),s.accuweatherKey);status=r.status;if(status==200)try{location=Wide(json::parse(r.body).at("Key").get<std::string>());}catch(...){status=0;}}
        if(!location.empty()){auto r=Get(L"dataservice.accuweather.com",L"/currentconditions/v1/"+Encode(location)+L"?language="+i18n::Code(),s.accuweatherKey);status=r.status;if(status!=200||!ParseWeather(r.body,data.weather))Failed(data.weather,status);}else Failed(data.weather,status);
    }
    // Re-evaluate age even when polling is paused or data is cached.
    if(data.air.valid&&Old(std::string(data.air.observed.begin(),data.air.observed.end()),4))data.air.stale=true;
    return data;
}
}
