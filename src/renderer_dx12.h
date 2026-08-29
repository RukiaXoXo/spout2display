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

#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

// DirectX 12 backend: creates a D3D12 device + swap chain and uses spoutDX12
// (SpoutDX12, D3D11On12 based) to receive the first available sender into a
// D3D12 resource, which is then drawn with a fullscreen triangle.
class DX12Renderer : public IRenderer
{
public:
    DX12Renderer() = default;
    ~DX12Renderer() override;

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
    int m_width = 0;
    int m_height = 0;
    float m_bgR = 0.0f;
    float m_bgG = 0.0f;
    float m_bgB = 0.0f;
    std::string m_preferredSender;

    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12CommandQueue> m_queue;
    ComPtr<IDXGISwapChain3> m_swapChain;
    ComPtr<ID3D12CommandAllocator> m_cmdAllocator;
    ComPtr<ID3D12GraphicsCommandList> m_cmdList;
    ComPtr<ID3D12RootSignature> m_rootSig;
    ComPtr<ID3D12PipelineState> m_pso;
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    ComPtr<ID3D12DescriptorHeap> m_srvHeap;
    ComPtr<ID3D12Fence> m_fence;

    ComPtr<ID3D12Resource> m_renderTargets[2];
    ComPtr<ID3D12Resource> m_receivedResource;
    ComPtr<ID3D12Resource> m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView = {};

    UINT m_frameIndex = 0;
    UINT m_rtvDescriptorSize = 0;
    UINT m_srvDescriptorSize = 0;
    HANDLE m_fenceEvent = nullptr;
    UINT64 m_fenceValue = 0;

    // FPS counter.
    double m_fps = 0.0;
    unsigned long long m_frameCount = 0;
    double m_lastFpsTime = 0.0;

    // Spout DX12 receiver (created lazily after the device exists).
    class spoutDX12 *m_receiver = nullptr;
    char m_senderName[256] = "";
    bool m_connected = false;

    bool createDeviceAndSwapChain();
    bool createPipeline();
    void waitForGpu();
    void recordCommandList(ID3D12Resource *texture);
};