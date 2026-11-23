#pragma once
#include <windows.h>
#include <gdiplus.h>
#include <algorithm>
namespace branding {
// Geometry and colours of the user's W.svg, kept as vectors at every DPI.
inline void DrawMark(Gdiplus::Graphics& g,Gdiplus::RectF bounds){
 using namespace Gdiplus;auto state=g.Save();float scale=std::min(bounds.Width/357.f,bounds.Height/241.f);
 g.TranslateTransform(bounds.X+(bounds.Width-357*scale)/2,bounds.Y+(bounds.Height-241*scale)/2);g.ScaleTransform(scale,scale);
 const float tx[]={0,74,165},ty[]={119.975f,119.975f,128.975f};const Color colors[]={Color(255,7,68,112),Color(255,0,106,183),Color(255,13,153,255)};
 for(int i=0;i<3;++i){GraphicsPath p;float w=226.402f,h=111.234f,d=18;
 p.AddArc(0.f,0.f,d,d,180.f,90.f);p.AddArc(w-d,0.f,d,d,270.f,90.f);p.AddArc(w-d,h-d,d,d,0.f,90.f);p.AddArc(0.f,h-d,d,d,90.f,90.f);p.CloseFigure();Matrix transform(.848048f,-.529919f,0,1,tx[i],ty[i]);p.Transform(&transform);SolidBrush brush(colors[i]);g.FillPath(&brush,&p);}
 SolidBrush white(Color(255,255,255,255));g.FillEllipse(&white,125.f,93.f,89.f,89.f);g.Restore(state);
}
}
