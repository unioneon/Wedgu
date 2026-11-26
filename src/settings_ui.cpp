#include "settings_ui.h"
#include "i18n.h"
#include "provider_links.h"
#include "typography.h"
#include "branding.h"
#include "color_picker.h"
#include "appearance.h"
#include "location_service.h"
#include "app_date.h"
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <gdiplus.h>
#include <algorithm>
#include <map>

namespace {
using namespace Gdiplus;
HWND window=nullptr,owner=nullptr,preview=nullptr;HINSTANCE instance;UINT applied;
HFONT font=nullptr,smallFont=nullptr,titleFont=nullptr,semibold=nullptr;AppSettings draft;
std::vector<WidgetItem> previewItems;std::wstring currentStatus;int page=0;
enum {CITY=201,LAT,LON,BATTERY,LOCK_SCALE,SHOW_BATTERY,SHOW_WEATHER,SHOW_AIR,LOCK_ON,IQ_KEY,ACCU_KEY,IQ_LINK,ACCU_LINK,TASK_COLOR,LOCK_COLOR,RESET_SIZE,SAVE,CANCEL,NAV_GENERAL,NAV_LOCK,NAV_SERVICES,NAV_ABOUT,TASK_AUTO,LOCK_AUTO,WEATHER_SOURCE,WEATHER_HINT,TASK_PICK,LOCK_PICK,WEATHER_OPEN,WEATHER_ACCU,SHOW_DATE,LOCATION_MODE,LOCATION_FIND,LOCATION_MANUAL,LOCATION_AUTO,LOCATION_STATUS,LANGUAGE,GITHUB_LINK,UPSTREAM_LINK};
constexpr int LANGUAGE_FIRST=600;
constexpr UINT_PTR LOCATION_TIMER=91;constexpr int LOCATION_FIRST=500;
std::shared_ptr<location::Job> locationJob;std::vector<environment::Place> cityMatches;std::wstring resolvedCity;bool locationReady=true,pendingSave=false;
constexpr UINT COLOR_CHOSEN=WM_APP+31,COLOR_SAMPLE=WM_APP+32;
constexpr int TASK_SWATCH=300,LOCK_SWATCH=310;
const COLORREF presets[]={RGB(248,248,250),RGB(155,160,171),RGB(38,41,48),RGB(245,220,153),RGB(234,182,204),RGB(164,218,211)};
const wchar_t* presetNames[]={L"Белый",L"Серый",L"Чёрный",L"Пастельный жёлтый",L"Пастельный розовый",L"Пастельный бирюзовый"};
bool sampling=false;int sampleTarget=0;COLORREF sampleColor=RGB(255,255,255);
struct Control{HWND h;int page;bool surface;};std::vector<Control> controls;std::map<int,bool> checks;
COLORREF panel=RGB(248,248,250),ink=RGB(39,41,47),muted=RGB(126,128,137),accent=RGB(0,122,255);
int X(int x,int width){return i18n::RTL()?822-x-width:x;}
int P(int n){return MulDiv(n,(int)GetDpiForWindow(window),96);}
Color C(COLORREF c){return Color(255,GetRValue(c),GetGValue(c),GetBValue(c));}
void Round(Graphics&g,RectF r,float radius,Color color){GraphicsPath path;float d=radius*2;path.AddArc(r.X,r.Y,d,d,180,90);path.AddArc(r.GetRight()-d,r.Y,d,d,270,90);path.AddArc(r.GetRight()-d,r.GetBottom()-d,d,d,0,90);path.AddArc(r.X,r.GetBottom()-d,d,d,90,90);path.CloseFigure();SolidBrush b(color);g.FillPath(&b,&path);}
// Native trackbars invalidate only the old/new native thumb bounds. Our round
// thumb extends past those bounds, so paint the whole control as one buffered frame.
void PaintSlider(HWND h,HDC destination){
 RECT r;GetClientRect(h,&r);if(r.right<=0||r.bottom<=0)return;
 HDC buffer=CreateCompatibleDC(destination);HBITMAP bitmap=CreateCompatibleBitmap(destination,r.right,r.bottom);
 HGDIOBJ old=SelectObject(buffer,bitmap);FillRect(buffer,&r,(HBRUSH)GetStockObject(WHITE_BRUSH));
 {
  Graphics g(buffer);g.SetSmoothingMode(SmoothingModeAntiAlias);
  RECT channel{},thumb{};SendMessageW(h,TBM_GETCHANNELRECT,0,(LPARAM)&channel);SendMessageW(h,TBM_GETTHUMBRECT,0,(LPARAM)&thumb);
  float centerX=(thumb.left+thumb.right)/2.f,centerY=(channel.top+channel.bottom)/2.f;
  RectF track((float)channel.left,centerY-P(1),(float)(channel.right-channel.left),(float)P(3));
  Round(g,track,track.Height/2,Color(255,223,225,232));
  float filled=std::clamp(centerX-track.X,0.f,track.Width);
  if(filled>0)Round(g,RectF(track.X,track.Y,filled,track.Height),std::min(track.Height,filled)/2,C(accent));
  float diameter=(float)P(14),x=centerX-diameter/2,y=centerY-diameter/2;
  SolidBrush white(Color(255,255,255,255));g.FillEllipse(&white,x,y,diameter,diameter);
  Pen border(Color(255,182,186,197),1);g.DrawEllipse(&border,x+.5f,y+.5f,diameter-1,diameter-1);
 }
 BitBlt(destination,0,0,r.right,r.bottom,buffer,0,0,SRCCOPY);
 SelectObject(buffer,old);DeleteObject(bitmap);DeleteDC(buffer);
}
LRESULT CALLBACK SliderProc(HWND h,UINT message,WPARAM w,LPARAM l,UINT_PTR id,DWORD_PTR){
 if(message==WM_PAINT){InvalidateRect(h,nullptr,FALSE);PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);PaintSlider(h,dc);EndPaint(h,&ps);return 0;}
 if(message==WM_PRINTCLIENT){PaintSlider(h,(HDC)w);return 0;}
 if(message==WM_ERASEBKGND)return 1;
 if(message==WM_NCDESTROY)RemoveWindowSubclass(h,SliderProc,id);
 return DefSubclassProc(h,message,w,l);
}
void Write(HDC dc,const std::wstring&t,RECT r,HFONT f,COLORREF c,UINT flags=DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS){auto old=SelectObject(dc,f);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,c);flags|=DT_NOPREFIX;auto translated=i18n::Detail(t);if(i18n::RTL()){flags|=DT_RTLREADING;if(!(flags&DT_CENTER))flags=(flags&~DT_RIGHT)|DT_RIGHT;}DrawTextW(dc,translated.c_str(),-1,&r,flags);SelectObject(dc,old);}
void TextAt(HDC dc,const std::wstring&t,int x,int y,int w,int h,HFONT f,COLORREF c,UINT flags=DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS){x=X(x,w);RECT r{P(x),P(y),P(x+w),P(y+h)};Write(dc,t,r,f,c,flags);}
HWND Add(const wchar_t*cls,const wchar_t*text,DWORD style,int id,int x,int y,int w,int h,int tab,bool surface=true){if(wcscmp(cls,L"STATIC")==0){style|=SS_NOPREFIX;if(h<=26)style|=SS_CENTERIMAGE;}auto translated=wcscmp(cls,L"EDIT")==0?std::wstring(text):i18n::T(text);DWORD ex=i18n::RTL()&&id!=IQ_KEY&&id!=ACCU_KEY?WS_EX_RTLREADING:0;if(i18n::RTL()&&wcscmp(cls,L"STATIC")==0)style|=SS_RIGHT;if(i18n::RTL()&&id==CITY)style|=ES_RIGHT;HWND c=CreateWindowExW(ex,cls,translated.c_str(),WS_CHILD|WS_VISIBLE|style,P(X(x,w)),P(y),P(w),P(h),window,(HMENU)(INT_PTR)id,instance,nullptr);SendMessageW(c,WM_SETFONT,(WPARAM)font,TRUE);controls.push_back({c,tab,surface});if(wcscmp(cls,TRACKBAR_CLASSW)==0)SetWindowSubclass(c,SliderProc,1,0);return c;}
void Label(const wchar_t*text,int x,int y,int w,int tab,bool secondary=false,int h=24,bool surface=true){HWND c=Add(L"STATIC",text,0,0,x,y,w,h,tab,surface);if(secondary)SendMessageW(c,WM_SETFONT,(WPARAM)smallFont,TRUE);SetWindowLongPtrW(c,GWLP_USERDATA,secondary);}
HWND Button(int id,const wchar_t*text,int x,int y,int w,int tab){return Add(L"BUTTON",text,BS_OWNERDRAW|WS_TABSTOP,id,x,y,w,30,tab,false);}
void Palette(bool lock,int y,int tab){int first=lock?LOCK_SWATCH:TASK_SWATCH;for(int i=0;i<6;++i)Add(L"BUTTON",presetNames[i],BS_OWNERDRAW|WS_TABSTOP,first+i,236+i*40,y,28,28,tab);Add(L"BUTTON",L"Другой цвет",BS_OWNERDRAW|WS_TABSTOP,lock?LOCK_COLOR:TASK_COLOR,500,y-1,30,30,tab);Add(L"BUTTON",L"Пипетка",BS_OWNERDRAW|WS_TABSTOP,lock?LOCK_PICK:TASK_PICK,548,y-1,30,30,tab);}
void Toggle(int id,const wchar_t*text,bool value,int y,int tab){checks[id]=value;Label(text,232,y,420,tab);Add(L"BUTTON",text,BS_OWNERDRAW|WS_TABSTOP,id,736,y-2,42,28,tab);}
bool Checked(int id){return checks[id];}
void SetText(int id,const std::wstring&s){auto t=id==CITY?s:i18n::Detail(s);SetDlgItemTextW(window,id,t.c_str());}
std::wstring Text(int id){HWND h=GetDlgItem(window,id);int n=GetWindowTextLengthW(h);std::wstring t(n+1,0);GetWindowTextW(h,t.data(),n+1);t.resize(n);return t;}
void WeatherControls(){bool accu=draft.weatherSource==1;ShowWindow(GetDlgItem(window,ACCU_KEY),page==2&&accu?SW_SHOW:SW_HIDE);ShowWindow(GetDlgItem(window,ACCU_LINK),page==2&&accu?SW_SHOW:SW_HIDE);ShowWindow(GetDlgItem(window,WEATHER_HINT),page==2&&!accu?SW_SHOW:SW_HIDE);}
void LocationControls(){bool busy=locationJob&&!locationJob->done.load(std::memory_order_acquire);EnableWindow(GetDlgItem(window,CITY),!draft.locationFromSystem&&!busy);EnableWindow(GetDlgItem(window,LOCATION_FIND),!busy);EnableWindow(GetDlgItem(window,LOCATION_MODE),!busy);SetText(LOCATION_FIND,busy?L"Поиск…":draft.locationFromSystem?L"Определить":L"Найти");}
void Page(int n){page=n;for(auto&c:controls)ShowWindow(c.h,c.page<0||c.page==n?SW_SHOW:SW_HIDE);WeatherControls();LocationControls();for(auto&c:controls)InvalidateRect(c.h,nullptr,TRUE);InvalidateRect(window,nullptr,TRUE);}
void Scale(){SetText(700,std::to_wstring((int)SendDlgItemMessageW(window,LOCK_SCALE,TBM_GETPOS,0,0))+L"%");SetText(701,std::to_wstring((int)SendDlgItemMessageW(window,BATTERY,TBM_GETPOS,0,0))+i18n::T(L" мин"));if(preview)InvalidateRect(preview,nullptr,FALSE);}
LRESULT CALLBACK PreviewProc(HWND h,UINT m,WPARAM w,LPARAM l){if(m!=WM_PAINT)return DefWindowProcW(h,m,w,l);PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);RECT r;GetClientRect(h,&r);Graphics g(dc);g.SetSmoothingMode(SmoothingModeAntiAlias);LinearGradientBrush bg(Point(0,0),Point(r.right,r.bottom),Color(255,47,78,143),Color(255,17,27,62));g.FillRectangle(&bg,0,0,r.right,r.bottom);SolidBrush haze(Color(22,255,255,255));g.FillEllipse(&haze,(int)-r.right/4,(int)r.bottom/3,(int)r.right,(int)r.bottom*2);
 RECT caption{P(18),P(12),r.right-P(18),P(36)};Write(dc,L"Локскрин · фрагмент",caption,smallFont,RGB(222,229,248));
 if(Checked(LOCK_ON)){auto list=previewItems;list.erase(std::remove_if(list.begin(),list.end(),[](const WidgetItem&i){return i.type==WidgetType::Date?!Checked(SHOW_DATE):i.type==WidgetType::Battery?!Checked(SHOW_BATTERY):i.type==WidgetType::Weather?!Checked(SHOW_WEATHER):(!Checked(SHOW_AIR)||Text(IQ_KEY).empty());}),list.end());if(Checked(SHOW_DATE)&&std::none_of(list.begin(),list.end(),[](const WidgetItem&i){return i.type==WidgetType::Date;})){WidgetItem date;date.type=WidgetType::Date;date.text=appdate::Current();list.push_back(date);}if(!list.empty()){int scale=(int)SendDlgItemMessageW(window,LOCK_SCALE,TBM_GETPOS,0,0);auto img=widget::RenderStrip(list,MulDiv(P(144),scale,100),false,appearance::LockscreenColor(draft.taskbarColor));Bitmap b(img.width,img.height,img.width*4,PixelFormat32bppPARGB,(BYTE*)img.pixels.data());float factor=std::min(1.f,(r.right-P(32))/(float)img.width);g.DrawImage(&b,(r.right-img.width*factor)/2,(r.bottom-img.height*factor)/2+P(12),img.width*factor,img.height*factor);}}else{RECT text{0,P(52),r.right,r.bottom-P(14)};Write(dc,L"Виджет выключен",text,font,RGB(222,229,248),DT_CENTER|DT_VCENTER|DT_SINGLELINE);}
 EndPaint(h,&ps);return 0;}
