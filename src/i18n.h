#pragma once
#include <windows.h>
#include <string>
#include <vector>
namespace i18n {
struct Language { const wchar_t* code; const wchar_t* name; };
const std::vector<Language>& Languages();
std::wstring Resolve(const std::wstring& preference,const std::wstring& systemLocale=L"");
bool Supported(const std::wstring& preference);
void Configure(const std::wstring& preference);
std::wstring Code();
bool RTL();
const wchar_t* FontFamily();
std::wstring T(const std::wstring& source);
// Translate only known message boundaries; never replace fragments in user data.
std::wstring Detail(const std::wstring& source);
int Message(HWND,const std::wstring& text,const std::wstring& title,UINT flags);
}
