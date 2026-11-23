#include "color_picker.h"
#include "i18n.h"
#include "typography.h"
#include <gdiplus.h>
#include <dwmapi.h>
#include <algorithm>
#include <cmath>
#include <cwchar>
#include <string>

namespace {
using namespace Gdiplus;
HWND window=nullptr,owner;HINSTANCE instance;HFONT font;UINT chosenMessage,sampleMessage;int target;
double hue=210,saturation=.5,value=1;bool dragging=false;int dragKind=0;
enum{HEX=901,CHOOSE,SAMPLE};
int P(int n){return MulDiv(n,(int)GetDpiForWindow(window),96);}
COLORREF RGBFromHSV(){double c=value*saturation,x=c*(1-std::abs(std::fmod(hue/60,2)-1)),m=value-c,r=0,g=0,b=0;if(hue<60){r=c;g=x;}else if(hue<120){r=x;g=c;}else if(hue<180){g=c;b=x;}else if(hue<240){g=x;b=c;}else if(hue<300){r=x;b=c;}else{r=c;b=x;}return RGB((int)std::round((r+m)*255),(int)std::round((g+m)*255),(int)std::round((b+m)*255));}
void HSVFromRGB(COLORREF color){double r=GetRValue(color)/255.,g=GetGValue(color)/255.,b=GetBValue(color)/255.;double high=std::max({r,g,b}),low=std::min({r,g,b}),d=high-low;value=high;saturation=high==0?0:d/high;if(d==0)hue=0;else if(high==r)hue=60*std::fmod((g-b)/d+6,6);else if(high==g)hue=60*((b-r)/d+2);else hue=60*((r-g)/d+4);}
std::wstring Hex(COLORREF c){wchar_t b[8];swprintf(b,8,L"#%02X%02X%02X",GetRValue(c),GetGValue(c),GetBValue(c));return b;}
void Updated(){SetDlgItemTextW(window,HEX,Hex(RGBFromHSV()).c_str());InvalidateRect(window,nullptr,FALSE);}
void DrawTextAt(HDC dc,const wchar_t*t,int x,int y,int w,int h){RECT r{P(x),P(y),P(x+w),P(y+h)};DrawTextW(dc,i18n::T(t).c_str(),-1,&r,DT_NOPREFIX|DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|(i18n::RTL()?DT_RTLREADING|DT_RIGHT:0));}
void Paint(HWND h){PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);RECT r;GetClientRect(h,&r);HDC buffer=CreateCompatibleDC(dc);HBITMAP bitmap=CreateCompatibleBitmap(dc,r.right,r.bottom);auto old=SelectObject(buffer,bitmap);FillRect(buffer,&r,(HBRUSH)GetStockObject(WHITE_BRUSH));
 Graphics g(buffer);g.SetSmoothingMode(SmoothingModeAntiAlias);auto selected=SelectObject(buffer,font);SetBkMode(buffer,TRANSPARENT);SetTextColor(buffer,RGB(48,51,60));DrawTextAt(buffer,L"Свой цвет",22,16,284,28);
 // Two gradients form the full saturation/value field without per-pixel GDI calls.
 double savedS=saturation,savedV=value; saturation=value=1; COLORREF full=RGBFromHSV(); saturation=savedS;value=savedV;
 LinearGradientBrush sv(Point(P(22),P(58)),Point(P(306),P(58)),Color(255,255,255,255),Color(255,GetRValue(full),GetGValue(full),GetBValue(full)));g.FillRectangle(&sv,P(22),P(58),P(284),P(180));
 LinearGradientBrush shade(Point(P(22),P(58)),Point(P(22),P(238)),Color(0,0,0,0),Color(255,0,0,0));g.FillRectangle(&shade,P(22),P(58),P(284),P(180));
 LinearGradientBrush spectrum(Point(P(22),P(258)),Point(P(306),P(258)),Color(255,255,0,0),Color(255,255,0,0));Color stops[]={Color(255,255,0,0),Color(255,255,255,0),Color(255,0,255,0),Color(255,0,255,255),Color(255,0,0,255),Color(255,255,0,255),Color(255,255,0,0)};REAL positions[]={0,1.f/6,2.f/6,3.f/6,4.f/6,5.f/6,1};spectrum.SetInterpolationColors(stops,positions,7);g.FillRectangle(&spectrum,P(22),P(258),P(284),P(16));
 Pen white(Color(255,255,255,255),2);Pen dark(Color(255,65,68,76),1);float cx=(float)(P(22)+saturation*P(283)),cy=(float)(P(58)+(1-value)*P(179));g.DrawEllipse(&dark,cx-P(5),cy-P(5),(float)P(10),(float)P(10));g.DrawEllipse(&white,cx-P(4),cy-P(4),(float)P(8),(float)P(8));float hx=(float)(P(22)+hue/359*P(283));g.DrawRectangle(&white,hx-P(2),(float)P(255),(float)P(4),(float)P(22));
 SolidBrush color(Color(255,GetRValue(RGBFromHSV()),GetGValue(RGBFromHSV()),GetBValue(RGBFromHSV())));g.FillEllipse(&color,P(22),P(301),P(30),P(30));DrawTextAt(buffer,L"HEX",68,300,38,30);SetTextColor(buffer,RGB(135,138,148));DrawTextAt(buffer,L"Пипетка: выберите любой цвет на экране",22,346,284,24);
 SelectObject(buffer,selected);BitBlt(dc,0,0,r.right,r.bottom,buffer,0,0,SRCCOPY);SelectObject(buffer,old);DeleteObject(bitmap);DeleteDC(buffer);EndPaint(h,&ps);}