void CreateControls(){
 int dpi=GetDpiForWindow(window);auto make=[&](int size,int weight){return typography::Font(MulDiv(size,dpi,96),weight);};font=make(13,FW_NORMAL);smallFont=make(11,FW_NORMAL);semibold=make(13,FW_MEDIUM);titleFont=make(22,FW_SEMIBOLD);
 Button(NAV_GENERAL,L"Виджеты",14,146,172,-1);Button(NAV_LOCK,L"Экран блокировки",14,188,172,-1);Button(NAV_SERVICES,L"Город и сервисы",14,230,172,-1);Button(NAV_ABOUT,L"О программе",14,272,172,-1);
 Toggle(SHOW_BATTERY,L"Батареи устройств",draft.showBattery,122,0);Toggle(SHOW_WEATHER,L"Погода",draft.showWeather,160,0);Toggle(SHOW_AIR,L"Качество воздуха",draft.showAir,198,0);Toggle(SHOW_DATE,L"Дата",draft.showDate,236,0);
 Label(L"Опрос батарей",232,304,360,0);Add(L"STATIC",L"",SS_RIGHT,701,684,304,92,22,0);HWND battery=Add(TRACKBAR_CLASSW,L"",TBS_NOTICKS|WS_TABSTOP,BATTERY,226,334,556,30,0);SendMessageW(battery,TBM_SETRANGE,TRUE,MAKELONG(1,60));SendMessageW(battery,TBM_SETPOS,TRUE,draft.batteryMinutes);Label(L"1 минута",232,366,160,0,true);Label(L"1 час",672,366,104,0,true);
 Label(L"Цвет виджетов",236,434,400,0);Button(TASK_AUTO,L"Авто",704,430,72,0);Palette(false,478,0);
 Label(L"Общий цвет для панели задач, трея и экрана блокировки.",216,548,582,0,true,24,false);
 Label(L"IQAir обновляется раз в 15 минут. Погода — раз в час.",216,576,582,0,true,24,false);
 Toggle(LOCK_ON,L"Показывать виджет на локскрине",draft.lockscreenEnabled,122,1);
 WNDCLASSW wc{};wc.lpfnWndProc=PreviewProc;wc.hInstance=instance;wc.lpszClassName=L"WirelessBatteryPreview";RegisterClassW(&wc);preview=Add(wc.lpszClassName,L"",0,0,216,182,582,168,1,false);SetWindowRgn(preview,CreateRoundRectRgn(0,0,P(582)+1,P(168)+1,P(24),P(24)),FALSE);
 Label(L"Размер",232,384,320,1);Add(L"STATIC",L"",SS_RIGHT,700,684,384,92,22,1);HWND size=Add(TRACKBAR_CLASSW,L"",TBS_NOTICKS|WS_TABSTOP,LOCK_SCALE,226,414,556,30,1);SendMessageW(size,TBM_SETRANGE,TRUE,MAKELONG(50,200));SendMessageW(size,TBM_SETPOS,TRUE,draft.lockscreenScale);Button(RESET_SIZE,L"Сбросить до 100%",232,454,170,1);
 Label(L"Цвет виджетов",236,512,400,1);Button(LOCK_AUTO,L"Авто",704,508,72,1);Palette(true,554,1);

 Label(L"Город",232,120,220,2);Button(LOCATION_MODE,draft.locationFromSystem?L"Геолокация Windows":L"Ввести вручную",500,116,274,2);
 HWND city=Add(L"EDIT",draft.city.c_str(),ES_AUTOHSCROLL|WS_TABSTOP,CITY,244,164,398,24,2);SendMessageW(city,EM_SETCUEBANNER,FALSE,(LPARAM)i18n::T(L"Введите название города").c_str());SendMessageW(city,EM_SETLIMITTEXT,100,0);Button(LOCATION_FIND,draft.locationFromSystem?L"Определить":L"Найти",668,160,108,2);
 HWND locStatus=Add(L"STATIC",draft.locationFromSystem?L"Местоположение Windows":L"Выберите город по названию · данные GeoNames / Open-Meteo",SS_ENDELLIPSIS,LOCATION_STATUS,216,208,582,18,2,false);SendMessageW(locStatus,WM_SETFONT,(WPARAM)smallFont,TRUE);SetWindowLongPtrW(locStatus,GWLP_USERDATA,1);

 Label(L"IQAir",232,248,330,2);HWND iq=Add(L"EDIT",draft.iqairKey.c_str(),ES_AUTOHSCROLL|ES_PASSWORD|WS_TABSTOP,IQ_KEY,244,286,520,24,2);SendMessageW(iq,EM_SETLIMITTEXT,512,0);Add(WC_LINK,L"<a>Получить API-ключ для IQAir ↗</a>",WS_TABSTOP,IQ_LINK,232,328,540,24,2);
 Label(L"Погода",232,398,200,2);Button(WEATHER_SOURCE,draft.weatherSource==0?L"Open-Meteo · без ключа":L"AccuWeather · API-ключ",488,394,286,2);HWND accu=Add(L"EDIT",draft.accuweatherKey.c_str(),ES_AUTOHSCROLL|ES_PASSWORD|WS_TABSTOP,ACCU_KEY,244,436,520,24,2);SendMessageW(accu,EM_SETLIMITTEXT,512,0);Add(WC_LINK,L"<a>Получить API-ключ AccuWeather ↗</a>",WS_TABSTOP,ACCU_LINK,232,478,540,24,2);

 Label(L"Ключи зашифрованы Windows и остаются на этом компьютере.",216,552,582,2,true,38,false);
 Label(L"Язык программы",232,126,240,3);std::wstring languageName=L"Как в системе";for(const auto&lang:i18n::Languages())if(draft.language==lang.code)languageName=lang.name;Button(LANGUAGE,languageName.c_str(),488,122,286,3);
 Label(L"Язык меняется сразу, без перезапуска.",232,171,540,3,true,40);
 Label(L"Wedgu",232,258,360,3);Label((i18n::T(L"Версия")+L" 1.1.2").c_str(),232,294,540,3,true);
 Label(L"Автор и исходный код",232,346,540,3);Add(WC_LINK,L"<a>github.com/unioneon ↗</a>",WS_TABSTOP,GITHUB_LINK,232,382,540,28,3);
 Label(L"На основе Wireless Device Battery Monitor · GPL-3.0",232,458,540,3,true,38);auto upstream=L"<a>"+i18n::T(L"Оригинальный проект ↗")+L"</a>";Add(WC_LINK,upstream.c_str(),WS_TABSTOP,UPSTREAM_LINK,232,508,540,28,3);
 Button(CANCEL,L"Закрыть",582,620,98,-1);Button(SAVE,L"Применить",694,620,104,-1);Scale();Page(0);
}
void Card(Graphics&g,int x,int y,int w,int h){x=X(x,w);Round(g,RectF((float)P(x),(float)P(y),(float)P(w),(float)P(h)),(float)P(11),Color(255,255,255,255));}
void Paint(HWND h){PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);RECT r;GetClientRect(h,&r);HDC mem=CreateCompatibleDC(dc);HBITMAP bitmap=CreateCompatibleBitmap(dc,r.right,r.bottom);auto old=SelectObject(mem,bitmap);HBRUSH background=CreateSolidBrush(panel);FillRect(mem,&r,background);DeleteObject(background);Graphics g(mem);g.SetSmoothingMode(SmoothingModeAntiAlias);SolidBrush side(Color(255,242,243,246));g.FillRectangle(&side,P(X(0,200)),0,P(200),r.bottom);Pen divider(Color(255,232,233,238));g.DrawLine(&divider,P(i18n::RTL()?622:200),0,P(i18n::RTL()?622:200),r.bottom);
 branding::DrawMark(g,RectF((float)P(X(22,34)),(float)P(31),(float)P(34),(float)P(25)));g.Flush();TextAt(mem,L"Wedgu",66,24,120,38,titleFont,ink);TextAt(mem,L"Настройки",22,112,160,20,smallFont,muted);
 const wchar_t*titles[]={L"Виджеты",L"Экран блокировки",L"Город и сервисы",L"О программе"};TextAt(mem,titles[page],216,28,582,34,titleFont,ink);TextAt(mem,page==0?L"Всё важное — рядом с часами.":page==1?L"Размер и цвет виджета · исходный фон сохранится.":page==2?L"Местоположение и источники данных.":L"Wedgu",216,69,582,22,smallFont,muted);
 if(page==0){Card(g,216,106,582,170);Card(g,216,292,582,106);Card(g,216,418,582,112);Pen line(Color(255,240,240,243));g.DrawLine(&line,P(232),P(153),P(782),P(153));g.DrawLine(&line,P(232),P(191),P(782),P(191));g.DrawLine(&line,P(232),P(229),P(782),P(229));}
 if(page==1){Card(g,216,106,582,56);Card(g,216,368,582,224);}
 if(page==2){Card(g,216,106,582,100);Round(g,RectF((float)P(X(232,420)),(float)P(156),(float)P(420),(float)P(40)),(float)P(7),Color(255,245,245,248));Card(g,216,230,582,134);Card(g,216,380,582,134);Round(g,RectF((float)P(X(232,548)),(float)P(278),(float)P(548),(float)P(40)),(float)P(7),Color(255,245,245,248));if(draft.weatherSource==1)Round(g,RectF((float)P(X(232,548)),(float)P(428),(float)P(548),(float)P(40)),(float)P(7),Color(255,245,245,248));else{g.Flush();TextAt(mem,L"Без регистрации и ключа. Обновление раз в час.\nИсточник: Open-Meteo · CC BY 4.0",236,436,536,58,font,muted,DT_LEFT|DT_WORDBREAK);}}
 if(page==3){Card(g,216,106,582,112);Card(g,216,238,582,182);Card(g,216,440,582,112);}
 TextAt(mem,L"Wedgu 1.1.2",20,616,174,24,smallFont,muted);
 std::wstring line=sampling?L"Пипетка: нажмите на цвет · Esc — отмена":currentStatus.empty()?L"Подключаем устройства…":currentStatus;if(page==3)line=L"GPL-3.0 · unioneon";if(page==2){auto start=line.find(L"Погода:");if(start!=std::wstring::npos)line=line.substr(start);}else if(page==1){auto start=line.find(L"Локскрин:");if(start!=std::wstring::npos)line=line.substr(start);}auto cut=line.find(L'\n');if(cut!=std::wstring::npos)line.resize(cut);TextAt(mem,line,216,616,350,34,smallFont,muted);
 BitBlt(dc,0,0,r.right,r.bottom,mem,0,0,SRCCOPY);SelectObject(mem,old);DeleteObject(bitmap);DeleteDC(mem);EndPaint(h,&ps);}
