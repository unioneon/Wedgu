#include "lockscreen.h"
#include "appearance.h"
#include <roapi.h>
#include <winstring.h>
#include <inspectable.h>
#include <asyncinfo.h>
#include <shcore.h>
#include <shlwapi.h>
#include <gdiplus.h>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <algorithm>

// Keep the COM interface externally visible: implementations are provided by WinRT,
// not this translation unit (GCC must not devirtualize its calls as unreachable).
struct LockStatics:IInspectable {
 virtual HRESULT STDMETHODCALLTYPE OriginalImageFile(IInspectable**)=0;
 virtual HRESULT STDMETHODCALLTYPE GetImageStream(IUnknown**)=0;
 virtual HRESULT STDMETHODCALLTYPE SetImageFileAsync(IUnknown*,IInspectable**)=0;
 virtual HRESULT STDMETHODCALLTYPE SetImageStreamAsync(IUnknown*,IInspectable**)=0;
};
namespace {
const GUID lockIID={0x3ee9d3ad,0xb607,0x40ae,{0xb4,0x26,0x76,0x31,0xd9,0x82,0x12,0x69}};
const GUID randomIID={0x905a0fe1,0xbc53,0x11df,{0x8c,0x49,0,0x1e,0x4f,0xc6,0x86,0xda}};
const GUID asyncIID={0x36,0,0,{0xc0,0,0,0,0,0,0,0x46}};
std::mutex mutex;std::condition_variable cv;std::thread worker;
bool pending=false,stop=false;AppSettings config;std::vector<WidgetItem> items;
HWND notify;UINT message;std::wstring lastSignature;
bool Exists(const std::wstring&p){return GetFileAttributesW(p.c_str())!=INVALID_FILE_ATTRIBUTES;}
HRESULT Apply(LockStatics* api,const std::wstring&file){
 IStream* stream=nullptr;HRESULT hr=SHCreateStreamOnFileEx(file.c_str(),STGM_READ|STGM_SHARE_DENY_WRITE,0,FALSE,nullptr,&stream);if(FAILED(hr))return hr;
 IUnknown* random=nullptr;hr=CreateRandomAccessStreamOverStream(stream,BSOS_DEFAULT,randomIID,(void**)&random);stream->Release();if(FAILED(hr))return hr;
 IInspectable* action=nullptr;hr=api->SetImageStreamAsync(random,&action);random->Release();
 if(SUCCEEDED(hr)&&action){IAsyncInfo* info=nullptr;hr=action->QueryInterface(asyncIID,(void**)&info);if(SUCCEEDED(hr)){AsyncStatus state=Started;auto until=GetTickCount64()+15000;while(SUCCEEDED(hr)&&state==Started&&GetTickCount64()<until){Sleep(50);hr=info->get_Status(&state);}if(SUCCEEDED(hr)&&state==Error)info->get_ErrorCode(&hr);if(state==Started){info->Cancel();hr=HRESULT_FROM_WIN32(ERROR_TIMEOUT);}if(state==Canceled)hr=E_ABORT;info->Close();info->Release();}action->Release();}return hr;
}
HRESULT Backup(LockStatics*api,const std::wstring&path){
 IUnknown* random=nullptr;HRESULT hr=api->GetImageStream(&random);if(FAILED(hr))return hr;
 IStream* stream=nullptr;hr=CreateStreamOverRandomAccessStream(random,IID_IStream,(void**)&stream);random->Release();if(FAILED(hr))return hr;
 IStream* out=nullptr;auto temp=path+L".tmp";hr=SHCreateStreamOnFileEx(temp.c_str(),STGM_CREATE|STGM_WRITE|STGM_SHARE_EXCLUSIVE,0,TRUE,nullptr,&out);
 if(SUCCEEDED(hr)){STATSTG stat{};hr=stream->Stat(&stat,STATFLAG_NONAME);if(SUCCEEDED(hr)&&(stat.cbSize.QuadPart==0||stat.cbSize.QuadPart>32*1024*1024))hr=E_FAIL;if(SUCCEEDED(hr)){ULARGE_INTEGER read{},written{};hr=stream->CopyTo(out,stat.cbSize,&read,&written);if(read.QuadPart!=stat.cbSize.QuadPart||written.QuadPart!=stat.cbSize.QuadPart)hr=E_FAIL;}out->Release();if(SUCCEEDED(hr)&&!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING))hr=HRESULT_FROM_WIN32(GetLastError());if(FAILED(hr))DeleteFileW(temp.c_str());}stream->Release();return hr;
}
HRESULT Compose(const AppSettings&s,const std::vector<WidgetItem>&list,const std::wstring&source,const std::wstring&out){
 using namespace Gdiplus;Bitmap background(source.c_str());if(background.GetLastStatus()!=Ok)return E_FAIL;
 const int w=background.GetWidth(),h=background.GetHeight();if(w<100||h<100||w>16000||h>16000)return E_FAIL;
 Bitmap canvas(w,h,PixelFormat32bppARGB);Graphics g(&canvas);g.DrawImage(&background,0,0,w,h);
 int dpi=std::max(48,MulDiv(h,96*s.lockscreenScale,1080*100));auto strip=widget::RenderStrip(list,dpi,false,appearance::LockscreenColor(s.taskbarColor));
 if(strip.width>w-40){dpi=std::max(24,MulDiv(dpi,w-40,strip.width));strip=widget::RenderStrip(list,dpi,false,appearance::LockscreenColor(s.taskbarColor));}
 Bitmap overlay(strip.width,strip.height,strip.width*4,PixelFormat32bppPARGB,(BYTE*)strip.pixels.data());g.DrawImage(&overlay,(w-strip.width)/2,h-h/11-strip.height,strip.width,strip.height);g.Flush();
 const CLSID png={0x557cf406,0x1a04,0x11d3,{0x9a,0x73,0,0,0xf8,0x1e,0xf3,0x2e}};return canvas.Save(out.c_str(),&png,nullptr)==Ok?S_OK:E_FAIL;
}
std::wstring Run(const AppSettings&s,const std::vector<WidgetItem>&list){
 auto dir=settings::DataDirectory();if(dir.empty())return L"Не удалось открыть папку локскрина";
 auto backup=dir+L"\\lockscreen-original.image",marker=dir+L"\\lockscreen-applied.flag",output=dir+L"\\lockscreen-widget.png";
 HSTRING cls=nullptr;const wchar_t* name=L"Windows.System.UserProfile.LockScreen";WindowsCreateString(name,(UINT32)wcslen(name),&cls);LockStatics* api=nullptr;HRESULT hr=RoGetActivationFactory(cls,lockIID,(void**)&api);WindowsDeleteString(cls);if(FAILED(hr))return L"Windows не разрешила доступ к фону локскрина";
 if(!s.lockscreenEnabled||list.empty()){
   if(Exists(marker)&&Exists(backup)){hr=Apply(api,backup);if(SUCCEEDED(hr)){DeleteFileW(marker.c_str());DeleteFileW(backup.c_str());}}
   api->Release();return SUCCEEDED(hr)?L"Локскрин: виджет отключён":L"Не удалось восстановить фон локскрина";
 }
 if(!Exists(marker)){hr=Backup(api,backup);}
 if(SUCCEEDED(hr))hr=Compose(s,list,s.lockscreenBackground.empty()?backup:s.lockscreenBackground,output);
 if(SUCCEEDED(hr)){
   // Mark before applying: an interrupted async operation must still be restorable.
   HANDLE file=CreateFileW(marker.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);else hr=HRESULT_FROM_WIN32(GetLastError());
   if(SUCCEEDED(hr))hr=Apply(api,output);
 }
 api->Release();return SUCCEEDED(hr)?L"Локскрин: фон с виджетом обновлён":L"Локскрин: Windows не разрешила обновить фон";
}
void Work(){RoInitialize(RO_INIT_MULTITHREADED);for(;;){AppSettings s;std::vector<WidgetItem> list;{std::unique_lock<std::mutex> lock(mutex);cv.wait(lock,[]{return pending||stop;});if(stop)break;s=config;list=items;pending=false;}auto* result=new std::wstring(Run(s,list));if(!PostMessageW(notify,message,0,(LPARAM)result))delete result;}RoUninitialize();}
}
namespace lockscreen {
void Start(HWND h,UINT m){notify=h;message=m;stop=false;worker=std::thread(Work);}
void Update(const AppSettings&s,const std::vector<WidgetItem>&list){
 std::wstring sig=std::to_wstring(s.lockscreenEnabled)+L":"+std::to_wstring(s.lockscreenScale)+L":"+std::to_wstring(s.taskbarColor)+s.lockscreenBackground;for(const auto&i:list)sig+=L"|"+std::to_wstring((int)i.kind)+L":"+std::to_wstring((int)i.type)+L":"+std::to_wstring(i.condition)+L":"+std::to_wstring(i.percent)+i.text+std::to_wstring(i.dim)+std::to_wstring(i.charging);
 std::lock_guard<std::mutex> lock(mutex);if(sig==lastSignature)return;lastSignature=sig;config=s;items=list;pending=true;cv.notify_one();
}
void Stop(){{std::lock_guard<std::mutex> lock(mutex);stop=true;}cv.notify_one();if(worker.joinable())worker.join();}
}
