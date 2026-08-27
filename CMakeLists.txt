cmake_minimum_required(VERSION 3.16)
project(spout2display LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# ---------------------------------------------------------------------------
# Spout2 SDK (BSD 2-Clause) - provided locally under ./spout2libs
# ---------------------------------------------------------------------------
set(SPOUT2_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/spout2libs")

# The static Spout library is built with the static MSVC runtime (/MT),
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

add_executable(spout2display WIN32
    src/main.cpp
    src/settings.cpp
    src/renderer_factory.cpp
    src/renderer_gl.cpp
    src/renderer_dx12.cpp
)

# Static linking: SPOUT_DLLEXP must stay empty (no dllimport/dllexport).
target_compile_definitions(spout2display PRIVATE
    UNICODE
    _UNICODE
)

target_include_directories(spout2display PRIVATE
    "${SPOUT2_ROOT}/include"
    "${SPOUT2_ROOT}/include/SpoutGL"
    "${SPOUT2_ROOT}/include/SpoutDX12"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(spout2display PRIVATE
    "${SPOUT2_ROOT}/MT/lib/Spout_static.lib"
    "${SPOUT2_ROOT}/MT/lib/SpoutDX_static.lib"
    "${SPOUT2_ROOT}/MT/lib/SpoutDX12_static.lib"
    opengl32
    glu32
    d3d11
    d3d12
    dxgi
    d3dcompiler
)