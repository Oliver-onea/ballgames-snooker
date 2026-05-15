# Platform Notes

## Desktop Native

The main port is `native/sdl`, a C++/SDL2 project.

Linux in this workspace:

```bash
make
make run
```

macOS:

```bash
brew install cmake pkg-config sdl2 sdl2_ttf
cmake -S native/sdl -B build/sdl -DCMAKE_BUILD_TYPE=Release
cmake --build build/sdl --parallel
./build/sdl/snooker_sdl
```

Windows with vcpkg:

```bat
vcpkg install sdl2 sdl2-ttf
cmake -S native/sdl -B build\sdl -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake
cmake --build build\sdl --config Release
build\sdl\Release\snooker_sdl.exe
```

## Browser And Mobile

The browser fallback is isolated in `web/`. Open `web/index.html` directly, or package that folder as a PWA/WebView shell for mobile.

## Python Fallback

The Pygame fallback is isolated in `native/pygame/`.

```bash
make pygame
```