void DrawButton(DRAWITEMSTRUCT*d){HDC dc=d->hDC;RECT r=d->rcItem;int id=d->CtlID;COLORREF base=(id>=NAV_GENERAL&&id<=NAV_ABOUT)?RGB(242,243,246):(id==CANCEL||id==SAVE)?panel:RGB(255,255,255);HBRUSH brush=CreateSolidBrush(base);FillRect(dc,&r,brush);DeleteObject(brush);Graphics g(dc);g.SetSmoothingMode(SmoothingModeAntiAlias);bool down=d->itemState&ODS_SELECTED;bool focus=d->itemState&ODS_FOCUS;auto rect=RectF((float)r.left,(float)r.top,(float)(r.right-r.left),(float)(r.bottom-r.top));
 int swatchIndex=id>=TASK_SWATCH&&id<TASK_SWATCH+6?id-TASK_SWATCH:id>=LOCK_SWATCH&&id<LOCK_SWATCH+6?id-LOCK_SWATCH:-1;
 if(swatchIndex>=0){bool lock=id>=LOCK_SWATCH;COLORREF selected=lock?draft.lockscreenColor:draft.taskbarColor;SolidBrush b(C(presets[swatchIndex]));g.FillEllipse(&b,P(3),P(3),P(22),P(22));Pen border(Color(255,204,207,215),1);g.DrawEllipse(&border,P(3),P(3),P(22),P(22));if(selected==presets[swatchIndex]){Pen ring(C(accent),(float)P(2));g.DrawEllipse(&ring,P(1),P(1),P(26),P(26));}return;}
 if(id==TASK_COLOR||id==LOCK_COLOR){LinearGradientBrush gradient(Point(0,0),Point(P(30),P(30)),Color(255,244,193,214),Color(255,139,210,223));Color colors[]={Color(255,247,215,150),Color(255,237,180,204),Color(255,164,176,239),Color(255,141,220,208)};REAL stops[]={0,.33f,.66f,1};gradient.SetInterpolationColors(colors,stops,4);g.FillEllipse(&gradient,P(3),P(3),P(24),P(24));Pen plus(Color(255,255,255,255),(float)P(1));g.DrawLine(&plus,P(10),P(15),P(20),P(15));g.DrawLine(&plus,P(15),P(10),P(15),P(20));return;}
 if(id==TASK_PICK||id==LOCK_PICK){Pen pen(C(ink),1.4f*P(1));pen.SetStartCap(LineCapRound);pen.SetEndCap(LineCapRound);g.DrawLine(&pen,P(11),P(21),P(21),P(11));g.DrawLine(&pen,P(9),P(18),P(14),P(23));g.DrawLine(&pen,P(17),P(9),P(23),P(15));g.DrawLine(&pen,P(9),P(22),P(7),P(24));g.DrawLine(&pen,P(20),P(9),P(22),P(7));return;}
 if(checks.count(id)){
   bool on=checks[id];float inset=(float)P(2);RectF track(rect.X+inset,rect.Y+inset,rect.Width-2*inset,rect.Height-2*inset);
   Round(g,track,track.Height/2,on?C(accent):Color(255,210,211,217));
   float margin=(float)P(3),diameter=track.Height-2*margin;float x=on?track.GetRight()-margin-diameter:track.X+margin;
   SolidBrush knob(Color(255,255,255,255));g.FillEllipse(&knob,x,track.Y+margin,diameter,diameter);
   if(focus&&!(d->itemState&ODS_NOFOCUSRECT)){RECT ring=r;InflateRect(&ring,-P(1),-P(1));DrawFocusRect(dc,&ring);}return;
 }

 bool nav=id>=NAV_GENERAL&&id<=NAV_ABOUT,selected=nav&&page==id-NAV_GENERAL,primary=id==SAVE;bool automatic=(id==TASK_AUTO||id==LOCK_AUTO)&&draft.taskbarColor==CLR_INVALID;
 COLORREF bg=nav?(selected?RGB(225,227,234):RGB(242,243,246)):primary?accent:RGB(230,230,235);if(automatic)bg=RGB(229,239,252);if(down)bg=primary?RGB(0,101,218):RGB(215,218,226);Round(g,rect,(float)P(7),C(bg));wchar_t text[120]{};GetWindowTextW(d->hwndItem,text,120);RECT tr=r;if(nav||id==WEATHER_SOURCE||id==LOCATION_MODE||id==LANGUAGE){int arrowPadding=(id==WEATHER_SOURCE||id==LOCATION_MODE||id==LANGUAGE)?34:12;tr.left+=P(i18n::RTL()?arrowPadding:12);tr.right-=P(i18n::RTL()?12:arrowPadding);}Write(dc,text,tr,selected||primary?semibold:font,(d->itemState&ODS_DISABLED)?RGB(150,153,163):primary?RGB(255,255,255):automatic?accent:ink,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|((nav||id==WEATHER_SOURCE)?DT_LEFT:DT_CENTER));if(id==WEATHER_SOURCE||id==LOCATION_MODE||id==LANGUAGE){Pen arrow(C(muted),1.3f);float x=(float)(i18n::RTL()?P(18):r.right-P(18)),y=(float)(r.top+P(13));g.DrawLine(&arrow,x-P(3),y,x,y+P(3));g.DrawLine(&arrow,x,y+P(3),x+P(3),y);}if(focus&&!(d->itemState&ODS_NOFOCUSRECT)){RECT ring=r;InflateRect(&ring,-P(4),-P(4));DrawFocusRect(dc,&ring);}}
