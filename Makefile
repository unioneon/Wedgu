# Builds Wedgu.exe with MinGW-w64 (MSYS2 MINGW64/UCRT64 shell, or any mingw g++ on PATH).
CXX      ?= g++
WINDRES  ?= windres
CXXFLAGS ?= -std=c++17 -Os -Wall -Wno-missing-field-initializers -ffunction-sections -fdata-sections \
            -D_WIN32_WINNT=0x0A00 -DWINVER=0x0A00
LDFLAGS  ?= -municode -mwindows -static -s -Wl,--gc-sections
NEWLIBS := -loleaut32 -lwinhttp -lcrypt32 -luuid -lshell32 -lshcore -lshlwapi -lgdiplus -lcomdlg32 -ldwmapi
LIBS     := -lhid -lsetupapi -lcfgmgr32 -lole32 -lcomctl32 -lgdi32 -lruntimeobject $(NEWLIBS)

SRC := $(wildcard src/*.cpp) $(wildcard src/drivers/*.cpp)
OBJ := $(patsubst src/%.cpp,build/%.o,$(SRC)) build/app_res.o

Wedgu.exe: $(OBJ)
	$(CXX) $(LDFLAGS) $^ -o $@ $(LIBS)

build/%.o: src/%.cpp $(wildcard src/*.h src/drivers/*.h)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -municode -c $< -o $@

build/app_res.o: res/app.rc res/app.manifest res/app.ico $(wildcard res/locales/*.json)
	@mkdir -p build
	$(WINDRES) -i $< -o $@ --include-dir res

clean:
	rm -rf build Wedgu.exe

test: build/app_res.o
	@mkdir -p build
	$(CXX) $(CXXFLAGS) tests/i18n_test.cpp src/i18n.cpp build/app_res.o -o build/i18n_test.exe
	./build/i18n_test.exe
	$(CXX) $(CXXFLAGS) -DUNICODE tests/display_state_test.cpp src/app_settings.cpp src/environment.cpp src/i18n.cpp build/app_res.o -o build/display_state_test.exe -lcomctl32 -lgdi32 $(NEWLIBS) -lole32
	./build/display_state_test.exe
	$(CXX) $(CXXFLAGS) -DUNICODE tests/slider_paint_test.cpp src/widget.cpp src/color_picker.cpp src/environment.cpp src/location_service.cpp src/i18n.cpp build/app_res.o -o build/slider_paint_test.exe $(NEWLIBS) -lgdi32 -lcomctl32 -lole32
	./build/slider_paint_test.exe
	$(CXX) $(CXXFLAGS) tests/environment_test.cpp src/environment.cpp src/app_settings.cpp src/i18n.cpp build/app_res.o -o build/environment_test.exe -lwinhttp -lcrypt32 -lole32 -luuid -lshell32
	./build/environment_test.exe
	$(CXX) $(CXXFLAGS) -DUNICODE tests/widget_render_test.cpp src/widget.cpp src/i18n.cpp build/app_res.o -o build/widget_render_test.exe -lcomctl32 -lgdi32 -lgdiplus
	./build/widget_render_test.exe
	$(CXX) $(CXXFLAGS) tests/keyboard_driver_test.cpp src/drivers/keyboards.cpp src/i18n.cpp build/app_res.o -o build/keyboard_driver_test.exe
	./build/keyboard_driver_test.exe
	$(CXX) -std=c++17 -O2 -Wall tests/rapoo_protocol_test.cpp src/i18n.cpp build/app_res.o -o build/rapoo_protocol_test.exe
	./build/rapoo_protocol_test.exe
	$(CXX) $(CXXFLAGS) tests/rapoo_driver_test.cpp src/drivers/rapoo.cpp src/i18n.cpp build/app_res.o -o build/rapoo_driver_test.exe
	./build/rapoo_driver_test.exe

	$(CXX) $(CXXFLAGS) tests/wlmouse_driver_test.cpp src/drivers/wlmouse.cpp -o build/wlmouse_driver_test.exe
	./build/wlmouse_driver_test.exe

.PHONY: clean test
