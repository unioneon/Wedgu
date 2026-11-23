#pragma once
#include <windows.h>
namespace colorpicker {
// chosenMessage: wParam=target, lParam=COLORREF. sampleMessage starts the screen eyedropper.
void Show(HINSTANCE,HWND owner,UINT chosenMessage,UINT sampleMessage,int target,COLORREF initial);
void Close();
bool Translate(MSG*);
}
