// Branded publication layout built from original screenshots and the vector Wedgu mark.
// Build: g++ -std=c++17 scripts/build-release-artwork.cpp -o build/release-artwork.exe -lgdiplus -lgdi32
#include <windows.h>
#include <gdiplus.h>
#include <string>
#include <cassert>
#include "../src/branding.h"
using namespace Gdiplus;
const CLSID PNG={0x557cf406,0x1a04,0x11d3,{0x9a,0x73,0,0,0xf8,0x1e,0xf3,0x2e}};
void RoundedPath(GraphicsPath& p,RectF r,float radius){float d=radius*2;p.AddArc(r.X,r.Y,d,d,180,90);p.AddArc(r.GetRight()-d,r.Y,d,d,270,90);p.AddArc(r.GetRight()-d,r.GetBottom()-d,d,d,0,90);p.AddArc(r.X,r.GetBottom()-d,d,d,90,90);p.CloseFigure();}
void Box(Graphics&g,RectF r,Color c,float radius){GraphicsPath p;RoundedPath(p,r,radius);SolidBrush b(c);g.FillPath(&b,&p);}
void Text(Graphics&g,const std::wstring&t,RectF r,float size,Color c,int style=FontStyleRegular){HDC dc=g.GetHDC();HFONT font=CreateFontW(-(int)(size*2),0,0,0,style==FontStyleBold?FW_BOLD:FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,ANTIALIASED_QUALITY,0,L"SF Pro Display");auto old=SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(c.GetR(),c.GetG(),c.GetB()));RECT box{(int)(r.X*2),(int)(r.Y*2),(int)(r.GetRight()*2),(int)(r.GetBottom()*2)};DrawTextW(dc,t.c_str(),-1,&box,DT_SINGLELINE|DT_NOPREFIX);SelectObject(dc,old);DeleteObject(font);g.ReleaseHDC(dc);}
void ImagePart(Graphics&g,Image&source,RectF dest,RectF crop,float radius=0){auto state=g.Save();if(radius){GraphicsPath clip;RoundedPath(clip,dest,radius);g.SetClip(&clip);}ImageAttributes edges;edges.SetWrapMode(WrapModeTileFlipXY);g.DrawImage(&source,dest,crop.X,crop.Y,crop.Width,crop.Height,UnitPixel,&edges);g.Restore(state);}
void RoundedCorners(Bitmap& bitmap,float radius){ int w=bitmap.GetWidth(),h=bitmap.GetHeight();Rect rect(0,0,w,h);BitmapData data{};assert(bitmap.LockBits(&rect,ImageLockModeRead|ImageLockModeWrite,PixelFormat32bppARGB,&data)==Ok);
 for(int y=0;y<h;y++)for(int x=0;x<w;x++){if((x>=radius&&x<w-radius)||(y>=radius&&y<h-radius))continue;float cx=x<radius?radius:w-radius,cy=y<radius?radius:h-radius;int inside=0;for(int sy=0;sy<8;sy++)for(int sx=0;sx<8;sx++){float dx=x+(sx+.5f)/8-cx,dy=y+(sy+.5f)/8-cy;if(dx*dx+dy*dy<=radius*radius)inside++;}auto pixel=(BYTE*)data.Scan0+y*data.Stride+x*4;pixel[3]=(BYTE)(pixel[3]*inside/64);if(!pixel[3])pixel[0]=pixel[1]=pixel[2]=0;}
 bitmap.UnlockBits(&data);}
int main(int argc,char**argv){bool en=argc>1&&std::string(argv[1])=="--english";GdiplusStartupInput input;ULONG_PTR token;GdiplusStartup(&token,&input,nullptr);{
 Bitmap b(2560,1280,PixelFormat32bppRGB);Graphics g(&b);g.ScaleTransform(2,2);g.SetSmoothingMode(SmoothingModeAntiAlias);g.SetInterpolationMode(InterpolationModeHighQualityBicubic);g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
 LinearGradientBrush bg(Point(0,0),Point(1280,640),Color(255,249,251,255),Color(255,226,240,252));g.Clear(Color(255,249,251,255));g.FillRectangle(&bg,-2,-2,1284,644);
 SolidBrush haze(Color(35,105,187,255));g.FillEllipse(&haze,870,-210,630,630);
 branding::DrawMark(g,RectF(54,43,68,47));Text(g,L"Wedgu",RectF(138,38,300,65),45,Color(255,27,34,47),FontStyleBold);
 Box(g,RectF(360,53,72,30),Color(255,232,239,249),15);Text(g,L"1.1.0",RectF(374,57,60,24),17,Color(255,40,101,159));
 Text(g,en?L"Everything important.":L"Всё важное",RectF(54,168,560,64),47,Color(255,27,34,47),FontStyleBold);
 Text(g,en?L"Right by your clock.":L"рядом с часами.",RectF(54,224,580,68),47,Color(255,27,34,47),FontStyleBold);
 Text(g,en?L"Batteries · Weather · Air quality":L"Батареи · Погода · Качество воздуха",RectF(57,320,560,38),22,Color(255,81,95,115));
 Text(g,en?L"Taskbar and lock screen widgets":L"Панель задач и экран блокировки",RectF(57,359,560,34),20,Color(255,81,95,115));
 Box(g,RectF(50,424,548,89),Color(255,255,255,255),20);
 Image bar(L"docs/screenshots/taskbar.png");ImagePart(g,bar,RectF(67,440,514,57),RectF(0,0,498,55),12);
 Box(g,RectF(57,552,130,31),Color(255,229,238,251),15);Text(g,L"Windows 10/11",RectF(70,557,120,23),15,Color(255,36,86,138));
 Box(g,RectF(199,552,120,31),Color(255,229,238,251),15);Text(g,en?L"13 languages":L"13 языков",RectF(209,557,110,23),15,Color(255,36,86,138));
 Text(g,L"by unioneon",RectF(417,558,180,24),16,Color(255,112,126,143));
 for(int i=14;i>0;--i)Box(g,RectF(643-i,77-i*.3f,574+i*2,485+i*2),Color((BYTE)(3),40,75,105),17+i);
 Image settings(L"docs/screenshots/widgets.png");ImagePart(g,settings,RectF(643,72,574,484.4f),RectF(0,0,(float)settings.GetWidth(),(float)settings.GetHeight()),15);
 Box(g,RectF(690,573,478,33),Color(255,255,255,255),16);Text(g,en?L"A Wireless Device Battery Monitor fork · GPL-3.0":L"Форк Wireless Device Battery Monitor · GPL-3.0",RectF(706,580,460,23),14,Color(255,95,111,131));
 Bitmap final(1280,640,PixelFormat32bppARGB);{Graphics output(&final);output.SetInterpolationMode(InterpolationModeHighQualityBicubic);ImagePart(output,b,RectF(0,0,1280,640),RectF(0,0,2560,1280));}RoundedCorners(final,28);assert(final.Save(en?L"docs/social-preview.png":L"docs/cover.ru.png",&PNG,nullptr)==Ok);
 }
 GdiplusShutdown(token);return 0;}
