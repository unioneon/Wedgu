// Exercise the production result/cache and display logic without touching the tray.
#include "../src/main.cpp"
#include <cassert>

static std::vector<WidgetItem> rendered;
static COLORREF renderedColor=CLR_INVALID,lockColor=CLR_INVALID;
static std::vector<popup::Row> menuRows;
namespace widget {
bool Create(HINSTANCE, HWND, UINT, bool) { return false; }
void Destroy() {}
bool Alive() { return false; }
void SetItems(const std::vector<WidgetItem>& items, const std::wstring&) { rendered = items; }
void SetLightTheme(bool) {}
void SetColor(COLORREF color) {renderedColor=color;}
PixelImage RenderStrip(const std::vector<WidgetItem>&,int,bool,COLORREF){return {};}
bool Reposition() { return false; }
}
namespace lockscreen {void Start(HWND,UINT){} void Update(const AppSettings&s,const std::vector<WidgetItem>&){lockColor=appearance::LockscreenColor(s.taskbarColor);} void Stop(){}}
namespace settingsui {void Show(HINSTANCE,HWND,UINT,const std::vector<WidgetItem>&,const std::wstring&){} void Update(const std::vector<WidgetItem>&,const std::wstring&){} void Close(){} bool Translate(MSG*){return false;}}
namespace popup {void Show(HINSTANCE,HWND,const std::wstring&,const std::vector<Row>& rows){menuRows=rows;}}
std::vector<DeviceStatus> PollAllDevices() { return {}; }
std::wstring DumpHidCollections() { return {}; }