void RedrawColors(){for(auto&c:controls){int id=GetDlgCtrlID(c.h);if((id>=TASK_SWATCH&&id<LOCK_SWATCH+6)||id==TASK_AUTO||id==LOCK_AUTO)InvalidateRect(c.h,nullptr,FALSE);}InvalidateRect(preview,nullptr,FALSE);}
void SetColor(bool,COLORREF color){std::wstring error;if(!settings::SaveColors(color,error)){i18n::Message(window,error.c_str(),L"Цвет",MB_ICONERROR);return;}draft.taskbarColor=color;draft.lockscreenColor=color;RedrawColors();SendMessageW(owner,settingsui::ColorsChanged,(WPARAM)color,0);}
void ChooseColor(bool lock){colorpicker::Show(instance,window,COLOR_CHOSEN,COLOR_SAMPLE,lock?1:0,lock?draft.lockscreenColor:draft.taskbarColor);}
void Sample(bool lock){colorpicker::Close();sampleTarget=lock?1:0;sampling=true;SetCapture(window);SetCursor(LoadCursorW(nullptr,IDC_CROSS));InvalidateRect(window,nullptr,FALSE);}
COLORREF ScreenPixel(){POINT p;GetCursorPos(&p);HDC screen=GetDC(nullptr);COLORREF color=GetPixel(screen,p.x,p.y);ReleaseDC(nullptr,screen);return color;}

