#include "location_service.h"
#include <locationapi.h>
#include <thread>
#include <cmath>
namespace location {
bool SystemPlace(environment::Place& out,std::wstring& error,bool requestPermission){
 HRESULT init=CoInitializeEx(nullptr,COINIT_MULTITHREADED);if(FAILED(init)){error=L"Геолокация Windows недоступна. Введите город вручную.";return false;}
 const CLSID cls={0xe5b8e079,0xee6d,0x4e33,{0xa4,0x38,0xc8,0x7f,0x2e,0x95,0x92,0x54}};
 ILocation* api=nullptr;HRESULT hr=CoCreateInstance(cls,nullptr,CLSCTX_INPROC_SERVER,__uuidof(ILocation),(void**)&api);bool valid=false;
 if(SUCCEEDED(hr)){
  IID type=__uuidof(ILatLongReport);if(requestPermission)hr=api->RequestPermissions(nullptr,&type,1,FALSE);
  auto until=GetTickCount64()+15000;
  while(SUCCEEDED(hr)&&GetTickCount64()<until){
   ILocationReport* report=nullptr;hr=api->GetReport(type,&report);
   if(SUCCEEDED(hr)){
    ILatLongReport* position=nullptr;hr=report->QueryInterface(type,(void**)&position);report->Release();
    if(SUCCEEDED(hr)){environment::Place place;hr=position->GetLatitude(&place.latitude);if(SUCCEEDED(hr))hr=position->GetLongitude(&place.longitude);position->Release();
     if(SUCCEEDED(hr)&&std::isfinite(place.latitude)&&std::isfinite(place.longitude)&&place.latitude>=-90&&place.latitude<=90&&place.longitude>=-180&&place.longitude<=180){
      place.name=L"Моё местоположение";place.detail=L"Определено Windows";
      ILocationReport* civicReport=nullptr;if(SUCCEEDED(api->GetReport(__uuidof(ICivicAddressReport),&civicReport))){ICivicAddressReport* civic=nullptr;if(SUCCEEDED(civicReport->QueryInterface(__uuidof(ICivicAddressReport),(void**)&civic))){BSTR city=nullptr;if(SUCCEEDED(civic->GetCity(&city))&&city&&*city)place.name=city;SysFreeString(city);civic->Release();}civicReport->Release();}
      out=std::move(place);valid=true;
     }
    }
    break;
   }
   if(hr==E_ACCESSDENIED&&!requestPermission)break;
   // The provider may need a moment to produce its first location report.
   Sleep(250);hr=S_OK;
  }
  api->Release();
 }
 CoUninitialize();if(!valid)error=L"Windows не определила местоположение. Включите геолокацию в Windows или введите город вручную.";
 return valid;
}
std::shared_ptr<Job> Start(const std::wstring& query,bool system){
 auto job=std::make_shared<Job>();job->query=query;job->system=system;
 // The job owns its result; closing the settings window never leaves a posted
 // pointer behind or blocks the UI on a network/location request.
 std::thread([job]{if(job->system){environment::Place place;if(SystemPlace(place,job->error,true))job->places.push_back(std::move(place));}else environment::SearchCities(job->query,job->places,job->error);job->done.store(true,std::memory_order_release);}).detach();
 return job;
}
}