void Track(LPARAM l){int x=(short)LOWORD(l),y=(short)HIWORD(l);if(dragKind==1){saturation=std::clamp((x-P(22))/(double)P(283),0.,1.);value=1-std::clamp((y-P(58))/(double)P(179),0.,1.);}else hue=std::clamp((x-P(22))/(double)P(283),0.,1.)*359;Updated();}
LRESULT CALLBACK Proc(HWND h,UINT m,WPARAM w,LPARAM l){switch(m){case WM_CREATE:{window=h;font=typography::Font(P(13));auto add=[&](const wchar_t*cls,const wchar_t*name,DWORD style,int id,int x,int y,int width,int height){HWND c=CreateWindowExW(wcscmp(cls,L"EDIT")==0?WS_EX_CLIENTEDGE:0,cls,i18n::T(name).c_str(),WS_CHILD|WS_VISIBLE|WS_TABSTOP|style,P(x),P(y),P(width),P(height),h,(HMENU)(INT_PTR)id,instance,nullptr);SendMessageW(c,WM_SETFONT,(WPARAM)font,TRUE);return c;};HWND edit=add(L"EDIT",L"",ES_AUTOHSCROLL,HEX,112,302,194,28);SendMessageW(edit,EM_SETLIMITTEXT,7,0);add(L"BUTTON",L"Пипетка",BS_PUSHBUTTON,SAMPLE,22,386,126,32);add(L"BUTTON",L"Выбрать",BS_DEFPUSHBUTTON,CHOOSE,180,386,126,32);Updated();return 0;}
case WM_PAINT:Paint(h);return 0;case WM_ERASEBKGND:return 1;
case WM_LBUTTONDOWN:{int x=(short)LOWORD(l),y=(short)HIWORD(l);if(x>=P(22)&&x<=P(306)&&y>=P(58)&&y<=P(238))dragKind=1;else if(x>=P(22)&&x<=P(306)&&y>=P(250)&&y<=P(282))dragKind=2;else return 0;dragging=true;SetCapture(h);Track(l);return 0;}
case WM_MOUSEMOVE:if(dragging)Track(l);return 0;case WM_LBUTTONUP:if(dragging){Track(l);dragging=false;ReleaseCapture();}return 0;
case WM_KEYDOWN:if(w==VK_ESCAPE){DestroyWindow(h);return 0;}break;
case WM_COMMAND:if(LOWORD(w)==SAMPLE){HWND parent=owner;int id=target;DestroyWindow(h);PostMessageW(parent,sampleMessage,id,0);return 0;}if(LOWORD(w)==CHOOSE){wchar_t hex[9]{};GetDlgItemTextW(h,HEX,hex,9);wchar_t* text=hex;if(*text==L'#')++text;if(wcslen(text)!=6||wcsspn(text,L"0123456789abcdefABCDEF")!=6){i18n::Message(h,L"Введите HEX-цвет, например #A6DAD4.",L"Цвет",MB_ICONINFORMATION);return 0;}unsigned long number=wcstoul(text,nullptr,16);PostMessageW(owner,chosenMessage,target,RGB(number>>16,(number>>8)&255,number&255));DestroyWindow(h);return 0;}break;
case WM_CTLCOLORSTATIC:SetBkColor((HDC)w,RGB(255,255,255));return (LRESULT)GetStockObject(WHITE_BRUSH);
case WM_CLOSE:DestroyWindow(h);return 0;case WM_DESTROY:if(GetCapture()==h)ReleaseCapture();DeleteObject(font);window=nullptr;return 0;
}return DefWindowProcW(h,m,w,l);}
}
namespace colorpicker {
bool Translate(MSG* m){if(!window)return false;if(m->message==WM_KEYDOWN&&m->wParam==VK_ESCAPE){DestroyWindow(window);return true;}if(m->message==WM_KEYDOWN&&m->wParam==VK_RETURN){SendMessageW(window,WM_COMMAND,CHOOSE,0);return true;}return IsDialogMessageW(window,m)!=FALSE;}
void Show(HINSTANCE inst,HWND parent,UINT chosen,UINT sample,int id,COLORREF initial){if(window)DestroyWindow(window);instance=inst;owner=parent;chosenMessage=chosen;sampleMessage=sample;target=id;HSVFromRGB(initial==CLR_INVALID?RGB(255,255,255):initial);WNDCLASSW wc{};wc.lpfnWndProc=Proc;wc.hInstance=inst;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.lpszClassName=L"WedguColorPicker";RegisterClassW(&wc);UINT dpi=GetDpiForWindow(parent);RECT r{0,0,MulDiv(328,dpi,96),MulDiv(438,dpi,96)};DWORD style=WS_CAPTION|WS_SYSMENU|WS_POPUP|WS_CLIPCHILDREN;AdjustWindowRectExForDpi(&r,style,FALSE,WS_EX_TOOLWINDOW,dpi);RECT pr;GetWindowRect(parent,&pr);HWND h=CreateWindowExW(WS_EX_TOOLWINDOW,wc.lpszClassName,i18n::T(L"Wedgu · Цвет").c_str(),style,pr.left+(pr.right-pr.left-r.right+r.left)/2,pr.top+(pr.bottom-pr.top-r.bottom+r.top)/2,r.right-r.left,r.bottom-r.top,parent,nullptr,inst,nullptr);if(h){int round=2;DwmSetWindowAttribute(h,33,&round,sizeof(round));ShowWindow(h,SW_SHOW);SetForegroundWindow(h);}}
void Close(){if(window)DestroyWindow(window);}
}
