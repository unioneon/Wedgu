#pragma once
#include "app_settings.h"
#include "widget.h"
namespace lockscreen {
void Start(HWND notify,UINT resultMessage);
void Update(const AppSettings&,const std::vector<WidgetItem>&);
void Stop();
// Result message lParam owns a new std::wstring; receiver must delete it.
}
