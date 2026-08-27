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

#include "renderer_dx12.h"

#include <d3dcompiler.h>
#include <cstring>
#include <string>
#include <vector>

#include "SpoutDX12.h"

namespace
{
    // Fullscreen quad vertex shader (position + texture coordinates).
    const char *g_vsSource = R"(
struct VSOut { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };
VSOut main(float3 pos : POSITION, float2 uv : TEXCOORD0) {
    VSOut o;
    o.pos = float4(pos, 1.0);
    o.uv = uv;
    return o;
}
)";

    // Pixel shader: sample the received texture.
    const char *g_psSource = R"(
Texture2D tex : register(t0);
SamplerState samp : register(s0);
struct VSOut { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };
float4 main(VSOut i) : SV_Target {
    return tex.Sample(samp, i.uv);
}
)";
} // namespace

DX12Renderer::~DX12Renderer()
{
    shutdown();
}

bool DX12Renderer::init(HWND hwnd, int width, int height)
{
    m_hwnd = hwnd;
    m_width = width;
    m_height = height;

    if (!createDeviceAndSwapChain())
        return false;
    if (!createPipeline())
        return false;

    // Create the Spout DX12 receiver using our device and command queue.
    m_receiver = new spoutDX12();
    ID3D12CommandQueue *rawQueue = m_queue.Get();
    if (!m_receiver->OpenDirectX12(m_device.Get(), (IUnknown **)&rawQueue))
    {
        delete m_receiver;
        m_receiver = nullptr;
        return false;
    }

    return true;
}

bool DX12Renderer::createDeviceAndSwapChain()
{
    ComPtr<IDXGIFactory4> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))
        return false;

    // Prefer a hardware adapter.
    ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i)
    {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            continue;
        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
                                        IID_PPV_ARGS(&m_device))))
            break;
        adapter.Reset();
    }
    if (!m_device)
    {
        // Fall back to WARP (software rasterizer).
        factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter));
        D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
                          IID_PPV_ARGS(&m_device));
    }
    if (!m_device)
        return false;

    // Command queue.
    D3D12_COMMAND_QUEUE_DESC qdesc = {};
    qdesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    qdesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    if (FAILED(m_device->CreateCommandQueue(&qdesc, IID_PPV_ARGS(&m_queue))))
        return false;

    // Swap chain (double buffered).
    DXGI_SWAP_CHAIN_DESC1 scDesc = {};
    scDesc.Width = (UINT)m_width;
    scDesc.Height = (UINT)m_height;
    scDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scDesc.SampleDesc.Count = 1;
    scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scDesc.BufferCount = 2;
    scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    ComPtr<IDXGISwapChain1> swapChain1;
    if (FAILED(factory->CreateSwapChainForHwnd(m_queue.Get(), m_hwnd, &scDesc,
                                               nullptr, nullptr, &swapChain1)))
        return false;
    if (FAILED(swapChain1.As(&m_swapChain)))
        return false;
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // RTV descriptor heap.
    D3D12_DESCRIPTOR_HEAP_DESC rtvDesc = {};
    rtvDesc.NumDescriptors = 2;
    rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    if (FAILED(m_device->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&m_rtvHeap))))
        return false;
    m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
    for (UINT i = 0; i < 2; ++i)
    {
        if (FAILED(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i]))))
            return false;
        m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHandle);
        rtvHandle.ptr += m_rtvDescriptorSize;
    }

    // SRV descriptor heap (shader visible).
    D3D12_DESCRIPTOR_HEAP_DESC srvDesc = {};
    srvDesc.NumDescriptors = 16;
    srvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(m_device->CreateDescriptorHeap(&srvDesc, IID_PPV_ARGS(&m_srvHeap))))
        return false;
    m_srvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // Command allocator + list.
    if (FAILED(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                IID_PPV_ARGS(&m_cmdAllocator))))
        return false;
    if (FAILED(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                           m_cmdAllocator.Get(), nullptr,
                                           IID_PPV_ARGS(&m_cmdList))))
        return false;
    m_cmdList->Close();

    // Fence.
    if (FAILED(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence))))
        return false;
    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_fenceEvent)
        return false;

    return true;
}

