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

// OpenGL backend: creates a legacy OpenGL context and uses SpoutReceiver
// (SpoutGL) to receive the first available sender into an OpenGL texture.
class OpenGLRenderer : public IRenderer
{
public:
    OpenGLRenderer() = default;
    ~OpenGLRenderer() override;

    bool init(HWND hwnd, int width, int height) override;
    void resize(int width, int height) override;
    void setBackgroundColor(float r, float g, float b) override;
    std::vector<std::string> getSenderList() override;
    void setSenderName(const std::string &name) override;
    void renderFrame() override;
    void present() override;
    double getFps() const override;
    void shutdown() override;

private:
    HWND m_hwnd = nullptr;
    HDC m_hDC = nullptr;
    HGLRC m_hRC = nullptr;
    int m_width = 0;
    int m_height = 0;
    float m_bgR = 0.0f;
    float m_bgG = 0.0f;
    float m_bgB = 0.0f;
    std::string m_preferredSender;

    // FPS counter.
    double m_fps = 0.0;
    unsigned long long m_frameCount = 0;
    double m_lastFpsTime = 0.0;

    // Spout receiver (created lazily after the GL context exists).
    class SpoutReceiver *m_receiver = nullptr;
    char m_senderName[256] = "";
    bool m_connected = false;
    unsigned int m_texID = 0;
    unsigned int m_texW = 0;
    unsigned int m_texH = 0;

    void createTexture(unsigned int w, unsigned int h);
};