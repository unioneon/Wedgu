// Wireless Battery - tray app showing the battery level of wireless mice, headsets and Bluetooth devices.
#include <windows.h>
#include <shellapi.h>
#include <dbt.h>
#include <initguid.h>
#include <hidclass.h>

#include <algorithm>
#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "devices.h"
#include "widget.h"
#include "app_settings.h"
#include "environment.h"
#include "settings_ui.h"
#include "i18n.h"
#include "lockscreen.h"
#include <gdiplus.h>
#include <atomic>
#include "typography.h"
#include "appearance.h"
#include "app_date.h"

namespace {

constexpr UINT WM_TRAY = WM_APP + 1;
constexpr UINT WM_RESULTS = WM_APP + 2;
constexpr UINT WM_WIDGET_CLICK = WM_APP + 3;
constexpr UINT WM_ENV=WM_APP+4, WM_SETTINGS=WM_APP+5, WM_LOCK=WM_APP+6, WM_OPEN_SETTINGS=WM_APP+7;
constexpr UINT_PTR TIMER_DEBOUNCE = 1;
constexpr UINT_PTR TIMER_WIDGET = 2;
constexpr DWORD POLL_INTERVAL_MS = 60 * 1000;
constexpr int LOW_BATTERY = 15;
constexpr UINT PLACEHOLDER_ID = 1;

enum MenuId : UINT { ID_REFRESH = 100, ID_AUTOSTART, ID_EXIT, ID_MODE_WIDGET, ID_MODE_TRAY, ID_SETTINGS, ID_DEVICES, ID_BACK, ID_DEVICE_FIRST = 1000 };

const wchar_t* kAppName = L"Wedgu";
const wchar_t* kRunKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const wchar_t* kRunValue = L"WirelessBattery";

const wchar_t* kSettingsKey = L"Software\\WirelessBattery";

HINSTANCE g_inst;
HWND g_hwnd;
HANDLE g_refreshEvt, g_stopEvt, g_worker;
UINT g_taskbarCreatedMsg;
bool g_lightTaskbar;
AppSettings g_config;
environment::Snapshot g_environment;
std::vector<WidgetItem> g_displayItems;
std::wstring g_lockStatus;
std::atomic<DWORD> g_pollInterval{POLL_INTERVAL_MS};
HANDLE g_environmentWorker=nullptr,g_environmentEvt=nullptr;
HICON g_environmentIcons[2]{};bool g_environmentAdded[2]{};
HICON g_dateIcon=nullptr;bool g_dateAdded=false;std::wstring g_lastDate;
std::wstring ProviderStatus(){return L"IQAir: "+(g_environment.air.valid?std::to_wstring(g_environment.air.value)+L" · ":L"")+g_environment.air.detail+L"\nПогода: "+g_environment.weather.detail+L"\n"+g_lockStatus;}
bool g_wantWidget = true;  // user's choice: taskbar widget (default) or tray icons
bool g_useWidget = false;  // what is actually shown right now

struct TrayEntry {
    UINT uid = 0;
    DeviceStatus st;
    int lastPct = -1;        // last known percentage, kept while the device sleeps
    ULONGLONG lastSeen = 0;
    bool added = false;
    bool lowWarned = false;
    std::wstring iconSig;    // what the current icon shows, to avoid needless redraws
    HICON icon = nullptr;
};
std::map<std::wstring, TrayEntry> g_entries;
std::vector<std::wstring> g_menuDeviceKeys;
bool g_placeholder = false;
HICON g_placeholderIcon = nullptr;

// ---- helpers ----------------------------------------------------------------------------------

UINT StableId(const std::wstring& key) {
    UINT h = 2166136261u;
    for (wchar_t c : key) { h ^= c; h *= 16777619u; }
    return (h & 0x7FFFFFFF) | 0x100;
}

bool ReadLightTaskbar() {
    DWORD v = 0, sz = sizeof(v);
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                     L"SystemUsesLightTheme", RRF_RT_REG_DWORD, nullptr, &v, &sz) == ERROR_SUCCESS)
        return v != 0;
    return false;
}

std::wstring ExePath() {
    wchar_t p[MAX_PATH];
    GetModuleFileNameW(nullptr, p, MAX_PATH);
    return p;
}

// Devices the user chose to hide (by key), stored as a REG_MULTI_SZ.
std::set<std::wstring> g_hidden;

void LoadHidden() {
    DWORD size = 0;
    if (RegGetValueW(HKEY_CURRENT_USER, kSettingsKey, L"HiddenDevices", RRF_RT_REG_MULTI_SZ, nullptr, nullptr, &size) != ERROR_SUCCESS || !size)
        return;
    std::vector<wchar_t> buf(size / sizeof(wchar_t) + 1, 0);
    if (RegGetValueW(HKEY_CURRENT_USER, kSettingsKey, L"HiddenDevices", RRF_RT_REG_MULTI_SZ, nullptr, buf.data(), &size) != ERROR_SUCCESS)
        return;
    for (const wchar_t* p = buf.data(); *p; p += wcslen(p) + 1) g_hidden.insert(p);
}