bool DX12Renderer::createPipeline()
{
    // Root signature: one SRV descriptor table + one static sampler.
    D3D12_DESCRIPTOR_RANGE range = {};
    range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    range.NumDescriptors = 1;
    range.BaseShaderRegister = 0;
    range.RegisterSpace = 0;

    D3D12_ROOT_PARAMETER rootParam = {};
    rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParam.DescriptorTable.NumDescriptorRanges = 1;
    rootParam.DescriptorTable.pDescriptorRanges = &range;
    rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;
    sampler.RegisterSpace = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
    rsDesc.NumParameters = 1;
    rsDesc.pParameters = &rootParam;
    rsDesc.NumStaticSamplers = 1;
    rsDesc.pStaticSamplers = &sampler;
    rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> sigBlob, errBlob;
    if (FAILED(D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1,
                                           &sigBlob, &errBlob)))
        return false;
    if (FAILED(m_device->CreateRootSignature(0, sigBlob->GetBufferPointer(),
                                             sigBlob->GetBufferSize(),
                                             IID_PPV_ARGS(&m_rootSig))))
        return false;

    // Compile shaders.
    ComPtr<ID3DBlob> vsBlob, psBlob, shaderErr;
    if (FAILED(D3DCompile(g_vsSource, strlen(g_vsSource), nullptr, nullptr, nullptr,
                          "main", "vs_5_0", 0, 0, &vsBlob, &shaderErr)))
        return false;
    if (FAILED(D3DCompile(g_psSource, strlen(g_psSource), nullptr, nullptr, nullptr,
                          "main", "ps_5_0", 0, 0, &psBlob, &shaderErr)))
        return false;

    // Pipeline state.
    D3D12_RASTERIZER_DESC raster = {};
    raster.FillMode = D3D12_FILL_MODE_SOLID;
    raster.CullMode = D3D12_CULL_MODE_NONE;
    raster.FrontCounterClockwise = FALSE;
    raster.DepthBias = 0;
    raster.DepthBiasClamp = 0.0f;
    raster.SlopeScaledDepthBias = 0.0f;
    raster.DepthClipEnable = TRUE;
    raster.MultisampleEnable = FALSE;
    raster.AntialiasedLineEnable = FALSE;
    raster.ForcedSampleCount = 0;
    raster.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    D3D12_BLEND_DESC blend = {};
    blend.AlphaToCoverageEnable = FALSE;
    blend.IndependentBlendEnable = FALSE;
    for (UINT i = 0; i < 8; ++i)
    {
        blend.RenderTarget[i].BlendEnable = FALSE;
        blend.RenderTarget[i].LogicOpEnable = FALSE;
        blend.RenderTarget[i].SrcBlend = D3D12_BLEND_ONE;
        blend.RenderTarget[i].DestBlend = D3D12_BLEND_ZERO;
        blend.RenderTarget[i].BlendOp = D3D12_BLEND_OP_ADD;
        blend.RenderTarget[i].SrcBlendAlpha = D3D12_BLEND_ONE;
        blend.RenderTarget[i].DestBlendAlpha = D3D12_BLEND_ZERO;
        blend.RenderTarget[i].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        blend.RenderTarget[i].LogicOp = D3D12_LOGIC_OP_NOOP;
        blend.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    }

    // Vertex input layout: position (float3) + texture coordinate (float2).
    D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = {inputLayout, _countof(inputLayout)};
    psoDesc.pRootSignature = m_rootSig.Get();
    psoDesc.VS = {vsBlob->GetBufferPointer(), vsBlob->GetBufferSize()};
    psoDesc.PS = {psBlob->GetBufferPointer(), psBlob->GetBufferSize()};
    psoDesc.RasterizerState = raster;
    psoDesc.BlendState = blend;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;

    if (FAILED(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pso))))
        return false;

    // Create the fullscreen quad vertex buffer (position + uv).
    struct Vertex
    {
        float x, y, z;
        float u, v;
    };
    Vertex quad[] = {
        {-1.0f, 1.0f, 0.0f, 0.0f, 0.0f},  // top-left
        {1.0f, 1.0f, 0.0f, 1.0f, 0.0f},   // top-right
        {-1.0f, -1.0f, 0.0f, 0.0f, 1.0f}, // bottom-left
        {1.0f, -1.0f, 0.0f, 1.0f, 1.0f},  // bottom-right
    };

    D3D12_RESOURCE_DESC vbDesc = {};
    vbDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    vbDesc.Width = sizeof(quad);
    vbDesc.Height = 1;
    vbDesc.DepthOrArraySize = 1;
    vbDesc.MipLevels = 1;
    vbDesc.SampleDesc.Count = 1;
    vbDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    D3D12_HEAP_PROPERTIES uploadHeap = {};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;

    if (FAILED(m_device->CreateCommittedResource(
            &uploadHeap, D3D12_HEAP_FLAG_NONE, &vbDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&m_vertexBuffer))))
        return false;

    void *pData = nullptr;
    if (FAILED(m_vertexBuffer->Map(0, nullptr, &pData)))
        return false;
    memcpy(pData, quad, sizeof(quad));
    m_vertexBuffer->Unmap(0, nullptr);

    m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
    m_vertexBufferView.StrideInBytes = sizeof(Vertex);
    m_vertexBufferView.SizeInBytes = sizeof(quad);

    return true;
}

