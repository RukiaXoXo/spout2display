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

#include "renderer_gl.h"

#include <GL/gl.h>
#include <cstring>

#include "SpoutReceiver.h"

OpenGLRenderer::~OpenGLRenderer()
{
    shutdown();
}

bool OpenGLRenderer::init(HWND hwnd, int width, int height)
{
    m_hwnd = hwnd;
    m_width = width;
    m_height = height;

    m_hDC = GetDC(hwnd);
    if (!m_hDC)
        return false;

    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;

    int pf = ChoosePixelFormat(m_hDC, &pfd);
    if (pf == 0)
        return false;
    if (!SetPixelFormat(m_hDC, pf, &pfd))
        return false;

    m_hRC = wglCreateContext(m_hDC);
    if (!m_hRC)
        return false;
    if (!wglMakeCurrent(m_hDC, m_hRC))
        return false;

    // Create the Spout receiver only after the GL context is current.
    m_receiver = new SpoutReceiver();
    return true;
}

void OpenGLRenderer::resize(int width, int height)
{
    m_width = width;
    m_height = height;
}

void OpenGLRenderer::setBackgroundColor(float r, float g, float b)
{
    m_bgR = r;
    m_bgG = g;
    m_bgB = b;
}

void OpenGLRenderer::createTexture(unsigned int w, unsigned int h)
{
    if (m_texID != 0)
        glDeleteTextures(1, &m_texID);

    glGenTextures(1, &m_texID);
    glBindTexture(GL_TEXTURE_2D, m_texID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)w, (GLsizei)h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void OpenGLRenderer::renderFrame()
{
    if (!m_receiver)
        return;

    // Look for the first available sender.
    int count = m_receiver->GetSenderCount();
    if (count > 0)
    {
        char name[256] = "";
        if (m_receiver->GetSender(0, name, 256))
        {
            if (!m_connected || std::strcmp(m_senderName, name) != 0)
            {
                m_receiver->ReleaseReceiver();
                m_texW = 0;
                m_texH = 0;
                m_connected = false;

                if (m_receiver->CreateReceiver(name, m_texW, m_texH))
                {
                    std::strncpy(m_senderName, name, sizeof(m_senderName) - 1);
                    m_senderName[sizeof(m_senderName) - 1] = '\0';
                    m_connected = true;
                    createTexture(m_texW, m_texH);
                }
            }
        }
    }

    glViewport(0, 0, m_width, m_height);
    glClearColor(m_bgR, m_bgG, m_bgB, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (m_connected)
    {
        if (m_receiver->ReceiveTexture(m_texID, GL_TEXTURE_2D, false))
        {
            unsigned int w = m_receiver->GetSenderWidth();
            unsigned int h = m_receiver->GetSenderHeight();
            if (w != m_texW || h != m_texH)
            {
                m_texW = w;
                m_texH = h;
                createTexture(w, h);
            }

            // Draw the received texture stretched to the window.
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            glOrtho(0.0, 1.0, 0.0, 1.0, -1.0, 1.0);
            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();

            // Enable alpha blending so the texture's alpha channel lets the
            // background color show through transparent areas.
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, m_texID);
            glColor3f(1.0f, 1.0f, 1.0f);
            glBegin(GL_QUADS);
            // V coordinates flipped so the image is not upside down.
            glTexCoord2f(0.0f, 1.0f);
            glVertex2f(0.0f, 0.0f);
            glTexCoord2f(1.0f, 1.0f);
            glVertex2f(1.0f, 0.0f);
            glTexCoord2f(1.0f, 0.0f);
            glVertex2f(1.0f, 1.0f);
            glTexCoord2f(0.0f, 0.0f);
            glVertex2f(0.0f, 1.0f);
            glEnd();
            glBindTexture(GL_TEXTURE_2D, 0);
            glDisable(GL_TEXTURE_2D);
            glDisable(GL_BLEND);
        }
        else
        {
            // Sender closed; drop the connection and retry next frame.
            m_connected = false;
            m_receiver->ReleaseReceiver();
        }
    }
}

void OpenGLRenderer::present()
{
    if (m_hDC)
        SwapBuffers(m_hDC);
}

void OpenGLRenderer::shutdown()
{
    if (m_receiver)
    {
        delete m_receiver;
        m_receiver = nullptr;
    }
    if (m_texID != 0)
    {
        glDeleteTextures(1, &m_texID);
        m_texID = 0;
    }
    if (m_hRC)
    {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(m_hRC);
        m_hRC = nullptr;
    }
    if (m_hDC && m_hwnd)
    {
        ReleaseDC(m_hwnd, m_hDC);
        m_hDC = nullptr;
    }
}