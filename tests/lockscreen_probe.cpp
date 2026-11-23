#include <windows.h>
#include <roapi.h>
#include <winstring.h>
#include <inspectable.h>
#include <shcore.h>
#include <shlwapi.h>
#include <cstdio>
struct LockStatics: IInspectable {
    virtual HRESULT STDMETHODCALLTYPE OriginalImageFile(IInspectable**)=0;
    virtual HRESULT STDMETHODCALLTYPE GetImageStream(IUnknown**)=0;
    virtual HRESULT STDMETHODCALLTYPE SetImageFileAsync(IUnknown*,IInspectable**)=0;
    virtual HRESULT STDMETHODCALLTYPE SetImageStreamAsync(IUnknown*,IInspectable**)=0;
};
int main(){
    RoInitialize(RO_INIT_MULTITHREADED);
    HSTRING name;const wchar_t* cls=L"Windows.System.UserProfile.LockScreen";
    WindowsCreateString(cls,(UINT32)wcslen(cls),&name);
    const GUID iid={0x3ee9d3ad,0xb607,0x40ae,{0xb4,0x26,0x76,0x31,0xd9,0x82,0x12,0x69}};
    LockStatics* api=nullptr;HRESULT hr=RoGetActivationFactory(name,iid,(void**)&api);
    WindowsDeleteString(name);printf("LockScreen activation: %08lX\n",(unsigned long)hr);
    if(FAILED(hr))return 1;
    IUnknown* random=nullptr;hr=api->GetImageStream(&random);printf("Read original lockscreen: %08lX\n",(unsigned long)hr);
    if(SUCCEEDED(hr)){
        IStream* stream=nullptr;hr=CreateStreamOverRandomAccessStream(random,IID_IStream,(void**)&stream);
        if(SUCCEEDED(hr)){IStream* file=nullptr;hr=SHCreateStreamOnFileEx(L"build/lockscreen-original-image",STGM_CREATE|STGM_WRITE|STGM_SHARE_EXCLUSIVE,FILE_ATTRIBUTE_NORMAL,TRUE,nullptr,&file);if(SUCCEEDED(hr)){ULARGE_INTEGER count;count.QuadPart=32*1024*1024;stream->CopyTo(file,count,nullptr,nullptr);file->Release();puts("Original image saved locally for preview.");}stream->Release();}random->Release();
    }
    api->Release();RoUninitialize();return FAILED(hr)?1:0;
}