void SaveHidden() {
    std::wstring multi;
    for (const auto& k : g_hidden) { multi += k; multi.push_back(L'\0'); }
    multi.push_back(L'\0');
    RegSetKeyValueW(HKEY_CURRENT_USER, kSettingsKey, L"HiddenDevices", REG_MULTI_SZ, multi.data(), (DWORD)(multi.size() * sizeof(wchar_t)));
}

bool IsHidden(const std::wstring& key) { return g_hidden.count(key) != 0; }

bool LoadWidgetMode() {
    DWORD v = 1, sz = sizeof(v);
    RegGetValueW(HKEY_CURRENT_USER, kSettingsKey, L"TaskbarWidget", RRF_RT_REG_DWORD, nullptr, &v, &sz);
    return v != 0;
}

void SaveWidgetMode(bool on) {
    DWORD v = on ? 1 : 0;
    RegSetKeyValueW(HKEY_CURRENT_USER, kSettingsKey, L"TaskbarWidget", REG_DWORD, &v, sizeof(v));
}

bool AutostartEnabled() {
    wchar_t buf[MAX_PATH * 2];
    DWORD sz = sizeof(buf);
    if (RegGetValueW(HKEY_CURRENT_USER, kRunKey, kRunValue, RRF_RT_REG_SZ, nullptr, buf, &sz) != ERROR_SUCCESS) return false;
    return _wcsicmp(buf, (L"\"" + ExePath() + L"\"").c_str()) == 0;
}

void SetAutostart(bool on) {
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &k) != ERROR_SUCCESS) return;
    if (on) {
        std::wstring v = L"\"" + ExePath() + L"\"";
        RegSetValueExW(k, kRunValue, 0, REG_SZ, (const BYTE*)v.c_str(), (DWORD)((v.size() + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(k, kRunValue);
    }
    RegCloseKey(k);
}

// ---- icon rendering ---------------------------------------------------------------------------
// Draws the percentage as large digits with a small level bar underneath. GDI text is rendered
// white-on-black into a scratch bitmap and its brightness is used as the alpha channel, which
// gives smooth edges on the transparent icon.

struct Rgb { BYTE r, g, b; };

HICON RenderIcon(const std::wstring& text, int barPct, Rgb textColor, Rgb barColor, bool dim) {
    HWND taskbar = FindWindowW(L"Shell_TrayWnd", nullptr);
    UINT dpi = taskbar ? GetDpiForWindow(taskbar) : 0;
    const int S = GetSystemMetricsForDpi(SM_CXSMICON, dpi ? dpi : GetDpiForSystem());
    const int barH = std::max(2, S / 8);
    const int textH = S - barH - 1;

    HDC dc = CreateCompatibleDC(nullptr);
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    // scratch bitmap for the glyphs
    const int W = S * 3, H = S * 2;
    bi.bmiHeader.biWidth = W;
    bi.bmiHeader.biHeight = -H;
    void* sbits = nullptr;
    HBITMAP scratch = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &sbits, nullptr, 0);
    HGDIOBJ oldBmp = SelectObject(dc, scratch);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    auto* sp = static_cast<DWORD*>(sbits);

    int bx0 = 0, by0 = 0, bx1 = -1, by1 = -1;
    for (int fh = textH * 2; fh >= 6; fh--) {
        HFONT f = CreateFontW(-fh, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              ANTIALIASED_QUALITY, DEFAULT_PITCH, typography::Family());
        HGDIOBJ oldF = SelectObject(dc, f);
        SetTextCharacterExtra(dc, text.size() >= 3 ? -1 : 0);
        memset(sbits, 0, (size_t)W * H * 4);
        TextOutW(dc, 2, 0, text.c_str(), (int)text.size());
        GdiFlush();
        SelectObject(dc, oldF);
        DeleteObject(f);
        bx0 = W; by0 = H; bx1 = -1; by1 = -1;
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++)
                if (sp[y * W + x] & 0xFF) { bx0 = std::min(bx0, x); bx1 = std::max(bx1, x); by0 = std::min(by0, y); by1 = std::max(by1, y); }
        if (bx1 < 0) break;
        if (bx1 - bx0 + 1 <= S && by1 - by0 + 1 <= textH) break;
    }

    // final icon bitmap
    bi.bmiHeader.biWidth = S;
    bi.bmiHeader.biHeight = -S;
    void* ibits = nullptr;
    HBITMAP color = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &ibits, nullptr, 0);
    auto* ip = static_cast<DWORD*>(ibits);
    memset(ip, 0, (size_t)S * S * 4);
    const int alphaScale = dim ? 140 : 255;
    auto put = [&](int x, int y, Rgb c, int a) {
        if (x < 0 || y < 0 || x >= S || y >= S || a <= 0) return;
        a = a * alphaScale / 255;
        ip[y * S + x] = ((DWORD)a << 24) | ((DWORD)c.r << 16) | ((DWORD)c.g << 8) | c.b;  // straight alpha
    };
    if (bx1 >= 0) {
        int gw = bx1 - bx0 + 1, gh = by1 - by0 + 1;
        int ox = (S - gw) / 2, oy = (textH - gh) / 2;
        for (int y = 0; y < gh; y++)
            for (int x = 0; x < gw; x++)
                put(ox + x, oy + y, textColor, sp[(by0 + y) * W + (bx0 + x)] & 0xFF);
    }
    // level bar: faint track + filled part
    int fill = barPct < 0 ? 0 : std::max(barPct > 0 ? 1 : 0, (barPct * S + 50) / 100);
    for (int y = S - barH; y < S; y++)
        for (int x = 0; x < S; x++) {
            if (x < fill) put(x, y, barColor, 255);
            else put(x, y, g_lightTaskbar ? Rgb{ 0, 0, 0 } : Rgb{ 255, 255, 255 }, 70);
        }

    SelectObject(dc, oldBmp);
    DeleteObject(scratch);
    DeleteDC(dc);

    HBITMAP mask = CreateBitmap(S, S, 1, 1, nullptr);
    {
        HDC mdc = CreateCompatibleDC(nullptr);
        HGDIOBJ o = SelectObject(mdc, mask);
        PatBlt(mdc, 0, 0, S, S, BLACKNESS);
        SelectObject(mdc, o);
        DeleteDC(mdc);
    }
    ICONINFO ii{ TRUE, 0, 0, mask, color };
    HICON icon = CreateIconIndirect(&ii);
    DeleteObject(mask);
    DeleteObject(color);
    return icon;
}

