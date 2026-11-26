@echo off
rem Builds Wedgu.exe with MinGW-w64 (g++ and windres on PATH).
setlocal
cd /d "%~dp0"
if not exist build mkdir build
windres -i res\app.rc -o build\app_res.o --include-dir res || exit /b 1
g++ -std=c++17 -Os -municode -mwindows -static -s -ffunction-sections -fdata-sections -Wl,--gc-sections ^
    -D_WIN32_WINNT=0x0A00 -DWINVER=0x0A00 -Wall -Wno-missing-field-initializers ^
    src\main.cpp src\i18n.cpp src\app_settings.cpp src\environment.cpp src\settings_ui.cpp src\color_picker.cpp src\location_service.cpp src\lockscreen.cpp src\devices.cpp src\widget.cpp src\hid.cpp src\drivers\keyboards.cpp ^
    src\drivers\glorious.cpp src\drivers\aula.cpp src\drivers\kysona.cpp src\drivers\atk_serial.cpp src\drivers\keychron.cpp src\drivers\lamzu.cpp src\drivers\wlmouse.cpp src\drivers\rongyuan.cpp src\drivers\sony_inzone.cpp src\drivers\rog.cpp src\drivers\zowie.cpp src\drivers\misc_mice.cpp src\drivers\razer_headset.cpp src\drivers\hyperx.cpp src\drivers\logitech.cpp src\drivers\razer.cpp ^
    src\drivers\steelseries.cpp src\drivers\corsair.cpp src\drivers\coolermaster.cpp src\drivers\playstation.cpp src\drivers\gamesir.cpp src\drivers\xinput.cpp src\drivers\rapoo.cpp ^
    src\drivers\generic.cpp src\drivers\headsetcontrol.cpp ^
    build\app_res.o -o Wedgu.exe ^
    -lhid -lsetupapi -lcfgmgr32 -lole32 -loleaut32 -lcomctl32 -lgdi32 -lruntimeobject -lwinhttp -lcrypt32 -luuid -lshell32 -lshcore -lshlwapi -lgdiplus -lcomdlg32 -ldwmapi || exit /b 1
echo Built Wedgu.exe
