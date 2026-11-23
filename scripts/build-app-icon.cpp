// Builds the Wedgu icon matching res/app.svg. g++ ... -lgdiplus -lole32 -lgdi32
#include <windows.h>
#include <gdiplus.h>
#include <fstream>
#include <vector>
#include <cstdint>
#include <cassert>
#include "../src/typography.h"
#include "../src/branding.h"
void Rounded(Gdiplus::Graphics&g,Gdiplus::Pen&pen,float x,float y,float w,float h){Gdiplus::GraphicsPath p;p.AddArc(x,y,4.f,4.f,180.f,90.f);p.AddArc(x+w-4,y,4.f,4.f,270.f,90.f);p.AddArc(x+w-4,y+h-4,4.f,4.f,0.f,90.f);p.AddArc(x,y+h-4,4.f,4.f,90.f,90.f);p.CloseFigure();g.DrawPath(&pen,&p);}
#pragma pack(push,1)
struct Header{uint16_t reserved=0,type=1,count=9;};
struct Entry{uint8_t w,h,colors=0,reserved=0;uint16_t planes=1,bits=32;uint32_t bytes,offset;};
#pragma pack(pop)
int main(){using namespace Gdiplus;GdiplusStartupInput input;ULONG_PTR token;GdiplusStartup(&token,&input,nullptr);{
 const CLSID png={0x557cf406,0x1a04,0x11d3,{0x9a,0x73,0,0,0xf8,0x1e,0xf3,0x2e}};
 std::vector<std::vector<char>> images;std::vector<Entry> entries;uint32_t offset=sizeof(Header)+9*sizeof(Entry);
 for(int s:{16,20,24,32,40,48,64,128,256}){Bitmap large(s*4,s*4,PixelFormat32bppARGB);{Graphics g(&large);g.Clear(Color(0,0,0,0));g.SetSmoothingMode(SmoothingModeAntiAlias);branding::DrawMark(g,RectF(s*.16f,s*.6f,s*3.68f,s*2.8f));}
 Bitmap small(s,s,PixelFormat32bppARGB);{Graphics g(&small);g.SetInterpolationMode(InterpolationModeHighQualityBicubic);g.DrawImage(&large,0,0,s,s);}
 IStream* stream=nullptr;CreateStreamOnHGlobal(nullptr,TRUE,&stream);assert(small.Save(stream,&png,nullptr)==Ok);STATSTG stat{};stream->Stat(&stat,STATFLAG_NONAME);LARGE_INTEGER zero{};stream->Seek(zero,STREAM_SEEK_SET,nullptr);std::vector<char> bytes((size_t)stat.cbSize.QuadPart);ULONG read=0;stream->Read(bytes.data(),(ULONG)bytes.size(),&read);stream->Release();assert(read==bytes.size());entries.push_back({(uint8_t)(s==256?0:s),(uint8_t)(s==256?0:s),0,0,1,32,(uint32_t)bytes.size(),offset});offset+=(uint32_t)bytes.size();images.push_back(std::move(bytes));if(s==256)assert(small.Save(L"build/preview-1.1/wedgu-icon.png",&png,nullptr)==Ok);
 }
 std::ofstream out("res/app.ico",std::ios::binary);Header header;out.write((char*)&header,sizeof(header));for(auto&e:entries)out.write((char*)&e,sizeof(e));for(auto&b:images)out.write(b.data(),b.size());
 }
 GdiplusShutdown(token);wprintf(L"Text family selected: %ls\n",typography::Family());return 0;
}
