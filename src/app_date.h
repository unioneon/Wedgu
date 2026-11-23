#pragma once
#include <windows.h>
#include <string>
#include <cwchar>
namespace appdate {
inline std::wstring Format(const SYSTEMTIME& time){wchar_t text[16]{};swprintf(text,16,L"%02u.%02u.%04u",(unsigned)time.wDay,(unsigned)time.wMonth,(unsigned)time.wYear);return text;}
inline std::wstring Current(){SYSTEMTIME time;GetLocalTime(&time);return Format(time);}
}
