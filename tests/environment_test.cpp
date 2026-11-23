#include "../src/environment.h"
#include <cassert>
#include <cstdio>
#include <string>
int main(int argc,char**argv){
 using namespace environment;
 if(argc>1&&std::string(argv[1])=="--live-city"){std::vector<Place> places;std::wstring error;bool ok=SearchCities(L"Ташкент",places,error);printf("City search: success=%d, matching_places=%zu\n",ok,places.size());return ok?0:1;}
 if(argc>1&&std::string(argv[1])=="--live-weather"){auto s=settings::Load();s.showAir=false;s.showWeather=true;s.weatherSource=0;Client client;auto r=client.Poll(s);printf("Open-Meteo: valid=%d, stale=%d, temperature_C=%d, icon=%d\n",r.weather.valid,r.weather.stale,r.weather.value,r.weather.icon);return r.weather.valid?0:1;}
 if(argc>1&&std::string(argv[1])=="--live"){
   auto s=settings::Load();s.showWeather=false;Client client;auto r=client.Poll(s);
   printf("IQAir: valid=%d, stale=%d, US_AQI=%d\n",r.air.valid,r.air.stale,r.air.value);
   return r.air.valid?0:1;
 }
 Reading r;
 const std::string start=R"({"status":"success","data":{"city":"Tashkent","current":{"pollution":{"aqius":)";
 const std::string end=R"(,"ts":"2026-11-23T11:00:00.000Z"}}}})";
 for(int a:{0,50,51,70,71,500}){assert(ParseAir(start+std::to_string(a)+end,r));assert(r.value==a);assert(r.icon==(a<=50?0:a<=70?1:2));}
 auto last=r;assert(!ParseAir("<html>Forbidden</html>",r));assert(r.value==last.value);
 assert(!ParseAir(start+"-1"+end,r));assert(!ParseAir(start+"2.5"+end,r));
 assert(!ParseAir(R"({"status":"fail","data":{"message":"quota"}})",r));
 assert(!ParseAir(R"({"status":"success","data":{"current":{"pollution":{"aqius":10}}}})",r));
 assert(ParseWeather(R"([{"Temperature":{"Metric":{"Value":-2.6}},"WeatherIcon":12,"WeatherText":"Rain","LocalObservationDateTime":"2026-11-23T16:00:00+05:00","EpochTime":1795431600}])",r));
 assert(r.value==-3&&r.icon==12);assert(!ParseWeather("[]",r));
 assert(!ParseWeather(R"([{"Temperature":{"Metric":{"Value":200}},"WeatherIcon":12}])",r));
 assert(ParseOpenMeteo(R"({"current":{"temperature_2m":17.4,"weather_code":0,"is_day":1,"time":"2026-11-23T12:00"}})",r));assert(r.value==17&&r.icon==1);
 assert(ParseOpenMeteo(R"({"current":{"temperature_2m":-2.6,"weather_code":0,"is_day":0,"time":"2026-11-23T12:00"}})",r));assert(r.value==-3&&r.icon==33);
 assert(ParseOpenMeteo(R"({"current":{"temperature_2m":5,"weather_code":95,"is_day":1,"time":"2026-11-23T12:00"}})",r));assert(r.icon==15);assert(!ParseOpenMeteo("{}",r));assert(!ParseOpenMeteo(R"({"current":{"temperature_2m":5,"weather_code":999,"is_day":1,"time":"2026-11-23T12:00"}})",r));
 std::vector<Place> places;assert(ParseCities(R"({"results":[{"name":"Tashkent","latitude":41.31,"longitude":69.28,"country":"Uzbekistan"},{"name":"Invalid","latitude":500,"longitude":0}]})",places));assert(places.size()==1&&places[0].name==L"Tashkent"&&places[0].longitude==69.28);assert(ParseCities("{}",places)&&places.empty());assert(!ParseCities("<html>blocked</html>",places));assert(!ParseCities(R"({"error":true})",places));
 AppSettings s;std::wstring error;assert(settings::Validate(s,error));s.batteryMinutes=0;assert(!settings::Validate(s,error));s.batteryMinutes=60;s.lockscreenScale=200;assert(settings::Validate(s,error));s.lockscreenScale=201;assert(!settings::Validate(s,error));s.lockscreenScale=50;s.latitude=91;assert(!settings::Validate(s,error));s.latitude=41;s.iqairKey=L"has spaces";assert(!settings::Validate(s,error));
 puts("Provider response validation, AQI icon boundaries and settings limits passed.");
}
