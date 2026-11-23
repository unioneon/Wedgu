#include "widget.h"

#include <commctrl.h>
#include <algorithm>
#include <cmath>
#include <gdiplus.h>
#include <cstdlib>
#include "air_paths.h"
#include "weather_icons.h"
#include "typography.h"
#include "appearance.h"

namespace {

HWND g_wnd, g_taskbar, g_tip, g_notify, g_rebar;
UINT g_clickMsg;
bool g_light, g_hover;
COLORREF g_color=CLR_INVALID;
std::vector<WidgetItem> g_items;
std::wstring g_tipText;
RECT g_lastTray{}, g_lastBar{};
int g_lastW = -1, g_lastDpi = 0;
SIZE g_size{};

const wchar_t* kClass = L"WirelessBatteryWidget";

int Dpi() {
    UINT d = g_taskbar ? GetDpiForWindow(g_taskbar) : 0;
    return d ? (int)d : 96;
}
int Px(int v) { return MulDiv(v, Dpi(), 96); }

wchar_t Glyph(DeviceKind k, bool bluetooth) {
    switch (k) {
        case DeviceKind::Headset:  return 0xE7F6;  // Headphone
        case DeviceKind::Mouse:    return 0xE962;  // Mouse
        case DeviceKind::Keyboard: return 0xE765;  // KeyboardClassic
        case DeviceKind::Gamepad:  return 0xE7FC;  // Game
        default:                   return bluetooth ? 0xE702 : 0xE772;  // Bluetooth / Devices
    }
}
constexpr wchar_t kBolt = 0xE945;            // LightningBolt
constexpr wchar_t kBatteryUnknown = 0xE996;  // shown when there is nothing to display

// Creates a font, falling back when the preferred face is not installed (GDI would silently substitute).
HFONT MakeFont(int px, int weight, const wchar_t* face, const wchar_t* fallback) {
    for (const wchar_t* f : { face, fallback }) {
        HFONT h = CreateFontW(-px, 0, 0, 0, weight, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              ANTIALIASED_QUALITY, DEFAULT_PITCH, f);
        HDC dc = CreateCompatibleDC(nullptr);
        HGDIOBJ o = SelectObject(dc, h);
        wchar_t got[LF_FACESIZE] = {};
        GetTextFaceW(dc, LF_FACESIZE, got);
        SelectObject(dc, o);
        DeleteDC(dc);
        if (!_wcsicmp(got, f) || f == fallback) return h;
        DeleteObject(h);
    }
    return nullptr;
}

// Grayscale coverage mask of a string, cropped to its ink bounds.
struct Mask {
    int w = 0, h = 0;
    std::vector<BYTE> a;
};

// The user's outlined controller design, rasterized from vector paths at each DPI.
// Keep the four D-pad bars and single large ring readable at taskbar sizes.
Mask RenderGamepad(int width) {
    struct Point { float x, y; };
    struct Line { Point a, b; float radius; };
    std::vector<Line> lines;
    auto line = [&](Point a, Point b, float stroke) { lines.push_back({a,b,stroke/2}); };
    auto curve = [&](Point a, Point b, Point c, Point d) {
        Point last = a;
        for (int i=1; i<=16; ++i) {
            float t=i/16.f, u=1-t;
            Point p{u*u*u*a.x+3*u*u*t*b.x+3*u*t*t*c.x+t*t*t*d.x,
                    u*u*u*a.y+3*u*u*t*b.y+3*u*t*t*c.y+t*t*t*d.y};
            line(last,p,.95f); last=p;
        }
    };
    line({7.4f,5},{16.6f,5},.95f);
    curve({16.6f,5},{19.3f,5},{20.5f,6.4f},{21.4f,9.5f});
    curve({21.4f,9.5f},{22.3f,12.3f},{23,15.5f},{22.5f,17.4f});
    curve({22.5f,17.4f},{22.1f,19},{20.5f,19.5f},{19.5f,18.3f});
    line({19.5f,18.3f},{17.9f,15.8f},.95f);
    curve({17.9f,15.8f},{17.5f,15.2f},{17.1f,14.9f},{16.4f,14.9f});
    line({16.4f,14.9f},{7.6f,14.9f},.95f);
    curve({7.6f,14.9f},{6.9f,14.9f},{6.5f,15.2f},{6.1f,15.8f});
    line({6.1f,15.8f},{4.5f,18.3f},.95f);
    curve({4.5f,18.3f},{3.5f,19.5f},{1.9f,19},{1.5f,17.4f});
    curve({1.5f,17.4f},{1,15.5f},{1.7f,12.3f},{2.6f,9.5f});
    curve({2.6f,9.5f},{3.5f,6.4f},{4.7f,5},{7.4f,5});
    auto rect = [](Point p, float x0, float y0, float x1, float y1) {
        return p.x>=x0 && p.x<=x1 && p.y>=y0 && p.y<=y1;
    };
    auto ring = [](Point p, Point c, float r, float stroke) {
        float x=p.x-c.x,y=p.y-c.y;
        return std::abs(std::sqrt(x*x+y*y)-r)<=stroke/2;
    };
    Mask m;
    m.w=width; m.h=(width*16+12)/24; m.a.resize((size_t)m.w*m.h);
    constexpr int samples=8;
    for(int y=0;y<m.h;++y) for(int x=0;x<m.w;++x) {
        int coverage=0;
        for(int sy=0;sy<samples;++sy)for(int sx=0;sx<samples;++sx) {
            Point p{(x+(sx+.5f)/samples)*24/width,4+(y+(sy+.5f)/samples)*24/width};
            bool ink=ring(p,{17.5f,9.7f},1.7f,.85f)||
                rect(p,4.6f,9.45f,6.45f,10.35f)||rect(p,7.55f,9.45f,9.4f,10.35f)||
                rect(p,6.55f,7.3f,7.45f,9.15f)||rect(p,6.55f,10.65f,7.45f,12.5f);
            for(const auto& l:lines) {
                if(ink)break;
                float vx=l.b.x-l.a.x,vy=l.b.y-l.a.y;
                float t=std::max(0.f,std::min(1.f,((p.x-l.a.x)*vx+(p.y-l.a.y)*vy)/(vx*vx+vy*vy)));
                float dx=p.x-l.a.x-t*vx,dy=p.y-l.a.y-t*vy;
                ink=dx*dx+dy*dy<=l.radius*l.radius;
            }
            coverage+=ink;
        }
        m.a[y*m.w+x]=(BYTE)((coverage*255+samples*samples/2)/(samples*samples));
    }
    return m;
}

Mask RenderMask(const std::wstring& s, HFONT font) {
    Mask m;
    // Render in grayscale at 4x resolution, then average coverage. This avoids
    // jagged single-pixel edges on small Fluent glyphs at fractional DPI.
    constexpr int renderScale = 4;
    LOGFONTW lf{};
    GetObjectW(font, sizeof(lf), &lf);
    lf.lfHeight *= renderScale;
    lf.lfWidth *= renderScale;
    lf.lfQuality = ANTIALIASED_QUALITY;
    HFONT largeFont = CreateFontIndirectW(&lf);
    const int scale = largeFont ? renderScale : 1;
    HDC dc = CreateCompatibleDC(nullptr);
    HGDIOBJ of = SelectObject(dc, largeFont ? largeFont : font);
    SIZE sz;
    GetTextExtentPoint32W(dc, s.c_str(), (int)s.size(), &sz);
    int W = sz.cx + 8, H = sz.cy + 8;
    BITMAPINFO bi{};
    bi.bmiHeader = { sizeof(BITMAPINFOHEADER), W, -H, 1, 32, BI_RGB };
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HGDIOBJ ob = SelectObject(dc, bmp);
    memset(bits, 0, (size_t)W * H * 4);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    TextOutW(dc, 4, 4, s.c_str(), (int)s.size());
    GdiFlush();
    auto* p = static_cast<DWORD*>(bits);
    int x0 = W, y0 = H, x1 = -1, y1 = -1;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            if (p[y * W + x] & 0xFF) { x0 = std::min(x0, x); x1 = std::max(x1, x); y0 = std::min(y0, y); y1 = std::max(y1, y); }
    if (x1 >= 0) {
        m.w = (x1 - x0 + scale) / scale;
        m.h = (y1 - y0 + scale) / scale;
        m.a.resize((size_t)m.w * m.h);
        for (int y = 0; y < m.h; y++)
            for (int x = 0; x < m.w; x++) {
                unsigned coverage = 0;
                for (int sy = 0; sy < scale; ++sy)
                    for (int sx = 0; sx < scale; ++sx) {
                        int px = x0 + x * scale + sx, py = y0 + y * scale + sy;
                        if (px <= x1 && py <= y1) coverage += p[py * W + px] & 0xFF;
                    }
                m.a[y * m.w + x] = (BYTE)((coverage + scale * scale / 2) / (scale * scale));
            }
    }
    SelectObject(dc, ob);
    SelectObject(dc, of);
    DeleteObject(bmp);
    DeleteDC(dc);
    if (largeFont) DeleteObject(largeFont);
    return m;
}

// Rasterize the supplied 24x24 SVGs at 4x for smooth edges.
Mask RenderSvg(const char* svg,int width) {
    using namespace Gdiplus;
    Mask m;m.w=m.h=width;m.a.resize((size_t)width*width);
    Bitmap bmp(width*4,width*4,PixelFormat32bppARGB);Graphics g(&bmp);
    g.Clear(Color(0,0,0,0));g.SetSmoothingMode(SmoothingModeAntiAlias);g.ScaleTransform(width/6.f,width/6.f);
    GraphicsPath path;const char* p=svg;char cmd=0;float x=0,y=0,sx=0,sy=0;
    auto number=[&](){while(*p==' '||*p==',')++p;char* end=nullptr;float v=strtof(p,&end);p=end;return v;};
    while(*p){while(*p==' '||*p==',')++p;if(!*p)break;if((*p>='A'&&*p<='Z'))cmd=*p++;
        if(cmd=='M'){x=number();y=number();sx=x;sy=y;path.StartFigure();cmd='L';}
        else if(cmd=='L'){float nx=number(),ny=number();path.AddLine(x,y,nx,ny);x=nx;y=ny;}
        else if(cmd=='H'){float nx=number();path.AddLine(x,y,nx,y);x=nx;}
        else if(cmd=='V'){float ny=number();path.AddLine(x,y,x,ny);y=ny;}
        else if(cmd=='C'){float a=number(),b=number(),c=number(),d=number(),nx=number(),ny=number();path.AddBezier(x,y,a,b,c,d,nx,ny);x=nx;y=ny;}
        else if(cmd=='Z'){path.CloseFigure();x=sx;y=sy;cmd=0;}else break;
    }
    Pen pen(Color(255,255,255,255),1.5f);pen.SetStartCap(LineCapRound);pen.SetEndCap(LineCapRound);pen.SetLineJoin(LineJoinRound);g.DrawPath(&pen,&path);g.Flush();
    for(int y0=0;y0<width;++y0)for(int x0=0;x0<width;++x0){unsigned a=0;for(int yy=0;yy<4;++yy)for(int xx=0;xx<4;++xx){Color c;bmp.GetPixel(x0*4+xx,y0*4+yy,&c);a+=c.GetA();}m.a[y0*width+x0]=(BYTE)(a/16);}
    return m;
}
Mask RenderAir(int index,int width){return RenderSvg(AirPaths[std::max(0,std::min(2,index))],width);}
Mask RenderWeather(int code,int width){auto path=weathericons::Paths(code);return RenderSvg(path.c_str(),width);}

struct Rgb { int r, g, b; };

struct Canvas {
    int w, h;
    std::vector<DWORD> px;  // premultiplied BGRA
    Canvas(int w_, int h_) : w(w_), h(h_), px((size_t)w_ * h_, 0) {}
    void Blend(int x, int y, Rgb c, int a) {  // source-over, a = 0..255
        if (x < 0 || y < 0 || x >= w || y >= h || a <= 0) return;
        DWORD d = px[y * w + x];
        int inv = 255 - a;
        int da = d >> 24, dr = (d >> 16) & 255, dg = (d >> 8) & 255, db = d & 255;
        int oa = a + da * inv / 255;
        int orr = c.r * a / 255 + dr * inv / 255;
        int og = c.g * a / 255 + dg * inv / 255;
        int ob = c.b * a / 255 + db * inv / 255;
        px[y * w + x] = ((DWORD)oa << 24) | ((DWORD)orr << 16) | ((DWORD)og << 8) | (DWORD)ob;
    }
    void Draw(const Mask& m, int x, int y, Rgb c, int alpha) {
        for (int j = 0; j < m.h; j++)
            for (int i = 0; i < m.w; i++) Blend(x + i, y + j, c, m.a[j * m.w + i] * alpha / 255);
    }
};

struct Piece { Mask m; Rgb color; int alpha; int gapAfter; };

std::vector<Piece> Layout(int& width,const std::vector<WidgetItem>& items,int dpi,bool light,COLORREF color) {
    auto px=[&](int n){return std::max(1,MulDiv(n,dpi,96));};
    COLORREF foreground=appearance::TaskbarColor(light,color);
    Rgb fg{GetRValue(foreground),GetGValue(foreground),GetBValue(foreground)};
    Rgb red = light ? Rgb{ 180, 45, 50 } : Rgb{ 220, 110, 110 };
    Rgb charge = light ? Rgb{ 45, 105, 155 } : Rgb{ 125, 175, 210 };
    if(color!=CLR_INVALID){red=fg;charge=fg;}
    HFONT icons = MakeFont(px(16), FW_NORMAL, L"Segoe Fluent Icons", L"Segoe MDL2 Assets");
    HFONT bolt = MakeFont(px(11), FW_NORMAL, L"Segoe Fluent Icons", L"Segoe MDL2 Assets");
    HFONT text = typography::Font(px(13));

    std::vector<Piece> out;
    if (items.empty()) out.push_back({ RenderMask(std::wstring(1, kBatteryUnknown), icons), fg, 110, 0 });
    for (size_t i = 0; i < items.size(); i++) {
        const auto& it = items[i];
        int a = it.dim ? 120 : 255;
        Rgb tc = it.low ? red : fg;
        if(it.type==WidgetType::Date){out.push_back({RenderMask(it.text,text),fg,a,i+1<items.size()?px(14):0});continue;}
        Mask icon = it.type==WidgetType::Air ? RenderAir(it.condition,px(18)) :
                    it.type==WidgetType::Weather ? RenderWeather(it.condition,px(20)) :
                    it.kind == DeviceKind::Gamepad ? RenderGamepad(px(20)) :
                    RenderMask(std::wstring(1, Glyph(it.kind, it.bluetooth)), icons);
        out.push_back({ std::move(icon), fg, a, px(5) });
        std::wstring t = !it.text.empty() ? it.text : it.percent >= 0 ? std::to_wstring(it.percent) + L"%" : L"?";
        out.push_back({ RenderMask(t, text), tc, a, it.charging ? px(2) : 0 });
        if (it.charging) out.push_back({ RenderMask(std::wstring(1, kBolt), bolt), charge, a, 0 });
        out.back().gapAfter = i + 1 < items.size() ? px(14) : 0;
    }
    DeleteObject(icons);
    DeleteObject(bolt);
    DeleteObject(text);

    width = px(8) * 2;
    for (auto& p : out) width += p.m.w + p.gapAfter;
    return out;
}

void Paint() {
    if (!g_wnd) return;
    RECT bar;
    GetClientRect(g_taskbar, &bar);
    int H = bar.bottom - bar.top;
    int W = 0;
    auto pieces = Layout(W,g_items,Dpi(),g_light,g_color);
    if (W <= 0 || H <= 0) return;
    Canvas cv(W, H);

    // Nearly transparent background so the whole strip receives mouse input,
    // plus a rounded hover highlight like the system tray icons have.
    for (auto& p : cv.px) p = 0x01000000;
    if (g_hover) {
        int inset = Px(4), r = Px(4);
        Rgb hc = g_light ? Rgb{ 0, 0, 0 } : Rgb{ 255, 255, 255 };
        int ha = g_light ? 20 : 26;
        for (int y = inset; y < H - inset; y++)
            for (int x = 0; x < W; x++) {
                int dx = x < r ? r - x : (x >= W - r ? x - (W - r - 1) : 0);
                int dy = y < inset + r ? inset + r - y : (y >= H - inset - r ? y - (H - inset - r - 1) : 0);
                if (dx * dx + dy * dy <= r * r) cv.Blend(x, y, hc, ha);
            }
    }

    int x = Px(8);
    for (auto& p : pieces) {
        cv.Draw(p.m, x, (H - p.m.h) / 2, p.color, p.alpha);
        x += p.m.w + p.gapAfter;
    }

    HDC screen = GetDC(nullptr);
    HDC mem = CreateCompatibleDC(screen);
    BITMAPINFO bi{};
    bi.bmiHeader = { sizeof(BITMAPINFOHEADER), W, -H, 1, 32, BI_RGB };
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(mem, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    memcpy(bits, cv.px.data(), (size_t)W * H * 4);
    HGDIOBJ ob = SelectObject(mem, bmp);
    g_size = { W, H };
    SIZE sz{ W, H };
    POINT src{ 0, 0 };
    BLENDFUNCTION bf{ AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    UpdateLayeredWindow(g_wnd, screen, nullptr, &sz, mem, &src, 0, &bf, ULW_ALPHA);
    SelectObject(mem, ob);
    DeleteObject(bmp);
    DeleteDC(mem);
    ReleaseDC(nullptr, screen);
    g_lastW = -1;  // force a re-position with the new size
}

LRESULT CALLBACK WidgetProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_MOUSEMOVE:
            if (!g_hover) {
                g_hover = true;
                TRACKMOUSEEVENT t{ sizeof(t), TME_LEAVE, h, 0 };
                TrackMouseEvent(&t);
                Paint();
                widget::Reposition();
            }
            return 0;
        case WM_MOUSELEAVE:
            g_hover = false;
            Paint();
            widget::Reposition();
            return 0;
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
            PostMessageW(g_notify, g_clickMsg, 0, 0);
            return 0;
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

}  // namespace

namespace widget {
void SetColor(COLORREF c){g_color=c;Paint();}
PixelImage RenderStrip(const std::vector<WidgetItem>& items,int dpi,bool light,COLORREF color){
    int w=0;auto pieces=Layout(w,items,dpi,light,color);int h=MulDiv(32,dpi,96);for(const auto&p:pieces)h=std::max(h,p.m.h+4);
    Canvas cv(w,h);int x=MulDiv(8,dpi,96);for(const auto&p:pieces){cv.Draw(p.m,x,(h-p.m.h)/2,p.color,p.alpha);x+=p.m.w+p.gapAfter;}
    return {w,h,std::move(cv.px)};
}

bool Create(HINSTANCE inst, HWND notifyWnd, UINT clickMsg, bool lightTheme) {
    Destroy();
    g_taskbar = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (!g_taskbar || !FindWindowExW(g_taskbar, nullptr, L"TrayNotifyWnd", nullptr)) { g_taskbar = nullptr; return false; }
    RECT tb;
    GetWindowRect(g_taskbar, &tb);
    if (tb.bottom - tb.top > tb.right - tb.left) { g_taskbar = nullptr; return false; }  // vertical taskbar: use tray icons
    // Windows 11's taskbar is a XAML island; on Windows 10 the task buttons live in ReBarWindow32
    // and have to be narrowed so they never slide under the widget.
    g_rebar = FindWindowExW(g_taskbar, nullptr, L"Windows.UI.Composition.DesktopWindowContentBridge", nullptr)
                  ? nullptr
                  : FindWindowExW(g_taskbar, nullptr, L"ReBarWindow32", nullptr);
    g_notify = notifyWnd;
    g_clickMsg = clickMsg;
    g_light = lightTheme;

    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc{ sizeof(wc) };
        wc.lpfnWndProc = WidgetProc;
        wc.hInstance = inst;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = kClass;
        registered = RegisterClassExW(&wc) != 0;
    }
    g_wnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_NOACTIVATE, kClass, L"", WS_CHILD | WS_CLIPSIBLINGS, 0, 0, 1, 1,
                            g_taskbar, nullptr, inst, nullptr);
    if (!g_wnd) { g_taskbar = nullptr; return false; }

    INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_WIN95_CLASSES };
    InitCommonControlsEx(&icc);
    g_tip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr, WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX, 0, 0, 0, 0,
                            notifyWnd, nullptr, inst, nullptr);
    if (g_tip) {
        TTTOOLINFOW ti{ sizeof(ti) };
        ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
        ti.hwnd = notifyWnd;
        ti.uId = (UINT_PTR)g_wnd;
        ti.lpszText = const_cast<wchar_t*>(L"Wireless Battery");
        SendMessageW(g_tip, TTM_ADDTOOLW, 0, (LPARAM)&ti);
        SendMessageW(g_tip, TTM_SETMAXTIPWIDTH, 0, 600);
    }
    g_lastDpi = Dpi();
    Paint();
    g_lastW = -1;
    Reposition();
    ShowWindow(g_wnd, SW_SHOWNOACTIVATE);
    return true;
}

