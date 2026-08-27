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
#include <windowsx.h>

#include <string>
#include <vector>

#include "renderer.h"
#include "settings.h"

// ---------------------------------------------------------------------------
// Global state
// ---------------------------------------------------------------------------
static std::unique_ptr<IRenderer> g_renderer;
static bool g_showFps = false;
static std::wstring g_baseTitle = L"Spout2 Display";

// Menu command IDs for the background color.
enum
{
    ID_BG_BLACK = 1,
    ID_BG_WHITE,
    ID_BG_RED,
    ID_BG_GREEN,
    ID_BG_BLUE
};

static void applyBackground(float r, float g, float b)
{
    if (g_renderer)
        g_renderer->setBackgroundColor(r, g, b);
}

// ---------------------------------------------------------------------------
// Window procedure
// ---------------------------------------------------------------------------
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_KEYDOWN:
        if (wParam == VK_F1)
        {
            g_showFps = !g_showFps;
            if (!g_showFps)
                SetWindowTextW(hwnd, g_baseTitle.c_str());
        }
        break;
    case WM_SIZE:
        if (g_renderer)
            g_renderer->resize(LOWORD(lParam), HIWORD(lParam));
        break;
    case WM_CONTEXTMENU:
    {
        HMENU menu = CreatePopupMenu();
        AppendMenu(menu, MF_STRING, ID_BG_BLACK, L"Black background");
        AppendMenu(menu, MF_STRING, ID_BG_WHITE, L"White background");
        AppendMenu(menu, MF_STRING, ID_BG_RED, L"Red background");
        AppendMenu(menu, MF_STRING, ID_BG_GREEN, L"Green background");
        AppendMenu(menu, MF_STRING, ID_BG_BLUE, L"Blue background");

        POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        if (pt.x == -1 && pt.y == -1)
            GetCursorPos(&pt);
        int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                                 pt.x, pt.y, 0, hwnd, nullptr);
        DestroyMenu(menu);

        switch (cmd)
        {
        case ID_BG_BLACK:
            applyBackground(0.0f, 0.0f, 0.0f);
            break;
        case ID_BG_WHITE:
            applyBackground(1.0f, 1.0f, 1.0f);
            break;
        case ID_BG_RED:
            applyBackground(1.0f, 0.0f, 0.0f);
            break;
        case ID_BG_GREEN:
            applyBackground(0.0f, 1.0f, 0.0f);
            break;
        case ID_BG_BLUE:
            applyBackground(0.0f, 0.0f, 1.0f);
            break;
        default:
            break;
        }
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR lpCmdLine, int nCmdShow)
{
    // Parse command line into argc/argv style for the settings module.
    int argc = 0;
    wchar_t **wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::vector<std::string> args;
    for (int i = 0; i < argc; ++i)
    {
        int len = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, nullptr, 0,
                                      nullptr, nullptr);
        std::string s(len - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, &s[0], len, nullptr, nullptr);
        args.push_back(s);
    }
    LocalFree(wargv);

    std::vector<char *> argvPtrs;
    for (auto &s : args)
        argvPtrs.push_back(&s[0]);

    RenderBackend backend = resolveBackend((int)argvPtrs.size(), argvPtrs.data());

    // Window title: application name + used backend.
    g_baseTitle = L"Spout2 Display";
    if (backend == RenderBackend::OpenGL)
        g_baseTitle += L" - OpenGL";
    else
        g_baseTitle += L" - DirectX 12";
    const std::wstring title = g_baseTitle;

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
        0, className, title.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720,
        nullptr, nullptr, hInstance, nullptr);
    if (!hwnd)
        return 1;

    RECT rc;
    GetClientRect(hwnd, &rc);

    g_renderer = createRenderer(backend);
    if (!g_renderer->init(hwnd, rc.right, rc.bottom))
    {
        MessageBox(hwnd, L"Failed to initialize the renderer.",
                   L"Spout2 Display", MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // Main loop - continuous rendering.
    MSG msg = {};
    DWORD lastFpsTitle = 0;
    for (;;)
    {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                g_renderer->shutdown();
                g_renderer.reset();
                return (int)msg.wParam;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        g_renderer->renderFrame();
        g_renderer->present();

        // Update the title with FPS once per second when F1 is active.
        if (g_showFps)
        {
            DWORD now = GetTickCount();
            if (now - lastFpsTitle >= 500)
            {
                lastFpsTitle = now;
                wchar_t buf[128];
                swprintf(buf, 128, L"%s - %.1f FPS", g_baseTitle.c_str(),
                         g_renderer->getFps());
                SetWindowTextW(hwnd, buf);
            }
        }
    }
}