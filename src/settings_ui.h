#pragma once
#include "app_settings.h"
#include "widget.h"
#include <string>
namespace settingsui {
inline constexpr UINT ColorsChanged=WM_APP+8;
inline constexpr UINT LanguageChanged=WM_APP+10;
inline constexpr UINT DateChanged=WM_APP+9;
void Show(HINSTANCE,HWND,UINT appliedMessage,const std::vector<WidgetItem>& preview,const std::wstring& status);
void Update(const std::vector<WidgetItem>&,const std::wstring&);
void Close();
bool Translate(MSG*);
}
namespace popup {
struct Row {
    UINT command;
    std::wstring title,detail;
    bool stayOpen=false,checked=false;
};
void Show(HINSTANCE,HWND,const std::wstring& caption,const std::vector<Row>&);
void ShowAnchored(HINSTANCE,HWND,HWND anchor,const std::wstring& caption,const std::vector<Row>&);
}