Rgb Foreground() {COLORREF color=appearance::TaskbarColor(g_lightTaskbar,g_config.taskbarColor);return {GetRValue(color),GetGValue(color),GetBValue(color)};}
std::wstring IconSignature(int shown,bool charging,bool dim){return std::to_wstring(shown)+(charging?L"c":L"")+(dim?L"d":L"")+(g_lightTaskbar?L"l":L"k")+L":"+std::to_wstring(g_config.taskbarColor);}

HICON IconFor(int pct, bool charging, bool dim) {
    Rgb fg = Foreground();
    Rgb bar = charging        ? Rgb{ 0, 170, 255 }
            : pct <= LOW_BATTERY ? Rgb{ 232, 32, 48 }
            : pct <= 30       ? Rgb{ 255, 150, 0 }
                              : Rgb{ 30, 190, 90 };
    Rgb text = (!charging && pct >= 0 && pct <= LOW_BATTERY) ? Rgb{ 240, 50, 60 } : fg;
    if(g_config.taskbarColor!=CLR_INVALID){text=fg;bar=fg;}
    std::wstring t = pct < 0 ? L"?" : std::to_wstring(pct);
    return RenderIcon(t, pct, text, bar, dim);
}

// ---- tray management --------------------------------------------------------------------------

void CopyTip(wchar_t* dst, size_t cap, const std::wstring& s) {
    wcsncpy(dst, s.c_str(), cap - 1);
    dst[cap - 1] = 0;
}

std::wstring StatusText(const TrayEntry& e) {
    const auto& s = e.st;
    if (!s.connected) return i18n::T(L"off / not connected");
    std::wstring t;
    if (!s.levelText.empty()) t = i18n::T(s.levelText);
    else if (s.percent >= 0) t = std::to_wstring(s.percent) + L"%";
    else if (e.lastPct >= 0) t = std::to_wstring(e.lastPct) + i18n::T(L"% (last known)");
    else t = i18n::T(L"unknown");
    if (s.charging) t += i18n::T(L", charging");
    if (s.asleep) t += i18n::T(L", asleep");
    return t;
}

void TrayCall(DWORD msg, UINT uid, HICON icon, const std::wstring& tip, const wchar_t* infoTitle = nullptr,
              const std::wstring& info = L"") {
    NOTIFYICONDATAW nid{ sizeof(nid) };
    nid.hWnd = g_hwnd;
    nid.uID = uid;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    nid.uCallbackMessage = WM_TRAY;
    nid.hIcon = icon;
    CopyTip(nid.szTip, ARRAYSIZE(nid.szTip), tip);
    if (infoTitle) {
        nid.uFlags |= NIF_INFO;
        CopyTip(nid.szInfoTitle, ARRAYSIZE(nid.szInfoTitle), infoTitle);
        CopyTip(nid.szInfo, ARRAYSIZE(nid.szInfo), info);
        nid.dwInfoFlags = NIIF_WARNING;
    }
    Shell_NotifyIconW(msg, &nid);
    if (msg == NIM_ADD) {
        nid.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &nid);
    }
}

