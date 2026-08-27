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

#include <windows.h>
#include <GL/gl.h>

class Renderer
{
public:
    void init(int width, int height);
    void resize(int width, int height);

    // Draws the given texture (or clears to black if textureID == 0).
    void render(GLuint textureID, unsigned int texW, unsigned int texH);

private:
    int m_width = 0;
    int m_height = 0;
};