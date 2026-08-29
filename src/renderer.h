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
#include <memory>
#include <string>
#include <vector>

// Backend selection for rendering + Spout receiving.
enum class RenderBackend
{
    OpenGL,
    DirectX12
};

// Abstract renderer. Each concrete backend owns its own Spout receiver and
// rendering pipeline, so the main loop only drives a generic frame cycle.
class IRenderer
{
public:
    virtual ~IRenderer() = default;

    // Create the graphics context, swap chain and Spout receiver.
    virtual bool init(HWND hwnd, int width, int height) = 0;
    // Handle window resize.
    virtual void resize(int width, int height) = 0;
    // Set the background (clear) color, RGB in 0..1 range.
    virtual void setBackgroundColor(float r, float g, float b) = 0;
    // List of currently available Spout2 senders.
    virtual std::vector<std::string> getSenderList() = 0;
    // Set the preferred sender to receive from (empty = first available).
    virtual void setSenderName(const std::string &name) = 0;
    // Receive the first available Spout sender and draw it.
    virtual void renderFrame() = 0;
    // Present the rendered frame to the window.
    virtual void present() = 0;
    // Current frames per second (updated by renderFrame).
    virtual double getFps() const = 0;
    // Release all resources.
    virtual void shutdown() = 0;
};

// Factory: creates the renderer for the requested backend.
std::unique_ptr<IRenderer> createRenderer(RenderBackend backend);