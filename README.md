# Spout2 Display

A Windows application that opens a window and displays the **Spout2 sender**.
It supports two rendering backends: **DirectX 12** (default) and **OpenGL**.

## Features

- Renders the received texture with DirectX 12 (default) or OpenGL.
- Handles sender resolution changes and reconnects if the sender closes.
- Backend selectable via a settings file or a command-line argument.
- FPS counter under F1 button

## Backend selection

The backend defaults to **DirectX 12**. You can switch to OpenGL in two ways:

1. **Settings file** — create `spout2display.ini` next to the executable:

   ```ini
   [General]
   renderer=opengl
   ```

   (use `renderer=dx12` for DirectX 12)

2. **Command line** (overrides the settings file):
   ```
   spout2display.exe --renderer=opengl
   spout2display.exe --renderer=dx12
   ```

## Requirements

- Windows 10/11 (x64)
- Visual Studio 2022 with the **Desktop development with C++** workload
- CMake 3.16+
- The Spout2 SDK Binaries (https://github.com/leadedge/Spout2/releases/download/2.007.017/Spout-SDK-binaries_2-007-017_1.zip) in `./spout2libs`

## Building

cmake -S . -B build -A x64
cmake --build build --config Release

Final file `build\Release\spout2display.exe`.

## Project layout

```
spout2display/
├── CMakeLists.txt          # Build configuration (static MT Spout libraries)
├── spout2libs/             # Spout2 SDK (download separately)
│   ├── include             #
│   ├── MD                  # Uses the dynamic C/C++ runtime (/MD)
│   └── MT                  # Uses the static C/C++ runtime (/MT)
├── src/
│   ├── main.cpp            # Win32 window + main loop
│   ├── settings.h    # Backend selection (ini + command line)
│   ├── settings.cpp     # Backend selection (ini + command line)
│   ├── renderer.h          # IRenderer interface + factory
│   ├── renderer_factory.cpp# Backend factory
│   ├── renderer_gl.h/.cpp  # OpenGL renderer + SpoutReceiver
│   └── renderer_dx12.h/.cpp# DirectX 12 renderer + spoutDX12
```

## Licensing

This project is licensed under the GNU Affero General Public License
version 3.0 only (AGPL-3.0-only). See the [LICENSE](LICENSE) file for
the full license text.

Copyright (C) 2026 RukiaXoXo  
https://rukia.moe

## Third-party software

- **Spout2 SDK** — BSD 2-Clause, Copyright (c) 2020-2024 Lynn Jarvis.
