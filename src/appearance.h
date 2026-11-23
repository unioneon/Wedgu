#pragma once
#include <windows.h>
namespace appearance {
inline COLORREF TaskbarColor(bool light,COLORREF selected){
 return selected==CLR_INVALID?(light?RGB(60,60,60):RGB(255,255,255)):selected;
}
inline COLORREF LockscreenColor(COLORREF selected){return selected==CLR_INVALID?RGB(255,255,255):selected;}
}