void TrayDelete(UINT uid) {
    NOTIFYICONDATAW nid{ sizeof(nid) };
    nid.hWnd = g_hwnd;
    nid.uID = uid;
    Shell_NotifyIconW(NIM_DELETE, &nid);
}

void UpdatePlaceholder() {
    if (g_useWidget) {
        if (g_placeholder) { TrayDelete(PLACEHOLDER_ID); g_placeholder = false; }
        return;
    }
    bool any = g_environmentAdded[0]||g_environmentAdded[1]||g_dateAdded;
    for (auto& [k, e] : g_entries) any |= e.added;
    if (!any && !g_placeholder) {
        if (!g_placeholderIcon) g_placeholderIcon = RenderIcon(L"-", -1, Foreground(), Foreground(), true);
        TrayCall(NIM_ADD, PLACEHOLDER_ID, g_placeholderIcon, L"Wedgu\n"+i18n::T(L"No devices with battery info found"));
        g_placeholder = true;
    } else if (any && g_placeholder) {
        TrayDelete(PLACEHOLDER_ID);
        g_placeholder = false;
    }
}

void ApplyEntry(TrayEntry& e) {
    const auto& s = e.st;
    bool visible = g_config.showBattery && s.connected && !g_useWidget && !IsHidden(s.key);
    if (!visible) {
        if (e.added) { TrayDelete(e.uid); e.added = false; }
        return;
    }
    int shown = s.percent >= 0 ? s.percent : e.lastPct;
    bool dim = s.asleep || shown < 0;
    std::wstring sig = IconSignature(shown,s.charging,dim);
    if (sig != e.iconSig || !e.icon) {
        if (e.icon) DestroyIcon(e.icon);
        e.icon = IconFor(shown, s.charging, dim);
        e.iconSig = sig;
    }
    std::wstring tip = s.name + L"\n" + StatusText(e);

    const wchar_t* infoTitle = nullptr;
    std::wstring info,lowTitle;
    if (s.charging || shown > LOW_BATTERY + 10) e.lowWarned = false;
    if (!s.charging && s.percent >= 0 && s.percent <= LOW_BATTERY && !e.lowWarned) {
        e.lowWarned = true;
        lowTitle=i18n::T(L"Low battery");infoTitle = lowTitle.c_str();
        info = s.name + L": " + std::to_wstring(s.percent) + L"%";
    }
    TrayCall(e.added ? NIM_MODIFY : NIM_ADD, e.uid, e.icon, tip, infoTitle, info);
    e.added = true;
}

int KindOrder(DeviceKind k) {  // headset first, then mouse, ... (matches the left-to-right layout)
    switch (k) {
        case DeviceKind::Headset:  return 0;
        case DeviceKind::Mouse:    return 1;
        case DeviceKind::Keyboard: return 2;
        case DeviceKind::Gamepad:  return 3;
        default:                   return 4;
    }
}

void UpdateDateTray(){
 if(g_useWidget||!g_config.showDate){if(g_dateAdded)TrayDelete(4);g_dateAdded=false;return;}
 if(g_dateIcon)DestroyIcon(g_dateIcon);
 SYSTEMTIME now;GetLocalTime(&now);g_dateIcon=RenderIcon(std::to_wstring(now.wDay),-1,Foreground(),Foreground(),false);TrayCall(g_dateAdded?NIM_MODIFY:NIM_ADD,4,g_dateIcon,appdate::Format(now));g_dateAdded=true;
}
void UpdateWidget() {
    const auto today=appdate::Current();
    std::vector<const TrayEntry*> list;
    for (auto& [k, e] : g_entries)
        if (g_config.showBattery && e.st.connected && !IsHidden(k)) list.push_back(&e);
    std::sort(list.begin(), list.end(), [](const TrayEntry* a, const TrayEntry* b) {
        int ka = KindOrder(a->st.kind), kb = KindOrder(b->st.kind);
        return ka != kb ? ka < kb : a->st.name < b->st.name;
    });
    std::vector<WidgetItem> items;
    std::wstring tip;
    for (auto* e : list) {
        WidgetItem it;
        it.kind = e->st.kind;
        it.percent = e->st.percent >= 0 ? e->st.percent : e->lastPct;
        it.text = i18n::T(e->st.levelText);
        it.charging = e->st.charging;
        it.dim = e->st.asleep || (it.percent < 0 && it.text.empty());
        it.low = !it.charging && it.percent >= 0 && it.percent <= LOW_BATTERY;
        it.bluetooth = e->st.via == L"Bluetooth";
        items.push_back(it);
        if (!tip.empty()) tip += L"\n";
        tip += e->st.name + L": " + StatusText(*e);
    }
    if (tip.empty()) tip = L"Wedgu: "+i18n::T(L"No devices with battery info found");
    for(int index=0;index<2;++index){bool enabled=index==0?g_config.showWeather:(g_config.showAir&&!g_config.iqairKey.empty());const auto&r=index==0?g_environment.weather:g_environment.air;
        if(!enabled)continue;
        WidgetItem item;item.type=index==0?WidgetType::Weather:WidgetType::Air;item.condition=r.icon;item.text=r.valid?std::to_wstring(r.value)+(index==0?L"°C":L""):L"—";item.dim=!r.valid||r.stale;items.push_back(item);tip+=L"\n"+i18n::Detail(r.detail)+(r.observed.empty()?L"":L"\n"+r.observed)+(r.stale?i18n::T(L" · сохранённое значение"):L"");
    }
    if(g_config.showDate){WidgetItem date;date.type=WidgetType::Date;date.text=today;items.push_back(date);tip+=L"\n"+date.text;}
    g_lastDate=today;UpdateDateTray();
    g_displayItems=items;
    if(g_useWidget)widget::SetItems(items, tip);
    settingsui::Update(items,ProviderStatus());
    lockscreen::Update(g_config,items);
}

