// spout2display
//
// A Windows application that opens a window and displays the Spout2 sender using OpenGL.
//
// This is an original implementation built on the Spout2 SDK
// (BSD 2-Clause, Copyright (c) 2020-2024 Lynn Jarvis).
//
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

#include <windows.h>
#include <cstring>

#include "SpoutReceiver.h"
#include "renderer.h"

// ---------------------------------------------------------------------------
// Global state
// ---------------------------------------------------------------------------
static HDC g_hDC = nullptr;
static HGLRC g_hRC = nullptr;
static Renderer g_renderer;

// NOTE: SpoutReceiver is intentionally NOT a global object. It must be created
// after the OpenGL context exists, so it is a function-local static that is
// initialized on the first call to renderFrame() (which happens after the
// context is created).
static char g_senderName[256] = "";
static bool g_connected = false;
static GLuint g_texID = 0;
static unsigned int g_texW = 0;
static unsigned int g_texH = 0;

// ---------------------------------------------------------------------------
// Texture management
// ---------------------------------------------------------------------------
static void createTexture(unsigned int w, unsigned int h)
{
    if (g_texID != 0)
        glDeleteTextures(1, &g_texID);

    glGenTextures(1, &g_texID);
    glBindTexture(GL_TEXTURE_2D, g_texID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)w, (GLsizei)h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);
}

// ---------------------------------------------------------------------------
// Frame rendering
// ---------------------------------------------------------------------------
static void renderFrame()
{
    // Created lazily on the first frame, after the OpenGL context exists.
    static SpoutReceiver g_receiver;

    // Look for the first available sender.
    int count = g_receiver.GetSenderCount();
    if (count > 0)
    {
        char name[256] = "";
        if (g_receiver.GetSender(0, name, 256))
        {
            // Connect (or reconnect) if we are not connected to this sender.
            if (!g_connected || std::strcmp(g_senderName, name) != 0)
            {
                g_receiver.ReleaseReceiver();
                g_texW = 0;
                g_texH = 0;
                g_connected = false;

                if (g_receiver.CreateReceiver(name, g_texW, g_texH))
                {
                    std::strncpy(g_senderName, name, sizeof(g_senderName) - 1);
                    g_senderName[sizeof(g_senderName) - 1] = '\0';
                    g_connected = true;
                    createTexture(g_texW, g_texH);
                }
            }
        }
    }

    if (g_connected)
    {
        // Receive the sender's shared texture into our OpenGL texture.
        if (g_receiver.ReceiveTexture(g_texID, GL_TEXTURE_2D, false))
        {
            // Handle sender resolution changes.
            unsigned int w = g_receiver.GetSenderWidth();
            unsigned int h = g_receiver.GetSenderHeight();
            if (w != g_texW || h != g_texH)
            {
                g_texW = w;
                g_texH = h;
                createTexture(w, h);
            }
            g_renderer.render(g_texID, g_texW, g_texH);
        }
        else
        {
            // The sender may have closed; drop the connection and retry.
            g_connected = false;
            g_receiver.ReleaseReceiver();
            g_renderer.render(0, 0, 0);
        }
    }
    else
    {
        g_renderer.render(0, 0, 0);
    }

    SwapBuffers(g_hDC);
}

// ---------------------------------------------------------------------------
// Window procedure
// ---------------------------------------------------------------------------
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_SIZE:
        g_renderer.resize(LOWORD(lParam), HIWORD(lParam));
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// ---------------------------------------------------------------------------
// OpenGL context creation
// ---------------------------------------------------------------------------
static bool createGLContext(HWND hwnd)
{
    g_hDC = GetDC(hwnd);
    if (!g_hDC)
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

    int pf = ChoosePixelFormat(g_hDC, &pfd);
    if (pf == 0)
        return false;
    if (!SetPixelFormat(g_hDC, pf, &pfd))
        return false;

    g_hRC = wglCreateContext(g_hDC);
    if (!g_hRC)
        return false;
    if (!wglMakeCurrent(g_hDC, g_hRC))
        return false;

    return true;
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    const wchar_t *className = L"Spout2DisplayWindow";

    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = className;
    RegisterClassEx(&wc);

    HWND hwnd = CreateWindowEx(
        0, className, L"Spout2 Display",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720,
        nullptr, nullptr, hInstance, nullptr);
    if (!hwnd)
        return 1;

    if (!createGLContext(hwnd))
    {
        MessageBox(hwnd, L"Failed to create OpenGL context.",
                   L"Spout2 Display", MB_OK | MB_ICONERROR);
        return 1;
    }

    RECT rc;
    GetClientRect(hwnd, &rc);
    g_renderer.init(rc.right, rc.bottom);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // Main loop - continuous rendering.
    MSG msg = {};
    for (;;)
    {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                if (g_texID != 0)
                    glDeleteTextures(1, &g_texID);
                if (g_hRC)
                {
                    wglMakeCurrent(nullptr, nullptr);
                    wglDeleteContext(g_hRC);
                }
                if (g_hDC)
                    ReleaseDC(hwnd, g_hDC);
                return (int)msg.wParam;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        renderFrame();
    }
}