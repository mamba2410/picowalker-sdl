# picowalker-sdl

Cross-platform SDL implementation of the picowalker stuffs.

For both windows and linux versions, you'll want to make sure that you have a 64k eeprom named `eeprom.bin` in the same directory as your executable.
For this, use [porocyon's rom dumper](https://git.titandemo.org/PoroCYon/pokewalker-rom-dumper) to clone an existing walker.

## Linux

Make sure you have `SDL2` and `SDL2-devel` installed.

```bash
mkdir -p build/x64-linux
cmake -B build/x64-linux .
cmake --build build/x64-linux --config Release
```

Your executable is `build/x64-linux/Release/picowalker-sdl`

## Windows

Install [vcpkg](https://vcpkg.io/en/getting-started.html) and download SDL2 with `./vcpkg.exe install sdl2`

Inside git bash:

```bash
mkdir -p build/x86-windows
cmake -B build/x86-windows -DCMAKE_TOOLCHAIN_FILE=${YOUR_VCPKG_INSTALL_DIR}/scripts/buildsystems/vcpkg.cmake
cmake --build build/x86-windows --config Release
```

Youre excutable is `build/x86-windows/Release/picowalker-sdl.exe` and you need `SDL2.dll` to be in the same folder as the `.exe` for it to run.

## What I need to add

- Only update screen once per main loop.
- Checking pressed buttons can be done with `SDL_PollEvent`. OR better wy might be with `SDL_GetKeyState` to return an array of bools if key is down.
- accel can probably just be a button press. eg `1` for +1 step, `2` for +10 steps, `3` for +100 etc.
- connect can be left alone for now, people will probably ask for melonds or whatever