void UpdateEnvironmentTray(){for(int i=0;i<2;++i){bool visible=!g_useWidget&&(i==0?g_config.showWeather:(g_config.showAir&&!g_config.iqairKey.empty()));UINT uid=2+i;if(!visible){if(g_environmentAdded[i])TrayDelete(uid);g_environmentAdded[i]=false;continue;}const auto&r=i==0?g_environment.weather:g_environment.air;if(g_environmentIcons[i])DestroyIcon(g_environmentIcons[i]);g_environmentIcons[i]=RenderIcon(r.valid?std::to_wstring(r.value)+(i==0?L"°":L""):L"?",-1,Foreground(),Foreground(),!r.valid||r.stale);TrayCall(g_environmentAdded[i]?NIM_MODIFY:NIM_ADD,uid,g_environmentIcons[i],i18n::Detail(r.detail)+L"\n"+(r.valid?std::to_wstring(r.value)+(i==0?L"°C":L" US AQI"):i18n::T(L"Нет данных"))+L"\n"+r.observed);g_environmentAdded[i]=true;}}

void OnResults(std::vector<DeviceStatus>& results) {
    std::map<std::wstring, bool> seen;
    for (auto& s : results) {
        auto& e = g_entries[s.key];
        if (!e.uid) e.uid = StableId(s.key);
        e.st = s;
        if (s.percent >= 0) e.lastPct = s.percent;
        e.lastSeen = GetTickCount64();
        seen[s.key] = true;
        ApplyEntry(e);
    }
    for (auto it = g_entries.begin(); it != g_entries.end();) {
        auto& e = it->second;
        // A missed battery query does not establish sleep: receivers can time out
        // even while the mouse is in use. Retain the last value without inventing
        // an asleep state; only a driver can explicitly report that state.
        bool inGrace = !seen.count(it->first) && e.st.graceSec > 0 && e.st.connected &&
                       GetTickCount64() - e.lastSeen < (ULONGLONG)e.st.graceSec * 1000;
        if (inGrace) {
            e.st.percent = -1;
            e.st.levelText.clear();
            ApplyEntry(e);
        }
        if (!seen.count(it->first) && !inGrace) {
            if (it->second.added) TrayDelete(it->second.uid);
            if (it->second.icon) DestroyIcon(it->second.icon);
            it = g_entries.erase(it);
        } else {
            ++it;
        }
    }
    UpdatePlaceholder();
    UpdateWidget();
}

// Makes the actual display match the chosen mode; falls back to tray icons if the taskbar
// widget cannot be created (e.g. Explorer not ready yet). Called once a second.
void Reconcile() {
    bool alive = g_wantWidget && widget::Alive();
    if (g_wantWidget && !alive) alive = widget::Create(g_inst, g_hwnd, WM_WIDGET_CLICK, g_lightTaskbar);
    if (!g_wantWidget) widget::Destroy();
    bool changed = alive != g_useWidget;
    g_useWidget = alive;
    if (changed) {
        UpdateEnvironmentTray();
        for (auto& [k, e] : g_entries) ApplyEntry(e);
        UpdatePlaceholder();
        UpdateWidget();
    } else if (g_useWidget) {
        widget::Reposition();
    }
}

void SetMode(bool useWidget) {
    g_wantWidget = useWidget;
    SaveWidgetMode(useWidget);
    Reconcile();
}

void ReAddAll() {
    g_placeholder = false;
    g_environmentAdded[0]=g_environmentAdded[1]=false;g_dateAdded=false;UpdateEnvironmentTray();UpdateDateTray();
    for (auto& [k, e] : g_entries) { e.added = false; ApplyEntry(e); }
    UpdatePlaceholder();
}