int main() {
    g_config.showWeather=false;g_config.showAir=false;
    g_useWidget = true; // Capture layout through the stub; no Shell_NotifyIcon calls.
    DeviceStatus mouse;
    mouse.key = L"logi:C548:2"; mouse.name = L"MX Master 3S";
    mouse.kind = DeviceKind::Mouse; mouse.percent = 50; mouse.graceSec = 1800;
    std::vector<DeviceStatus> results{mouse};
    OnResults(results);
    assert(rendered.size() == 1 && rendered[0].percent == 50 && !rendered[0].dim);
    results.clear();
    OnResults(results);
    auto& entry = g_entries.at(mouse.key);
    assert(entry.st.percent == -1 && entry.lastPct == 50 && !entry.st.asleep);
    assert(StatusText(entry) == L"50% (last known)");
    assert(rendered[0].percent == 50 && !rendered[0].dim);
    OnResults(results); // Consecutive misses must not invent sleep either.
    assert(!g_entries.at(mouse.key).st.asleep && !rendered[0].dim);
    mouse.percent = 49; results = {mouse}; OnResults(results);
    assert(rendered[0].percent == 49 && !rendered[0].dim);
    assert(StatusText(g_entries.at(mouse.key)) == L"49%");
    mouse.percent = -1; mouse.asleep = true; results = {mouse}; OnResults(results);
    assert(rendered[0].percent == 49 && rendered[0].dim);
    assert(StatusText(g_entries.at(mouse.key)).find(L"asleep") != std::wstring::npos);
    g_entries.at(mouse.key).lastSeen = GetTickCount64() - 1800001;
    results.clear(); OnResults(results);
    assert(g_entries.empty() && rendered.empty());
    mouse.key = L"unknown"; mouse.asleep = false; results = {mouse}; OnResults(results);
    assert(rendered[0].percent == -1 && rendered[0].dim);
    mouse.levelText = L"USB"; results = {mouse}; OnResults(results);
    assert(rendered[0].text == L"USB" && !rendered[0].dim);
    assert(StatusText(g_entries.at(mouse.key)) == L"USB");
    g_config.showAir=true;g_config.iqairKey.clear();g_environment.air.valid=true;g_environment.air.value=117;UpdateWidget();assert(rendered.size()==1);
    g_config.iqairKey=L"test-only-placeholder";UpdateWidget();assert(rendered.size()==2&&rendered.back().type==WidgetType::Air);
    g_config.iqairKey.clear();UpdateWidget();assert(rendered.size()==1);
    g_wantWidget=true;ShowDeviceMenu();assert(menuRows.front().command==ID_BACK&&menuRows.front().stayOpen);
    auto menuDevice=std::find_if(menuRows.begin(),menuRows.end(),[](const popup::Row&r){return r.command==ID_DEVICE_FIRST;});assert(menuDevice!=menuRows.end()&&menuDevice->checked&&menuDevice->stayOpen);
    g_hidden.insert(mouse.key);ShowDeviceMenu();menuDevice=std::find_if(menuRows.begin(),menuRows.end(),[](const popup::Row&r){return r.command==ID_DEVICE_FIRST;});assert(!menuDevice->checked);g_hidden.clear();
    auto widgetMode=std::find_if(menuRows.begin(),menuRows.end(),[](const popup::Row&r){return r.command==ID_MODE_WIDGET;});assert(widgetMode!=menuRows.end()&&widgetMode->checked&&widgetMode->stayOpen);
    // Colour messages must apply without a battery change or pressing Apply.
    const COLORREF pink=RGB(234,182,204),blue=RGB(20,110,220);
    WndProc(nullptr,settingsui::ColorsChanged,pink,0);auto pinkSignature=IconSignature(50,false,false);assert(renderedColor==pink&&lockColor==pink);
    g_entries.at(mouse.key).iconSig=L"cached-pink";
    WndProc(nullptr,settingsui::ColorsChanged,blue,0);assert(renderedColor==blue&&lockColor==blue);assert(g_entries.at(mouse.key).iconSig.empty());assert(IconSignature(50,false,false)!=pinkSignature);
    g_lightTaskbar=true;WndProc(nullptr,settingsui::ColorsChanged,CLR_INVALID,0);assert(renderedColor==CLR_INVALID&&lockColor==RGB(255,255,255));assert(Foreground().r==60);
    g_lightTaskbar=false;assert(Foreground().r==255&&Foreground().g==255&&Foreground().b==255);
    g_config.taskbarColor=blue;HICON icon=IconFor(50,false,false);ICONINFO iconInfo{};assert(GetIconInfo(icon,&iconInfo));BITMAP bitmap{};GetObjectW(iconInfo.hbmColor,sizeof(bitmap),&bitmap);std::vector<DWORD> pixels((size_t)bitmap.bmWidth*bitmap.bmHeight);BITMAPINFO info{};info.bmiHeader={sizeof(BITMAPINFOHEADER),bitmap.bmWidth,-bitmap.bmHeight,1,32,BI_RGB};HDC dc=CreateCompatibleDC(nullptr);assert(GetDIBits(dc,iconInfo.hbmColor,0,bitmap.bmHeight,pixels.data(),&info,DIB_RGB_COLORS));int opaque=0;for(DWORD pixel:pixels)if((pixel>>24)>=250){assert(((pixel>>16)&255)==GetRValue(blue)&&((pixel>>8)&255)==GetGValue(blue)&&(pixel&255)==GetBValue(blue));++opaque;}assert(opaque>0);DeleteDC(dc);DeleteObject(iconInfo.hbmMask);DeleteObject(iconInfo.hbmColor);DestroyIcon(icon);
    assert(!AppSettings{}.showDate);SYSTEMTIME fixed{};fixed.wDay=3;fixed.wMonth=2;fixed.wYear=2027;assert(appdate::Format(fixed)==L"03.02.2027");fixed.wDay=29;fixed.wMonth=2;fixed.wYear=2028;assert(appdate::Format(fixed)==L"29.02.2028");
    WndProc(nullptr,settingsui::DateChanged,TRUE,0);assert(g_config.showDate);assert(rendered.back().type==WidgetType::Date&&rendered.back().text==appdate::Current());g_lastDate=L"old-date";WndProc(nullptr,WM_TIMECHANGE,0,0);assert(g_lastDate==appdate::Current());WndProc(nullptr,settingsui::DateChanged,FALSE,0);assert(!g_config.showDate&&rendered.back().type!=WidgetType::Date);WndProc(nullptr,settingsui::DateChanged,TRUE,0);assert(rendered.back().type==WidgetType::Date);WndProc(nullptr,settingsui::DateChanged,FALSE,0);assert(rendered.back().type!=WidgetType::Date);
    puts("Display state passed: cached level stays normal, explicit sleep dims, refresh recovers, expired entries disappear.");
}