bool Save();
void StartLookup(){
 if(locationJob&&!locationJob->done.load(std::memory_order_acquire))return;
 auto query=Text(CITY);if(!draft.locationFromSystem&&query.size()<2){SetText(LOCATION_STATUS,L"Введите название города (не менее 2 букв).");pendingSave=false;return;}
 locationReady=false;cityMatches.clear();locationJob=location::Start(query,draft.locationFromSystem);LocationControls();SetText(LOCATION_STATUS,draft.locationFromSystem?L"Определяем местоположение через Windows…":L"Ищем город…");SetTimer(window,LOCATION_TIMER,100,nullptr);
}
void ChoosePlace(const environment::Place& place){draft.city=place.name;draft.latitude=place.latitude;draft.longitude=place.longitude;resolvedCity=place.name;SetText(CITY,place.name);locationReady=true;SetText(LOCATION_STATUS,place.detail.empty()?place.name:place.name+L" · "+place.detail);LocationControls();bool save=pendingSave;pendingSave=false;if(save)Save();}
void FinishLookup(){
 if(!locationJob||!locationJob->done.load(std::memory_order_acquire))return;
 KillTimer(window,LOCATION_TIMER);auto result=locationJob;locationJob.reset();LocationControls();
 if(!result->error.empty()){SetText(LOCATION_STATUS,result->error);pendingSave=false;return;}
 cityMatches=result->places;if(cityMatches.empty()){SetText(LOCATION_STATUS,L"Город не найден. Уточните название.");pendingSave=false;return;}
 if(cityMatches.size()==1){ChoosePlace(cityMatches.front());return;}
 SetText(LOCATION_STATUS,L"Выберите подходящий город из списка.");std::vector<popup::Row> rows;for(size_t i=0;i<cityMatches.size();++i)rows.push_back({LOCATION_FIRST+(UINT)i,cityMatches[i].name,cityMatches[i].detail});popup::ShowAnchored(instance,window,GetDlgItem(window,CITY),L"Выберите город",rows);
}
bool Save(){
 if(!locationReady||(!draft.locationFromSystem&&_wcsicmp(Text(CITY).c_str(),resolvedCity.c_str())!=0)){pendingSave=true;StartLookup();return false;}
 draft.city=resolvedCity;draft.showDate=Checked(SHOW_DATE);
draft.iqairKey=Text(IQ_KEY);draft.accuweatherKey=Text(ACCU_KEY);draft.batteryMinutes=(int)SendDlgItemMessageW(window,BATTERY,TBM_GETPOS,0,0);draft.lockscreenScale=(int)SendDlgItemMessageW(window,LOCK_SCALE,TBM_GETPOS,0,0);draft.showBattery=Checked(SHOW_BATTERY);draft.showWeather=Checked(SHOW_WEATHER);draft.showAir=Checked(SHOW_AIR);draft.lockscreenEnabled=Checked(LOCK_ON);std::wstring error;if(!settings::Save(draft,error)){i18n::Message(window,error.c_str(),L"Настройки",MB_ICONWARNING);return false;}PostMessageW(owner,applied,0,0);return true;}
void ChangeLanguage(const std::wstring& language){
 std::wstring error;if(!settings::SaveLanguage(language,error)){i18n::Message(window,error,L"Язык программы",MB_ICONERROR);return;}
 // Snapshot edits without saving them. Language is the only immediately persisted preference.
 auto city=Text(CITY);draft.iqairKey=Text(IQ_KEY);draft.accuweatherKey=Text(ACCU_KEY);draft.batteryMinutes=(int)SendDlgItemMessageW(window,BATTERY,TBM_GETPOS,0,0);draft.lockscreenScale=(int)SendDlgItemMessageW(window,LOCK_SCALE,TBM_GETPOS,0,0);draft.showBattery=Checked(SHOW_BATTERY);draft.showWeather=Checked(SHOW_WEATHER);draft.showAir=Checked(SHOW_AIR);draft.showDate=Checked(SHOW_DATE);draft.lockscreenEnabled=Checked(LOCK_ON);draft.language=language;
 KillTimer(window,LOCATION_TIMER);locationJob.reset();cityMatches.clear();pendingSave=false;colorpicker::Close();sampling=false;if(GetCapture()==window)ReleaseCapture();
 for(auto&control:controls){DestroyWindow(control.h);}controls.clear();checks.clear();preview=nullptr;DeleteObject(font);DeleteObject(smallFont);DeleteObject(titleFont);DeleteObject(semibold);
 i18n::Configure(language);SetWindowTextW(window,i18n::T(L"Wedgu — Настройки").c_str());CreateControls();SetText(CITY,city);Page(3);SendMessageW(owner,settingsui::LanguageChanged,0,0);
}
LRESULT CALLBACK Proc(HWND h,UINT m,WPARAM w,LPARAM l){switch(m){
 case WM_CREATE:window=h;CreateControls();return 0;
 case COLOR_CHOSEN:SetColor(w!=0,(COLORREF)l);return 0;
 case COLOR_SAMPLE:Sample(w!=0);return 0;
 case WM_SETCURSOR:if(sampling){SetCursor(LoadCursorW(nullptr,IDC_CROSS));return TRUE;}break;
 case WM_MOUSEMOVE:if(sampling){sampleColor=ScreenPixel();SetCursor(LoadCursorW(nullptr,IDC_CROSS));return 0;}break;
 case WM_LBUTTONDOWN:if(sampling)return 0;break;
 case WM_LBUTTONUP:if(sampling){sampleColor=ScreenPixel();sampling=false;ReleaseCapture();if(sampleColor!=CLR_INVALID)SetColor(sampleTarget!=0,sampleColor);InvalidateRect(h,nullptr,FALSE);return 0;}break;
 case WM_CAPTURECHANGED:if(sampling){sampling=false;InvalidateRect(h,nullptr,FALSE);}return 0;
 case WM_PAINT:Paint(h);return 0;case WM_ERASEBKGND:return 1;
 case WM_DRAWITEM:DrawButton((DRAWITEMSTRUCT*)l);return TRUE;
 case WM_HSCROLL:Scale();return 0;
 case WM_TIMER:if(w==LOCATION_TIMER){FinishLookup();return 0;}break;
 case WM_NOTIFY:{auto*n=(NMHDR*)l;

if((n->code==NM_CLICK||n->code==NM_RETURN)&&(n->idFrom==GITHUB_LINK||n->idFrom==UPSTREAM_LINK)){ShellExecuteW(h,L"open",n->idFrom==GITHUB_LINK?L"https://github.com/unioneon/":L"https://github.com/krisgfx9/Wireless-Device-Battery-Monitor",nullptr,nullptr,SW_SHOWNORMAL);return 0;}
if((n->code==NM_CLICK||n->code==NM_RETURN)&&(n->idFrom==IQ_LINK||n->idFrom==ACCU_LINK)){ShellExecuteW(h,L"open",n->idFrom==IQ_LINK?providers::IQAirKeys:providers::AccuWeatherKeys,nullptr,nullptr,SW_SHOWNORMAL);}
 return 0;}
 case WM_COMMAND:{int id=LOWORD(w);if(id==LANGUAGE){std::vector<popup::Row> rows;auto& langs=i18n::Languages();for(size_t i=0;i<langs.size();++i)rows.push_back({LANGUAGE_FIRST+(UINT)i,langs[i].name,L"",false,draft.language==langs[i].code});popup::ShowAnchored(instance,h,GetDlgItem(h,LANGUAGE),L"Язык программы",rows);return 0;}if(id>=LANGUAGE_FIRST&&id<LANGUAGE_FIRST+(int)i18n::Languages().size()){ChangeLanguage(i18n::Languages()[id-LANGUAGE_FIRST].code);return 0;}if(id==CITY&&HIWORD(w)==EN_CHANGE){locationReady=_wcsicmp(Text(CITY).c_str(),resolvedCity.c_str())==0;return 0;}if(id==LOCATION_FIND){pendingSave=false;StartLookup();return 0;}if(id==LOCATION_MODE){popup::ShowAnchored(instance,h,GetDlgItem(h,LOCATION_MODE),L"Местоположение",{{LOCATION_MANUAL,L"Ввести город вручную",L"Поиск по названию",false,!draft.locationFromSystem},{LOCATION_AUTO,L"Геолокация Windows",L"По данным системы",false,draft.locationFromSystem}});return 0;}if(id==LOCATION_MANUAL||id==LOCATION_AUTO){draft.locationFromSystem=id==LOCATION_AUTO;SetText(LOCATION_MODE,draft.locationFromSystem?L"Геолокация Windows":L"Ввести вручную");locationReady=!draft.locationFromSystem&&_wcsicmp(Text(CITY).c_str(),resolvedCity.c_str())==0;LocationControls();InvalidateRect(GetDlgItem(h,LOCATION_MODE),nullptr,FALSE);pendingSave=false;if(draft.locationFromSystem)StartLookup();return 0;}if(id>=LOCATION_FIRST&&id<LOCATION_FIRST+(int)cityMatches.size()){ChoosePlace(cityMatches[id-LOCATION_FIRST]);return 0;}if(id>=TASK_SWATCH&&id<TASK_SWATCH+6){SetColor(false,presets[id-TASK_SWATCH]);return 0;}if(id>=LOCK_SWATCH&&id<LOCK_SWATCH+6){SetColor(true,presets[id-LOCK_SWATCH]);return 0;}if(id==WEATHER_SOURCE){popup::ShowAnchored(instance,h,GetDlgItem(h,WEATHER_SOURCE),L"Источник погоды",{{WEATHER_OPEN,L"Open-Meteo",L"Без регистрации и API-ключа",false,draft.weatherSource==0},{WEATHER_ACCU,L"AccuWeather",L"Потребуется свой API-ключ",false,draft.weatherSource==1}});return 0;}if(id==WEATHER_OPEN||id==WEATHER_ACCU){draft.weatherSource=id==WEATHER_OPEN?0:1;SetText(WEATHER_SOURCE,draft.weatherSource==0?L"Open-Meteo · без ключа":L"AccuWeather · API-ключ");WeatherControls();InvalidateRect(GetDlgItem(h,WEATHER_SOURCE),nullptr,FALSE);InvalidateRect(h,nullptr,FALSE);return 0;}if(id>=NAV_GENERAL&&id<=NAV_ABOUT){Page(id-NAV_GENERAL);return 0;}if(checks.count(id)){bool enabled=!checks[id];if(id==SHOW_DATE){std::wstring error;if(!settings::SaveDate(enabled,error)){i18n::Message(h,error.c_str(),L"Дата",MB_ICONERROR);return 0;}draft.showDate=enabled;}checks[id]=enabled;InvalidateRect((HWND)l,nullptr,FALSE);InvalidateRect(preview,nullptr,FALSE);if(id==SHOW_DATE)SendMessageW(owner,settingsui::DateChanged,enabled?1:0,0);return 0;}switch(id){case CANCEL:DestroyWindow(h);return 0;case SAVE:if(Save()){currentStatus=L"Настройки сохранены";InvalidateRect(h,nullptr,FALSE);}return 0;case RESET_SIZE:SendDlgItemMessageW(h,LOCK_SCALE,TBM_SETPOS,TRUE,100);Scale();return 0;case TASK_COLOR:ChooseColor(false);return 0;case LOCK_COLOR:ChooseColor(true);return 0;case TASK_AUTO:SetColor(false,CLR_INVALID);return 0;case LOCK_AUTO:SetColor(true,CLR_INVALID);return 0;case TASK_PICK:Sample(false);return 0;case LOCK_PICK:Sample(true);return 0;}break;}
 case WM_CTLCOLOREDIT:if(GetDlgCtrlID((HWND)l)==IQ_KEY||GetDlgCtrlID((HWND)l)==ACCU_KEY||GetDlgCtrlID((HWND)l)==CITY){static HBRUSH field=CreateSolidBrush(RGB(245,245,248));SetBkColor((HDC)w,RGB(245,245,248));SetTextColor((HDC)w,ink);return (LRESULT)field;}[[fallthrough]];
 case WM_CTLCOLORSTATIC:case WM_CTLCOLORBTN:{if(GetDlgCtrlID((HWND)l)==CITY){static HBRUSH field=CreateSolidBrush(RGB(245,245,248));SetBkColor((HDC)w,RGB(245,245,248));SetTextColor((HDC)w,muted);return (LRESULT)field;}bool surface=true;for(auto&c:controls)if(c.h==(HWND)l){surface=c.surface;break;}SetBkColor((HDC)w,surface?RGB(255,255,255):panel);SetTextColor((HDC)w,GetWindowLongPtrW((HWND)l,GWLP_USERDATA)?muted:ink);static HBRUSH gray=CreateSolidBrush(panel);return (LRESULT)(surface?GetStockObject(WHITE_BRUSH):gray);}
 case WM_CLOSE:DestroyWindow(h);return 0;
 case WM_DESTROY:KillTimer(h,LOCATION_TIMER);locationJob.reset();cityMatches.clear();colorpicker::Close();sampling=false;if(GetCapture()==h)ReleaseCapture();controls.clear();checks.clear();window=nullptr;DeleteObject(font);DeleteObject(smallFont);DeleteObject(titleFont);DeleteObject(semibold);return 0;
 }return DefWindowProcW(h,m,w,l);}
}
namespace settingsui {
void Show(HINSTANCE inst,HWND parent,UINT msg,const std::vector<WidgetItem>&items,const std::wstring&status){if(window){ShowWindow(window,SW_RESTORE);SetForegroundWindow(window);return;}instance=inst;owner=parent;applied=msg;draft=settings::Load();resolvedCity=draft.city;locationReady=true;pendingSave=false;locationJob.reset();previewItems=items;currentStatus=status;INITCOMMONCONTROLSEX ic{sizeof(ic),ICC_WIN95_CLASSES|ICC_LINK_CLASS};InitCommonControlsEx(&ic);WNDCLASSW wc{};wc.lpfnWndProc=Proc;wc.hInstance=inst;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.lpszClassName=L"WirelessBatterySettings";wc.hIcon=LoadIconW(inst,MAKEINTRESOURCEW(1));RegisterClassW(&wc);UINT dpi=GetDpiForSystem();RECT r{0,0,MulDiv(822,dpi,96),MulDiv(670,dpi,96)};DWORD style=WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|WS_CLIPCHILDREN;AdjustWindowRectExForDpi(&r,style,FALSE,0,dpi);MONITORINFO mi{sizeof(mi)};POINT cursor;GetCursorPos(&cursor);GetMonitorInfoW(MonitorFromPoint(cursor,MONITOR_DEFAULTTONEAREST),&mi);HWND h=CreateWindowExW(0,wc.lpszClassName,i18n::T(L"Wedgu — Настройки").c_str(),style,mi.rcWork.left+(mi.rcWork.right-mi.rcWork.left-r.right+r.left)/2,mi.rcWork.top+std::max(0L,(mi.rcWork.bottom-mi.rcWork.top-r.bottom+r.top)/2),r.right-r.left,r.bottom-r.top,nullptr,nullptr,inst,nullptr);if(h){int round=2;DwmSetWindowAttribute(h,33,&round,sizeof(round));COLORREF caption=panel;DwmSetWindowAttribute(h,35,&caption,sizeof(caption));ShowWindow(h,SW_SHOW);SetForegroundWindow(h);}}
void Update(const std::vector<WidgetItem>&items,const std::wstring&status){previewItems=items;currentStatus=status;if(window){InvalidateRect(preview,nullptr,FALSE);InvalidateRect(window,nullptr,FALSE);}}
void Close(){if(window)DestroyWindow(window);}
bool Translate(MSG*m){if(window&&m->message==WM_KEYDOWN&&m->wParam==VK_RETURN&&GetFocus()==GetDlgItem(window,CITY)){pendingSave=false;StartLookup();return true;}if(colorpicker::Translate(m))return true;if(window&&sampling&&m->message==WM_KEYDOWN&&m->wParam==VK_ESCAPE){sampling=false;ReleaseCapture();InvalidateRect(window,nullptr,FALSE);return true;}return window&&IsDialogMessageW(window,m);}
}

