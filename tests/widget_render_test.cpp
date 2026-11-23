#include "../src/widget.h"
#include "../src/weather_icons.h"
#include "../src/appearance.h"
#include <gdiplus.h>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <algorithm>
int main(){
 Gdiplus::GdiplusStartupInput input;ULONG_PTR token;assert(Gdiplus::GdiplusStartup(&token,&input,nullptr)==Gdiplus::Ok);
 CreateDirectoryW(L"build/preview-1.1",nullptr);CreateDirectoryW(L"build/preview-1.1/weather-icons",nullptr);
 {
 for(int dpi:{48,96,144,192,384})for(int c=1;c<=44;++c){WidgetItem i;i.type=WidgetType::Weather;i.condition=c;i.text=L" ";auto p=widget::RenderStrip({i},dpi,true);assert(p.width>0&&p.height>0);assert(std::count_if(p.pixels.begin(),p.pixels.end(),[](DWORD v){return (v>>24)>30;})>10);}
 WidgetItem battery;battery.kind=DeviceKind::Mouse;battery.percent=5;battery.low=true;battery.charging=true;
 for(COLORREF color:{RGB(234,182,204),RGB(20,110,220),RGB(255,255,255)}){auto rendered=widget::RenderStrip({battery},144,true,color);int count=0;for(DWORD pixel:rendered.pixels)if((pixel>>24)==255){assert(((pixel>>16)&255)==GetRValue(color)&&((pixel>>8)&255)==GetGValue(color)&&(pixel&255)==GetBValue(color));++count;}assert(count>0);}
 assert(appearance::LockscreenColor(CLR_INVALID)==RGB(255,255,255));assert(appearance::TaskbarColor(true,CLR_INVALID)!=appearance::TaskbarColor(false,CLR_INVALID));
 const int codes[]={1,3,7,33,35,11,12,15,18,22,25,32,5,30,31,43};
 const wchar_t* names[]={L"Солнце",L"Переменная облачность",L"Облачно",L"Ясная ночь",L"Облачная ночь",L"Туман",L"Ливень",L"Гроза",L"Дождь",L"Снег",L"Дождь со снегом",L"Ветер",L"Дымка",L"Жара",L"Мороз",L"Ночной снег"};
 using namespace Gdiplus;Bitmap canvas(960,480,PixelFormat32bppARGB);Graphics g(&canvas);g.Clear(Color(255,247,247,249));g.SetSmoothingMode(SmoothingModeAntiAlias);Font label(L"Segoe UI",12,FontStyleRegular,UnitPixel);SolidBrush ink(Color(255,62,65,74));
 for(int n=0;n<16;++n){int code=codes[n];WidgetItem i;i.type=WidgetType::Weather;i.condition=code;i.text=L" ";auto image=widget::RenderStrip({i},192,true);Bitmap icon(image.width,image.height,image.width*4,PixelFormat32bppPARGB,(BYTE*)image.pixels.data());int x=(n%4)*240,y=(n/4)*120;g.DrawImage(&icon,x+88,y+12);g.DrawString(names[n],-1,&label,PointF((float)x+20,(float)y+84),&ink);
   std::ofstream file("build/preview-1.1/weather-icons/weather-"+std::to_string(code)+".svg");file<<"<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"1.5\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\""<<weathericons::Paths(code)<<"\"/></svg>";
 }
 g.Flush();const CLSID png={0x557cf406,0x1a04,0x11d3,{0x9a,0x73,0,0,0xf8,0x1e,0xf3,0x2e}};assert(canvas.Save(L"build/preview-1.1/weather-icons.png",&png,nullptr)==Ok);
 }
 Gdiplus::GdiplusShutdown(token);puts("All 44 weather condition codes render at 5 DPI levels. SVG and PNG previews exported.");
}
