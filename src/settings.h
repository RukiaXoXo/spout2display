// Copyright (C) 2026 RukiaXoXo <https://rukia.moe>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published
// by the Free Software Foundation, version 3 of the License only.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include "renderer.h"

// Loads the renderer backend from a settings file (spout2display.ini) and/or
// command-line arguments. Precedence: command line > settings file > default.
//
// Settings file format (placed next to the executable):
//   [General]
//   renderer=dx12        ; "dx12" (default) or "opengl"
//
// Command line:
//   spout2display.exe --renderer=opengl
//   spout2display.exe --renderer=dx12
RenderBackend resolveBackend(int argc, char **argv);