void DX12Renderer::waitForGpu()
{
    ++m_fenceValue;
    m_queue->Signal(m_fence.Get(), m_fenceValue);
    if (m_fence->GetCompletedValue() < m_fenceValue)
    {
        m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent);
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }
}

void DX12Renderer::resize(int width, int height)
{
    if (width == m_width && height == m_height)
        return;
    m_width = width;
    m_height = height;

    waitForGpu();
    for (UINT i = 0; i < 2; ++i)
        m_renderTargets[i].Reset();

    DXGI_SWAP_CHAIN_DESC1 desc = {};
    m_swapChain->GetDesc1(&desc);
    desc.Width = (UINT)width;
    desc.Height = (UINT)height;
    m_swapChain->ResizeBuffers(2, (UINT)width, (UINT)height, desc.Format,
                               desc.Flags);
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
    for (UINT i = 0; i < 2; ++i)
    {
        m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i]));
        m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHandle);
        rtvHandle.ptr += m_rtvDescriptorSize;
    }
}

void DX12Renderer::renderFrame()
{
    if (!m_receiver)
        return;

    // Find the first available sender.
    std::vector<std::string> senders = m_receiver->GetSenderList();
    if (!senders.empty())
    {
        const char *name = senders[0].c_str();
        if (!m_connected || std::strcmp(m_senderName, name) != 0)
        {
            m_receiver->ReleaseReceiver();
            m_connected = false;
            m_receiver->SetReceiverName(name);
            std::strncpy(m_senderName, name, sizeof(m_senderName) - 1);
            m_senderName[sizeof(m_senderName) - 1] = '\0';
            m_connected = true;
        }
    }

    if (m_connected)
    {
        if (m_receiver->ReceiveDX12Resource(m_receivedResource.GetAddressOf()))
        {
            // When the sender is new or changed, (re)create the receiving
            // D3D12 texture resource and its shader resource view.
            if (m_receiver->IsUpdated())
            {
                DXGI_FORMAT fmt = m_receiver->GetSenderFormat();
                if (m_receiver->CreateDX12texture(
                        m_device.Get(),
                        m_receiver->GetSenderWidth(),
                        m_receiver->GetSenderHeight(),
                        D3D12_RESOURCE_STATE_COPY_DEST,
                        fmt,
                        m_receivedResource.GetAddressOf()))
                {
                    D3D12_CPU_DESCRIPTOR_HANDLE srvCpu(
                        m_srvHeap->GetCPUDescriptorHandleForHeapStart());
                    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
                    srvDesc.Shader4ComponentMapping =
                        D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                    srvDesc.Format = fmt; // SRV format must match the texture
                    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                    srvDesc.Texture2D.MipLevels = 1;
                    m_device->CreateShaderResourceView(
                        m_receivedResource.Get(), &srvDesc, srvCpu);
                }
            }
        }
        else
        {
            m_connected = false;
            m_receiver->ReleaseReceiver();
        }
    }

    recordCommandList(m_receivedResource.Get());
}