void Destroy() {
    if (g_rebar && IsWindow(g_rebar) && g_taskbar && IsWindow(g_taskbar)) {
        // Let Explorer lay the task buttons out again over the full width.
        RECT r;
        GetClientRect(g_taskbar, &r);
        PostMessageW(g_taskbar, WM_SIZE, SIZE_RESTORED, MAKELPARAM(r.right, r.bottom));
    }
    g_rebar = nullptr;
    if (g_tip) { DestroyWindow(g_tip); g_tip = nullptr; }
    if (g_wnd) { DestroyWindow(g_wnd); g_wnd = nullptr; }
    g_taskbar = nullptr;
    g_hover = false;
}

bool Alive() { return g_wnd && IsWindow(g_wnd) && g_taskbar && IsWindow(g_taskbar); }

void SetItems(const std::vector<WidgetItem>& items, const std::wstring& tooltip) {
    g_items = items;
    if (tooltip != g_tipText && g_tip) {
        g_tipText = tooltip;
        TTTOOLINFOW ti{ sizeof(ti) };
        ti.hwnd = g_notify;
        ti.uId = (UINT_PTR)g_wnd;
        ti.lpszText = const_cast<wchar_t*>(g_tipText.c_str());
        SendMessageW(g_tip, TTM_UPDATETIPTEXTW, 0, (LPARAM)&ti);
    }
    Paint();
    Reposition();
}

