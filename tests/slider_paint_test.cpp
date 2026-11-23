// Regression: moving the thumb back and forth must produce exactly the same
// pixels as painting its final position once onto a fresh surface.
#include "../src/settings_ui.cpp"
#include <cassert>
#include <cstdio>
// Stub persistence so this UI regression never changes the user's settings.
static bool savedDate=false;static int dateSaves=0,dateEvents=0;static bool notifiedDate=false;
namespace settings {
AppSettings Load(){return AppSettings{};}
bool Save(const AppSettings&,std::wstring&){return true;}
bool SaveColors(COLORREF,std::wstring&){return true;}
bool SaveLanguage(const std::wstring&,std::wstring&){return true;}
bool SaveDate(bool enabled,std::wstring&){savedDate=enabled;++dateSaves;return true;}
}
LRESULT CALLBACK OwnerProc(HWND h,UINT msg,WPARAM w,LPARAM l){if(msg==settingsui::DateChanged){notifiedDate=w!=0;++dateEvents;return 0;}return DefWindowProcW(h,msg,w,l);}
struct Surface {
 HDC dc=CreateCompatibleDC(nullptr);void* pixels=nullptr;HBITMAP bitmap;HGDIOBJ old;
 Surface(int w,int h){BITMAPINFO info{};info.bmiHeader={sizeof(BITMAPINFOHEADER),w,-h,1,32,BI_RGB};bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);old=SelectObject(dc,bitmap);memset(pixels,0,(size_t)w*h*4);}
 ~Surface(){SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);}
};
int main(){
 Gdiplus::GdiplusStartupInput input;ULONG_PTR token;assert(Gdiplus::GdiplusStartup(&token,&input,nullptr)==Gdiplus::Ok);
 instance=GetModuleHandleW(nullptr);draft=AppSettings{};
 INITCOMMONCONTROLSEX common{sizeof(common),ICC_WIN95_CLASSES|ICC_LINK_CLASS};InitCommonControlsEx(&common);
 WNDCLASSW wc{};wc.lpfnWndProc=Proc;wc.hInstance=instance;wc.lpszClassName=L"WedguSliderPaintTest";RegisterClassW(&wc);
 WNDCLASSW ownerClass{};ownerClass.lpfnWndProc=OwnerProc;ownerClass.hInstance=instance;ownerClass.lpszClassName=L"WedguDateEventTest";RegisterClassW(&ownerClass);owner=CreateWindowExW(0,ownerClass.lpszClassName,L"",WS_POPUP,-4000,-4000,1,1,nullptr,nullptr,instance,nullptr);assert(owner);
 HWND parent=CreateWindowExW(0,wc.lpszClassName,L"",WS_POPUP|WS_CLIPCHILDREN,-4000,-4000,1000,900,nullptr,nullptr,instance,nullptr);assert(parent);
 for(int id:{BATTERY,LOCK_SCALE}){
  HWND slider=GetDlgItem(parent,id);RECT r;GetClientRect(slider,&r);Surface swept(r.right,r.bottom),fresh(r.right,r.bottom);
  int low=(int)SendMessageW(slider,TBM_GETRANGEMIN,0,0),high=(int)SendMessageW(slider,TBM_GETRANGEMAX,0,0);
  for(int sweep=0;sweep<3;++sweep){for(int pos=low;pos<=high;pos+=2){SendMessageW(slider,TBM_SETPOS,FALSE,pos);PaintSlider(slider,swept.dc);}for(int pos=high;pos>=low;pos-=3){SendMessageW(slider,TBM_SETPOS,FALSE,pos);PaintSlider(slider,swept.dc);}}
  SendMessageW(slider,TBM_SETPOS,FALSE,(low+high)/2);PaintSlider(slider,swept.dc);PaintSlider(slider,fresh.dc);GdiFlush();assert(memcmp(swept.pixels,fresh.pixels,(size_t)r.right*r.bottom*4)==0);
 }
 assert(!Checked(SHOW_DATE));
 SendMessageW(parent,WM_COMMAND,SHOW_DATE,(LPARAM)GetDlgItem(parent,SHOW_DATE));assert(Checked(SHOW_DATE)&&savedDate&&notifiedDate&&dateSaves==1&&dateEvents==1);
 SendMessageW(parent,WM_COMMAND,SHOW_DATE,(LPARAM)GetDlgItem(parent,SHOW_DATE));assert(!Checked(SHOW_DATE)&&!savedDate&&!notifiedDate&&dateSaves==2&&dateEvents==2);
 // Language switches must preserve edits and change only language persistence.
 SetText(CITY,L"Paris");SetText(IQ_KEY,L"unsaved-test-key");SendDlgItemMessageW(parent,BATTERY,TBM_SETPOS,TRUE,37);checks[SHOW_WEATHER]=false;
 for(const auto&language:i18n::Languages()){ChangeLanguage(language.code);assert(i18n::Code()==i18n::Resolve(language.code));assert(Text(CITY)==L"Paris");assert(Text(IQ_KEY)==L"unsaved-test-key");assert(SendDlgItemMessageW(parent,BATTERY,TBM_GETPOS,0,0)==37);assert(!Checked(SHOW_WEATHER));assert(page==3);for(auto&control:controls){RECT bounds;GetWindowRect(control.h,&bounds);MapWindowPoints(nullptr,parent,(POINT*)&bounds,2);assert(bounds.left>=0&&bounds.right<=MulDiv(822,GetDpiForWindow(parent),96)+1);} }
 i18n::Configure(L"en");
 DestroyWindow(owner);
 DestroyWindow(parent);Gdiplus::GdiplusShutdown(token);puts("Both sliders repaint cleanly after repeated sweeps; no residual thumb pixels. Date toggle saves and notifies immediately.");
}
