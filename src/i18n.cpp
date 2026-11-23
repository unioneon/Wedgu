#include "i18n.h"
#include "../vendor/json.hpp"
#include <memory>
#include <atomic>
#include <map>
#include <cwctype>
namespace {
struct Catalog {std::wstring code;std::map<std::wstring,std::wstring> text;};
std::shared_ptr<const Catalog> active=std::make_shared<Catalog>(Catalog{L"en",{}});
std::wstring Wide(const std::string&s){int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),(int)s.size(),nullptr,0);std::wstring w(n,0);if(n)MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),(int)s.size(),w.data(),n);return w;}
std::wstring Normal(std::wstring s){for(auto&c:s)c=(wchar_t)towlower(c);auto p=s.find_first_of(L"-_");if(p!=s.npos)s.resize(p);return s==L"zh"?L"zh-CN":s;}
}
namespace i18n {
const std::vector<Language>& Languages(){static const std::vector<Language> list={{L"system",L"Как в системе"},{L"en",L"English"},{L"ru",L"Русский"},{L"es",L"Español"},{L"zh-CN",L"简体中文"},{L"fr",L"Français"},{L"de",L"Deutsch"},{L"ar",L"العربية"},{L"pt",L"Português"},{L"ja",L"日本語"},{L"ko",L"한국어"},{L"it",L"Italiano"},{L"tr",L"Türkçe"},{L"hi",L"हिन्दी"}};return list;}
bool Supported(const std::wstring&s){for(const auto&l:Languages())if(s==l.code)return true;return false;}
std::wstring Resolve(const std::wstring& preference,const std::wstring& systemLocale){if(preference!=L"system")return Supported(preference)?preference:L"en";std::wstring locale=systemLocale;if(locale.empty()){ULONG count=0,size=0;if(GetUserPreferredUILanguages(MUI_LANGUAGE_NAME,&count,nullptr,&size)&&size){std::vector<wchar_t> names(size);if(GetUserPreferredUILanguages(MUI_LANGUAGE_NAME,&count,names.data(),&size))locale=names.data();}if(locale.empty()){wchar_t name[LOCALE_NAME_MAX_LENGTH]{};LCIDToLocaleName(MAKELCID(GetUserDefaultUILanguage(),SORT_DEFAULT),name,LOCALE_NAME_MAX_LENGTH,0);locale=name;}}locale=Normal(locale);return Supported(locale)&&locale!=L"system"?locale:L"en";}
void Configure(const std::wstring& preference){auto c=std::make_shared<Catalog>();c->code=Resolve(preference);int index=0;for(size_t i=1;i<Languages().size();++i)if(c->code==Languages()[i].code)index=(int)i-1;HMODULE module=GetModuleHandleW(nullptr);HRSRC r=FindResourceW(module,MAKEINTRESOURCEW(1001+index),MAKEINTRESOURCEW(10));if(r){const char* begin=(const char*)LockResource(LoadResource(module,r));DWORD size=SizeofResource(module,r);auto j=nlohmann::json::parse(begin,begin+size,nullptr,false);if(j.is_object())for(auto it=j.begin();it!=j.end();++it)if(it.value().is_string())c->text.emplace(Wide(it.key()),Wide(it.value().get<std::string>()));}std::atomic_store(&active,std::shared_ptr<const Catalog>(c));}
std::wstring Code(){return std::atomic_load(&active)->code;}
bool RTL(){return Code()==L"ar";}
const wchar_t* FontFamily(){auto code=Code();if(code==L"zh-CN")return L"Microsoft YaHei UI";if(code==L"ja")return L"Yu Gothic UI";if(code==L"ko")return L"Malgun Gothic";if(code==L"hi")return L"Nirmala UI";if(code==L"ar")return L"Segoe UI";return nullptr;}
std::wstring T(const std::wstring&s){auto c=std::atomic_load(&active);auto it=c->text.find(s);return it==c->text.end()?s:it->second;}
std::wstring Detail(const std::wstring&s){auto translated=T(s);if(translated!=s)return translated;for(const auto& delimiter:{std::wstring(L"\n"),std::wstring(L" · ")}){auto p=s.find(delimiter);if(p!=s.npos)return Detail(s.substr(0,p))+delimiter+Detail(s.substr(p+delimiter.size()));}for(const auto&prefix:{std::wstring(L"Погода:"),std::wstring(L"Локскрин:")})if(s.compare(0,prefix.size(),prefix)==0)return T(prefix)+Detail(s.substr(prefix.size()));if(!s.empty()&&s[0]==L' ')return L" "+Detail(s.substr(1));return s;}
int Message(HWND h,const std::wstring&text,const std::wstring&title,UINT flags){return MessageBoxW(h,Detail(text).c_str(),T(title).c_str(),flags|(RTL()?MB_RTLREADING|MB_RIGHT:0));}
}
