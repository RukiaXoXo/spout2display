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

#include "settings.h"

#include <windows.h>
#include <cstring>
#include <string>

namespace
{
    RenderBackend parseBackend(const std::string &value)
    {
        if (value == "opengl" || value == "gl")
            return RenderBackend::OpenGL;
        return RenderBackend::DirectX12; // default
    }

    // Returns the directory of the current executable.
    std::wstring exeDirectory()
    {
        wchar_t buf[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, buf, MAX_PATH);
        std::wstring path(buf);
        size_t pos = path.find_last_of(L"\\/");
        if (pos != std::wstring::npos)
            path = path.substr(0, pos + 1);
        return path;
    }
} // namespace

RenderBackend resolveBackend(int argc, char **argv)
{
    RenderBackend backend = RenderBackend::DirectX12; // default

    // 1) Settings file next to the executable.
    std::wstring iniPath = exeDirectory() + L"spout2display.ini";
    wchar_t value[64] = {};
    DWORD len = GetPrivateProfileStringW(L"General", L"renderer", L"",
                                         value, 64, iniPath.c_str());
    if (len > 0)
    {
        int size = WideCharToMultiByte(CP_UTF8, 0, value, (int)len, nullptr, 0,
                                       nullptr, nullptr);
        std::string str(size, '\0');
        WideCharToMultiByte(CP_UTF8, 0, value, (int)len, &str[0], size,
                            nullptr, nullptr);
        backend = parseBackend(str);
    }

    // 2) Command line overrides the settings file.
    for (int i = 1; i < argc; ++i)
    {
        const char *arg = argv[i];
        if (std::strncmp(arg, "--renderer=", 11) == 0)
            backend = parseBackend(arg + 11);
    }

    return backend;
}