void SetLightTheme(bool light) {
    g_light = light;
    Paint();
}

bool Reposition() {
    if (!Alive()) return false;
    if (Dpi() != g_lastDpi) { g_lastDpi = Dpi(); Paint(); }
    HWND tray = FindWindowExW(g_taskbar, nullptr, L"TrayNotifyWnd", nullptr);
    if (!tray) return true;
    RECT tr, bar;
    GetWindowRect(tray, &tr);
    GetWindowRect(g_taskbar, &bar);
    POINT p{ tr.left, bar.top };
    ScreenToClient(g_taskbar, &p);
    int left = p.x - g_size.cx - Px(2);

    // Windows 10: Explorer re-expands the task buttons on its own layout passes; narrow them again.
    if (g_rebar && IsWindow(g_rebar)) {
        RECT rr;
        GetWindowRect(g_rebar, &rr);
        POINT tl{ rr.left, rr.top };
        ScreenToClient(g_taskbar, &tl);
        if (tl.x + (rr.right - rr.left) > left && left > tl.x)
            MoveWindow(g_rebar, tl.x, tl.y, left - tl.x, rr.bottom - rr.top, TRUE);
    }

    // Keep us above the taskbar's own content (our sibling windows).
    bool covered = GetWindow(g_wnd, GW_HWNDPREV) != nullptr;
    if (!covered && EqualRect(&tr, &g_lastTray) && EqualRect(&bar, &g_lastBar) && g_lastW == g_size.cx) return true;
    g_lastTray = tr;
    g_lastBar = bar;
    g_lastW = g_size.cx;
    SetWindowPos(g_wnd, HWND_TOP, left, 0, g_size.cx, g_size.cy, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    return true;
}

}  // namespace widget