namespace {
struct MenuData {
 HWND parent;
 std::wstring caption;
 std::vector<popup::Row> rows;
 HFONT font,small;
 int hot=-1,first=0,visible=0;
};
LRESULT CALLBACK MenuProc(HWND h,UINT message,WPARAM w,LPARAM l){
 auto* data=(MenuData*)GetWindowLongPtrW(h,GWLP_USERDATA);
 if(message==WM_NCCREATE){data=(MenuData*)((CREATESTRUCTW*)l)->lpCreateParams;SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)data);}
 if(!data)return DefWindowProcW(h,message,w,l);
 int dpi=GetDpiForWindow(h);auto px=[&](int v){return MulDiv(v,dpi,96);};
 auto choose=[&](){if(data->hot<0||data->hot>=(int)data->rows.size())return;auto row=data->rows[data->hot];if(!row.command)return;HWND parent=data->parent;if(!row.stayOpen)DestroyWindow(h);PostMessageW(parent,WM_COMMAND,row.command,0);};
 switch(message){
 case WM_PAINT:{
  PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);RECT r;GetClientRect(h,&r);HDC buffer=CreateCompatibleDC(dc);HBITMAP bitmap=CreateCompatibleBitmap(dc,r.right,r.bottom);auto old=SelectObject(buffer,bitmap);FillRect(buffer,&r,(HBRUSH)GetStockObject(WHITE_BRUSH));
  {Graphics g(buffer);g.SetSmoothingMode(SmoothingModeAntiAlias);
   RECT header{px(18),px(10),r.right-px(18),px(50)};Write(buffer,data->caption,header,data->small,RGB(126,128,140));
   for(int i=data->first;i<std::min((int)data->rows.size(),data->first+data->visible);++i){const auto& row=data->rows[i];int shown=i-data->first;RECT bounds{px(7),px(56+shown*44),r.right-px(7),px(100+shown*44)};
    if((int)i==data->hot&&row.command)Round(g,RectF((float)bounds.left,(float)bounds.top,(float)(bounds.right-bounds.left),(float)(bounds.bottom-bounds.top)),(float)px(8),Color(255,239,241,246));
    g.Flush();RECT text=bounds;text.left=px(i18n::RTL()?40:18);text.right-=px(i18n::RTL()?18:32);
    if(row.detail.empty())Write(buffer,row.title,text,data->font,row.command?RGB(60,64,77):RGB(143,145,154));
    else{RECT title=text;title.top+=px(3);title.bottom=title.top+px(21);Write(buffer,row.title,title,data->font,RGB(60,64,77));RECT detail=text;detail.top+=px(24);detail.bottom-=px(3);Write(buffer,row.detail,detail,data->small,RGB(139,142,152));}
    if(row.checked){RECT check{i18n::RTL()?px(14):r.right-px(34),bounds.top,i18n::RTL()?px(34):r.right-px(14),bounds.bottom};Write(buffer,L"✓",check,data->font,RGB(0,122,255),DT_CENTER|DT_VCENTER|DT_SINGLELINE);}
   }
  }
  BitBlt(dc,0,0,r.right,r.bottom,buffer,0,0,SRCCOPY);SelectObject(buffer,old);DeleteObject(bitmap);DeleteDC(buffer);EndPaint(h,&ps);return 0;
 }
 case WM_ERASEBKGND:return 1;
 case WM_MOUSEMOVE:{int y=(short)HIWORD(l),index=y<px(56)?-1:(y-px(56))/px(44);if(index>=data->visible)index=-1;if(index>=0)index+=data->first;if(index!=data->hot){data->hot=index;InvalidateRect(h,nullptr,FALSE);}return 0;}
 case WM_MOUSEWHEEL:data->first=std::clamp(data->first-(short)HIWORD(w)/WHEEL_DELTA*3,0,std::max(0,(int)data->rows.size()-data->visible));data->hot=-1;InvalidateRect(h,nullptr,FALSE);return 0;
 case WM_LBUTTONUP:choose();return 0;
 case WM_KEYDOWN:
  if(w==VK_ESCAPE){DestroyWindow(h);return 0;}if(w==VK_RETURN){choose();return 0;}
  if((w==VK_DOWN||w==VK_UP)&&!data->rows.empty()){int size=(int)data->rows.size();for(int i=0;i<size;++i){data->hot=(data->hot+(w==VK_DOWN?1:size-1)+size)%size;if(data->rows[data->hot].command)break;}if(data->hot<data->first)data->first=data->hot;if(data->hot>=data->first+data->visible)data->first=data->hot-data->visible+1;InvalidateRect(h,nullptr,FALSE);}return 0;
 case WM_ACTIVATE:if(LOWORD(w)==WA_INACTIVE)DestroyWindow(h);return 0;
 case WM_NCDESTROY:DeleteObject(data->font);DeleteObject(data->small);delete data;return 0;
 }
 return DefWindowProcW(h,message,w,l);
}
}
namespace popup {
void ShowAt(HINSTANCE inst,HWND parent,HWND anchor,const std::wstring& caption,const std::vector<Row>& rows){
 WNDCLASSW wc{};wc.lpfnWndProc=MenuProc;wc.hInstance=inst;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.lpszClassName=L"WirelessBatteryMenu";RegisterClassW(&wc);
 POINT point;GetCursorPos(&point);MONITORINFO monitor{sizeof(monitor)};GetMonitorInfoW(MonitorFromPoint(point,MONITOR_DEFAULTTONEAREST),&monitor);
 int dpi=GetDpiForWindow(parent);if(!dpi)dpi=GetDpiForSystem();auto px=[&](int n){return MulDiv(n,dpi,96);};
 HWND existing=FindWindowW(wc.lpszClassName,nullptr);if(existing&&GetWindow(existing,GW_OWNER)!=parent)existing=nullptr;
 RECT box{};if(anchor)GetWindowRect(anchor,&box);else if(existing)GetWindowRect(existing,&box);
 int visible=std::max(1,std::min((int)rows.size(),(int)(monitor.rcWork.bottom-monitor.rcWork.top-px(90))/px(44)));int width=anchor?std::max((int)(box.right-box.left),px(280)):px(360),height=px(66+visible*44);
 int x=anchor?box.left:existing?box.left:point.x-width;
 int y=anchor?box.bottom+px(6):existing?box.bottom-height:point.y-height;
 if(anchor&&y+height>monitor.rcWork.bottom)y=box.top-px(6)-height;
 x=std::clamp(x,(int)monitor.rcWork.left,(int)std::max(monitor.rcWork.left,monitor.rcWork.right-width));y=std::clamp(y,(int)monitor.rcWork.top,(int)std::max(monitor.rcWork.top,monitor.rcWork.bottom-height));
 HWND h=existing;
 if(h){auto* data=(MenuData*)GetWindowLongPtrW(h,GWLP_USERDATA);data->caption=caption;data->rows=rows;data->hot=-1;data->first=0;data->visible=visible;SetWindowPos(h,HWND_TOPMOST,x,y,width,height,SWP_NOACTIVATE);}
 else{auto* data=new MenuData{parent,caption,rows,typography::Font(px(13)),typography::Font(px(11))};data->visible=visible;h=CreateWindowExW(WS_EX_TOPMOST|WS_EX_TOOLWINDOW,wc.lpszClassName,L"Wedgu",WS_POPUP,x,y,width,height,parent,nullptr,inst,data);if(!h){DeleteObject(data->font);DeleteObject(data->small);delete data;return;}}
 int round=2;DwmSetWindowAttribute(h,33,&round,sizeof(round));SetWindowRgn(h,CreateRoundRectRgn(0,0,width+1,height+1,px(24),px(24)),TRUE);InvalidateRect(h,nullptr,FALSE);ShowWindow(h,SW_SHOW);SetForegroundWindow(h);
}
void Show(HINSTANCE inst,HWND parent,const std::wstring& caption,const std::vector<Row>& rows){ShowAt(inst,parent,nullptr,caption,rows);}
void ShowAnchored(HINSTANCE inst,HWND parent,HWND anchor,const std::wstring& caption,const std::vector<Row>& rows){ShowAt(inst,parent,anchor,caption,rows);}
}
