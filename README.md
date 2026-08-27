# Spout2 Display

A Windows application that opens a window and displays the **Spout2 sender** using **OpenGL**.

## Features

- Renders the received texture with OpenGL (fixed-function pipeline).
- Handles sender resolution changes and reconnects if the sender closes.

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
├── CMakeLists.txt          # Build configuration (static MT Spout library)
├── spout2libs/                 # Spout2 SDK (download separately)
│   ├── include             #
│   ├── MD                  # Uses the dynamic C/C++ runtime (/MD)
│   └── MT                  # Uses the static C/C++ runtime (/MT)
├── src/
│   ├── main.cpp            # Win32 window + OpenGL context + SpoutReceiver
│   ├── renderer.h          # OpenGL renderer interface
│   └── renderer.cpp        # OpenGL quad renderer
```

## Licensing

This project is licensed under the GNU Affero General Public License
version 3.0 only (AGPL-3.0-only). See the [LICENSE](LICENSE) file for
the full license text.

Copyright (C) 2026 RukiaXoXo  
https://rukia.moe

## Third-party software

- **Spout2 SDK** — BSD 2-Clause, Copyright (c) 2020-2024 Lynn Jarvis.