void RedrawAll() {
    UpdateEnvironmentTray();UpdateDateTray();
    if (g_placeholderIcon) {
        DestroyIcon(g_placeholderIcon);
        g_placeholderIcon = nullptr;
        if (g_placeholder) { TrayDelete(PLACEHOLDER_ID); g_placeholder = false; }
    }
    for (auto& [k, e] : g_entries) { e.iconSig.clear(); ApplyEntry(e); }
    UpdatePlaceholder();
}

void ApplyColors(COLORREF color){g_config.taskbarColor=color;g_config.lockscreenColor=color;widget::SetColor(color);RedrawAll();UpdateWidget();}

void ShowDeviceMenu(){
 std::vector<popup::Row> rows{{ID_BACK,L"‹  Назад",L"",true}};g_menuDeviceKeys.clear();
 for(const auto&[key,entry]:g_entries){rows.push_back({ID_DEVICE_FIRST+(UINT)g_menuDeviceKeys.size(),entry.st.name+L" · "+StatusText(entry),entry.st.via,true,!IsHidden(key)});g_menuDeviceKeys.push_back(key);}
 if(g_menuDeviceKeys.empty())rows.push_back({0,L"Устройства пока не обнаружены"});
 rows.push_back({ID_MODE_WIDGET,L"На панели задач",L"Иконки и значения в одной строке",true,g_wantWidget});
 rows.push_back({ID_MODE_TRAY,L"Отдельные значки в трее",L"",true,!g_wantWidget});
 rows.push_back({ID_AUTOSTART,L"Запускать с Windows",L"",true,AutostartEnabled()});
 popup::Show(g_inst,g_hwnd,L"Устройства и отображение",rows);
}
void ShowMenu(){popup::Show(g_inst,g_hwnd,L"Wedgu · "+g_config.city,{{ID_REFRESH,L"Обновить данные"},{ID_DEVICES,L"Устройства и отображение  ›",L"",true},{ID_SETTINGS,L"Настройки"},{ID_EXIT,L"Выйти"}});}
void MenuCommand(UINT id){
 switch(id){
 case ID_REFRESH:SetEvent(g_refreshEvt);SetEvent(g_environmentEvt);return;
 case ID_DEVICES:ShowDeviceMenu();return;case ID_BACK:ShowMenu();return;
 case ID_MODE_WIDGET:SetMode(true);ShowDeviceMenu();return;
 case ID_MODE_TRAY:SetMode(false);ShowDeviceMenu();return;
 case ID_AUTOSTART:SetAutostart(!AutostartEnabled());ShowDeviceMenu();return;
 case ID_SETTINGS:settingsui::Show(g_inst,g_hwnd,WM_SETTINGS,g_displayItems,ProviderStatus());return;
 case ID_EXIT:DestroyWindow(g_hwnd);return;
 }
 if(id>=ID_DEVICE_FIRST&&id<ID_DEVICE_FIRST+g_menuDeviceKeys.size()){
  auto key=g_menuDeviceKeys[id-ID_DEVICE_FIRST];if(!g_hidden.erase(key))g_hidden.insert(key);SaveHidden();
  auto entry=g_entries.find(key);if(entry!=g_entries.end())ApplyEntry(entry->second);
  UpdatePlaceholder();UpdateWidget();ShowDeviceMenu();
 }
}

DWORD WINAPI EnvironmentWorker(LPVOID){environment::Client client;for(;;){auto* result=new environment::Snapshot(client.Poll(settings::Load()));if(!PostMessageW(g_hwnd,WM_ENV,0,(LPARAM)result))delete result;HANDLE events[]={g_stopEvt,g_environmentEvt};if(WaitForMultipleObjects(2,events,FALSE,30000)==WAIT_OBJECT_0)return 0;}}

// ---- worker thread ----------------------------------------------------------------------------

DWORD WINAPI Worker(LPVOID) {
    for (;;) {
        auto* v = new std::vector<DeviceStatus>(PollAllDevices());
        if (!PostMessageW(g_hwnd, WM_RESULTS, 0, (LPARAM)v)) delete v;
        HANDLE hs[2] = { g_stopEvt, g_refreshEvt };
        if (WaitForMultipleObjects(2, hs, FALSE, g_pollInterval.load()) == WAIT_OBJECT_0) return 0;
    }
}

void RefreshSoon(UINT ms) { SetTimer(g_hwnd, TIMER_DEBOUNCE, ms, nullptr); }

LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == g_taskbarCreatedMsg && msg != 0) {  // Explorer restarted
        widget::Destroy();
        g_useWidget = false;
        ReAddAll();
        Reconcile();
        return 0;
    }
    switch (msg) {
        case WM_OPEN_SETTINGS:settingsui::Show(g_inst,g_hwnd,WM_SETTINGS,g_displayItems,ProviderStatus());return 0;
        case WM_COMMAND:MenuCommand(LOWORD(wp));return 0;
        case settingsui::LanguageChanged:g_config.language=settings::Load().language;RedrawAll();UpdateWidget();UpdateEnvironmentTray();UpdatePlaceholder();SetEvent(g_environmentEvt);return 0;
        case settingsui::DateChanged:g_config.showDate=wp!=0;UpdateWidget();UpdatePlaceholder();return 0;
        case settingsui::ColorsChanged:ApplyColors((COLORREF)wp);return 0;
        case WM_SETTINGS:g_config=settings::Load();i18n::Configure(g_config.language);g_pollInterval=g_config.batteryMinutes*60000u;ApplyColors(g_config.taskbarColor);SetEvent(g_refreshEvt);SetEvent(g_environmentEvt);return 0;
        case WM_ENV:{auto* result=(environment::Snapshot*)lp;g_environment=*result;delete result;UpdateEnvironmentTray();UpdateWidget();UpdatePlaceholder();return 0;}
        case WM_LOCK:{auto* result=(std::wstring*)lp;g_lockStatus=*result;delete result;settingsui::Update(g_displayItems,ProviderStatus());return 0;}
        case WM_RESULTS: {
            auto* v = reinterpret_cast<std::vector<DeviceStatus>*>(lp);
            OnResults(*v);
            delete v;
            return 0;
        }
        case WM_TRAY:
            switch (LOWORD(lp)) {
                case WM_CONTEXTMENU:
                case NIN_SELECT:
                case NIN_KEYSELECT: ShowMenu(); break;
            }
            return 0;
        case WM_WIDGET_CLICK:
            ShowMenu();
            return 0;
        case WM_ENDSESSION:  // sign-out / shutdown / installer restart manager
            if (wp) DestroyWindow(h);
            return 0;
        case WM_DEVICECHANGE:
            if (wp == DBT_DEVICEARRIVAL || wp == DBT_DEVICEREMOVECOMPLETE) RefreshSoon(2500);
            return TRUE;
        case WM_POWERBROADCAST:
            if (wp == PBT_APMRESUMEAUTOMATIC) RefreshSoon(5000);
            return TRUE;
        case WM_TIMER:
            if (wp == TIMER_DEBOUNCE) { KillTimer(h, TIMER_DEBOUNCE); SetEvent(g_refreshEvt); }
            if (wp == TIMER_WIDGET){Reconcile();if(g_config.showDate&&g_lastDate!=appdate::Current())UpdateWidget();}
            return 0;
        case WM_TIMECHANGE:if(g_config.showDate)UpdateWidget();return 0;
        case WM_SETTINGCHANGE:
            if (lp && !lstrcmpW((LPCWSTR)lp, L"ImmersiveColorSet")) {
                bool light = ReadLightTaskbar();
                if (light != g_lightTaskbar) { g_lightTaskbar = light; RedrawAll(); widget::SetLightTheme(light); }
            }
            return 0;
        case WM_DESTROY:
            settingsui::Close();
            KillTimer(h, TIMER_WIDGET);
            widget::Destroy();
            SetEvent(g_stopEvt);
            if (g_worker) WaitForSingleObject(g_worker, INFINITE);
            if(g_environmentWorker)WaitForSingleObject(g_environmentWorker,INFINITE);
            lockscreen::Stop();
            for(int i=0;i<2;++i){if(g_environmentAdded[i])TrayDelete(2+i);if(g_environmentIcons[i])DestroyIcon(g_environmentIcons[i]);}
            for (auto& [k, e] : g_entries) {
                if (e.added) TrayDelete(e.uid);
                if (e.icon) DestroyIcon(e.icon);
            }
            if(g_dateAdded)TrayDelete(4);
            if(g_dateIcon)DestroyIcon(g_dateIcon);
            if (g_placeholder) TrayDelete(PLACEHOLDER_ID);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

// `WirelessBattery.exe --list` prints what it finds, `--dump` every HID collection.
int ListToConsole(bool dump) {
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!out || out == INVALID_HANDLE_VALUE) {
        if (AttachConsole(ATTACH_PARENT_PROCESS)) out = CreateFileW(L"CONOUT$", GENERIC_WRITE, FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    }
    auto write = [&](const std::wstring& s) {
        int n = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0, nullptr, nullptr);
        std::string u(n, 0);
        WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), u.data(), n, nullptr, nullptr);
        DWORD w;
        WriteFile(out, u.data(), (DWORD)u.size(), &w, nullptr);
    };
    if (dump) {
        write(DumpHidCollections());
        return 0;
    }
    auto list = PollAllDevices();
    if (list.empty()) write(L"No devices with battery info found.\n");
    for (auto& s : list) {
        std::wstring line = s.name + L"  [" + s.via + L"]  ";
        if (!s.connected) line += L"off / not connected";
        else if (s.asleep) line += L"asleep";
        else if (!s.levelText.empty()) line += s.levelText;
        else if (s.percent >= 0) line += std::to_wstring(s.percent) + L"%";
        else line += L"unknown";
        if (s.charging) line += L", charging";
        if (s.millivolts) line += L", " + std::to_wstring(s.millivolts) + L" mV";
        write(line + L"   (" + s.key + L")\n");
    }
    return 0;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR cmdLine, int) {
    if (cmdLine && wcsstr(cmdLine, L"--list")) return ListToConsole(false);
    if (cmdLine && wcsstr(cmdLine, L"--dump")) return ListToConsole(true);
    if (cmdLine && wcsstr(cmdLine, L"--quit")) {  // used by the installer before upgrading/uninstalling
        if (HWND w = FindWindowW(L"WirelessBatteryWnd", nullptr)) {
            PostMessageW(w, WM_CLOSE, 0, 0);
            for (int i = 0; i < 50 && FindWindowW(L"WirelessBatteryWnd", nullptr); i++) Sleep(100);
        }
        return 0;
    }
    g_inst = inst;

    HANDLE mutex = CreateMutexW(nullptr, TRUE, L"Local\\WirelessBatteryTray");
    if (GetLastError() == ERROR_ALREADY_EXISTS){if(cmdLine&&wcsstr(cmdLine,L"--settings"))PostMessageW(FindWindowW(L"WirelessBatteryWnd",nullptr),WM_OPEN_SETTINGS,0,0);CloseHandle(mutex);return 0;}
    Gdiplus::GdiplusStartupInput graphicsInput;ULONG_PTR graphicsToken=0;Gdiplus::GdiplusStartup(&graphicsToken,&graphicsInput,nullptr);
    g_config=settings::Load();g_pollInterval=g_config.batteryMinutes*60000u;

    g_lightTaskbar = ReadLightTaskbar();
    g_taskbarCreatedMsg = RegisterWindowMessageW(L"TaskbarCreated");

    WNDCLASSEXW wc{ sizeof(wc) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.hIcon=LoadIconW(inst,MAKEINTRESOURCEW(1));wc.hIconSm=wc.hIcon;
    wc.lpszClassName = L"WirelessBatteryWnd";
    RegisterClassExW(&wc);
    // A hidden top-level window (not message-only) so it receives the TaskbarCreated broadcast.
    g_hwnd = CreateWindowExW(WS_EX_TOOLWINDOW, wc.lpszClassName, kAppName, WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, inst, nullptr);
    if (!g_hwnd) return 1;

    DEV_BROADCAST_DEVICEINTERFACE_W filter{ sizeof(filter) };
    filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    filter.dbcc_classguid = GUID_DEVINTERFACE_HID;
    HDEVNOTIFY notify = RegisterDeviceNotificationW(g_hwnd, &filter, DEVICE_NOTIFY_WINDOW_HANDLE);

    g_refreshEvt = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    g_stopEvt = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    g_environmentEvt=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    lockscreen::Start(g_hwnd,WM_LOCK);
    widget::SetColor(g_config.taskbarColor);
    g_wantWidget = LoadWidgetMode();
    LoadHidden();
    Reconcile();
    UpdatePlaceholder();  // something to click on until the first poll finishes
    SetTimer(g_hwnd, TIMER_WIDGET, 1000, nullptr);
    g_worker = CreateThread(nullptr, 0, Worker, nullptr, 0, nullptr);

    g_environmentWorker=CreateThread(nullptr,0,EnvironmentWorker,nullptr,0,nullptr);
    if(cmdLine&&wcsstr(cmdLine,L"--settings"))PostMessageW(g_hwnd,WM_OPEN_SETTINGS,0,0);
    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0) > 0) {
        if(settingsui::Translate(&m))continue;
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    // drain results posted after shutdown started
    while (PeekMessageW(&m, nullptr, WM_RESULTS, WM_RESULTS, PM_REMOVE)) delete reinterpret_cast<std::vector<DeviceStatus>*>(m.lParam);
    while(PeekMessageW(&m,nullptr,WM_ENV,WM_ENV,PM_REMOVE))delete (environment::Snapshot*)m.lParam;
    while(PeekMessageW(&m,nullptr,WM_LOCK,WM_LOCK,PM_REMOVE))delete (std::wstring*)m.lParam;
    if (notify) UnregisterDeviceNotification(notify);
    if (g_worker) CloseHandle(g_worker);
    if(g_environmentWorker)CloseHandle(g_environmentWorker);
    CloseHandle(g_stopEvt);CloseHandle(g_refreshEvt);CloseHandle(g_environmentEvt);
    Gdiplus::GdiplusShutdown(graphicsToken);
    ReleaseMutex(mutex);CloseHandle(mutex);
    return 0;
}