void DX12Renderer::recordCommandList(ID3D12Resource *texture)
{
    waitForGpu();

    m_cmdAllocator->Reset();
    m_cmdList->Reset(m_cmdAllocator.Get(), m_pso.Get());

    // Set the viewport and scissor rect (required, otherwise nothing draws).
    D3D12_VIEWPORT viewport = {0.0f, 0.0f, (float)m_width, (float)m_height,
                               0.0f, 1.0f};
    D3D12_RECT scissor = {0, 0, m_width, m_height};
    m_cmdList->RSSetViewports(1, &viewport);
    m_cmdList->RSSetScissorRects(1, &scissor);

    // Transition back buffer to render target.
    ID3D12Resource *backBuffer = m_renderTargets[m_frameIndex].Get();
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = backBuffer;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_cmdList->ResourceBarrier(1, &barrier);

    D3D12_CPU_DESCRIPTOR_HANDLE rtv(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
    rtv.ptr += m_frameIndex * m_rtvDescriptorSize;
    m_cmdList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
    float clear[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    m_cmdList->ClearRenderTargetView(rtv, clear, 0, nullptr);

    if (texture)
    {
        // Bind the SRV created when the receiving texture was (re)created.
        m_cmdList->SetGraphicsRootSignature(m_rootSig.Get());
        ID3D12DescriptorHeap *heaps[] = {m_srvHeap.Get()};
        m_cmdList->SetDescriptorHeaps(1, heaps);
        D3D12_GPU_DESCRIPTOR_HANDLE srvGpu(
            m_srvHeap->GetGPUDescriptorHandleForHeapStart());
        m_cmdList->SetGraphicsRootDescriptorTable(0, srvGpu);
        m_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        m_cmdList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
        m_cmdList->DrawInstanced(4, 1, 0, 0);
    }

    // Transition back buffer to present.
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    m_cmdList->ResourceBarrier(1, &barrier);

    m_cmdList->Close();

    ID3D12CommandList *lists[] = {m_cmdList.Get()};
    m_queue->ExecuteCommandLists(1, lists);
}

void DX12Renderer::present()
{
    m_swapChain->Present(1, 0);
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}

void DX12Renderer::shutdown()
{
    if (m_queue && m_fence)
        waitForGpu();

    if (m_receiver)
    {
        delete m_receiver;
        m_receiver = nullptr;
    }
    if (m_fenceEvent)
    {
        CloseHandle(m_fenceEvent);
        m_fenceEvent = nullptr;
    }
    m_receivedResource.Reset();
    m_vertexBuffer.Reset();
    m_renderTargets[0].Reset();
    m_renderTargets[1].Reset();
    m_pso.Reset();
    m_rootSig.Reset();
    m_cmdList.Reset();
    m_cmdAllocator.Reset();
    m_srvHeap.Reset();
    m_rtvHeap.Reset();
    m_swapChain.Reset();
    m_queue.Reset();
    m_fence.Reset();
    m_device.Reset();
}