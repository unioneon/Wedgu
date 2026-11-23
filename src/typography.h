#pragma once
#include <windows.h>
#include "i18n.h"
#include <cwchar>
namespace typography {
inline const wchar_t* Family(){
 if(auto fallback=i18n::FontFamily())return fallback;
 static const wchar_t* family=[](){
  HDC dc=CreateCompatibleDC(nullptr);HFONT font=CreateFontW(-16,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,ANTIALIASED_QUALITY,0,L"SF Pro Display");
  auto old=SelectObject(dc,font);wchar_t name[LF_FACESIZE]{};GetTextFaceW(dc,LF_FACESIZE,name);SelectObject(dc,old);DeleteObject(font);DeleteDC(dc);
  return _wcsicmp(name,L"SF Pro Display")==0?L"SF Pro Display":L"Segoe UI";
 }();return family;
}
inline HFONT Font(int px,int weight=FW_NORMAL){return CreateFontW(-px,0,0,0,weight,0,0,0,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,Family());}
}
