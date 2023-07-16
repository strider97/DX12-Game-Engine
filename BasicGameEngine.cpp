//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************
#pragma once
#include "stdafx.h"
#include "BasicGameEngine.h"
#include <string.h>
#include "ObjLoader.h"
#include "WICTextureLoader12.h"

BasicGameEngine::BasicGameEngine(UINT width, UINT height, std::wstring name) :
    DXSample(width, height, name),
    m_frameIndex(0),
    m_pCbvDataBegin(nullptr),
    m_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
    m_scissorRect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height)),
    m_rtvDescriptorSize(0),
    m_constantBufferData{}
{
}

void BasicGameEngine::OnInit()
{
    LoadPipeline();
    loadObjects();
    LoadPipelineAssets();
    loadModels();

    loadTextureFromFile(new Texture(L"./Textures/white.png"));
}

// Load the rendering pipeline dependencies.
void BasicGameEngine::LoadPipeline()
{
    UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
    // Enable the debug layer (requires the Graphics Tools "optional feature").
    // NOTE: Enabling the debug layer after device creation will invalidate the active device.
    {
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            debugController->EnableDebugLayer();

            // Enable additional debug layers.
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
    }
#endif

    ComPtr<IDXGIFactory4> factory;
    ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

    if (m_useWarpDevice)
    {
        ComPtr<IDXGIAdapter> warpAdapter;
        ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));

        ThrowIfFailed(D3D12CreateDevice(
            warpAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device)
            ));
    }
    else
    {
        ComPtr<IDXGIAdapter1> hardwareAdapter;
        GetHardwareAdapter(factory.Get(), &hardwareAdapter);

        ThrowIfFailed(D3D12CreateDevice(
            hardwareAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device)
            ));
    }

    // Describe and create the command queue.
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

    // Describe and create the swap chain.
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = FrameCount;
    swapChainDesc.Width = m_width;
    swapChainDesc.Height = m_height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> swapChain;
    ThrowIfFailed(factory->CreateSwapChainForHwnd(
        m_commandQueue.Get(),        // Swap chain needs the queue so that it can force a flush on it.
        Win32Application::GetHwnd(),
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain
        ));

    // This sample does not support fullscreen transitions.
    ThrowIfFailed(factory->MakeWindowAssociation(Win32Application::GetHwnd(), DXGI_MWA_NO_ALT_ENTER));

    ThrowIfFailed(swapChain.As(&m_swapChain));
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // Create descriptor heaps.
    {
        // Describe and create a render target view (RTV) descriptor heap.
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.NumDescriptors = FrameCount;
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

        m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        // Describe and create a constant buffer view (CBV) descriptor heap.
        // Flags indicate that this descriptor heap can be bound to the pipeline 
        // and that descriptors contained in it can be referenced by a root table.
        D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
        cbvHeapDesc.NumDescriptors = 3;
        cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&m_cbvHeap)));
        m_cbvHeapDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }

    // Create frame resources.
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

        // Create a RTV for each frame.
        for (UINT n = 0; n < FrameCount; n++)
        {
            ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
            m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);
            rtvHandle.Offset(1, m_rtvDescriptorSize);
        }
    }

    ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));
}

// Load the sample assets.
void BasicGameEngine::LoadPipelineAssets()
{
    // Create a root signature consisting of a descriptor table with a single CBV.
    {
        D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

        // This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

        if (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
        {
            featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
        }

        CD3DX12_DESCRIPTOR_RANGE1 ranges[3];
        CD3DX12_ROOT_PARAMETER1 rootParameters[4];
        CD3DX12_ROOT_PARAMETER1 shadowRootParameters[1];

        ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
        rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_ALL);
        rootParameters[2].InitAsConstantBufferView(1);

        ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
        rootParameters[1].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_PIXEL);

        ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
        rootParameters[3].InitAsDescriptorTable(1, &ranges[2], D3D12_SHADER_VISIBILITY_PIXEL);

        shadowRootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_VERTEX);

        // Allow input layout and deny uneccessary access to certain pipeline stages.
        D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

        D3D12_STATIC_SAMPLER_DESC sampler = {};
        sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler.MipLODBias = 0;
        sampler.MaxAnisotropy = 0;
        sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        //sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        sampler.MinLOD = 0.0f;
        sampler.MaxLOD = D3D12_FLOAT32_MAX;
        sampler.ShaderRegister = 0;
        sampler.RegisterSpace = 0;
        sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 1, &sampler, rootSignatureFlags);

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC shadowRootSignatureDesc;
        shadowRootSignatureDesc.Init_1_1(_countof(shadowRootParameters), shadowRootParameters, 1, &sampler, rootSignatureFlags);

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));
        ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
        
        ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&shadowRootSignatureDesc, featureData.HighestVersion, &signature, &error));
        ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&shadowRootSignature)));

    }

    // Create the pipeline state, which includes compiling and loading shaders.
    {
        ComPtr<ID3DBlob> vertexShader;
        ComPtr<ID3DBlob> pixelShader;

        ComPtr<ID3DBlob> shadowVertexShader;
        ComPtr<ID3DBlob> shadowPixelShader;


#if defined(_DEBUG)
        // Enable better shader debugging with the graphics debugging tools.
        UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        UINT compileFlags = 0;
#endif

        ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"shaders.hlsl").c_str(), nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr));
        ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"shaders.hlsl").c_str(), nullptr, nullptr, "PSSimpleAlbedo", "ps_5_0", compileFlags, 0, &pixelShader, nullptr));

        ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"shaders.hlsl").c_str(), nullptr, nullptr, "ShadowVS", "vs_5_0", compileFlags, 0, &shadowVertexShader, nullptr));
        ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"shaders.hlsl").c_str(), nullptr, nullptr, "ShadowPS", "ps_5_0", compileFlags, 0, &shadowPixelShader, nullptr));


        // Define the vertex input layout.
        D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "UV", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 2, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        D3D12_RASTERIZER_DESC rasterizerDesc;
        ZeroMemory(&rasterizerDesc, sizeof(rasterizerDesc));
        rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
        rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
        rasterizerDesc.FrontCounterClockwise = TRUE;
        rasterizerDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
        rasterizerDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        rasterizerDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        rasterizerDesc.DepthClipEnable = TRUE;
        rasterizerDesc.MultisampleEnable = FALSE;
        rasterizerDesc.AntialiasedLineEnable = FALSE;
        rasterizerDesc.ForcedSampleCount = 0;
        rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

        D3D12_DEPTH_STENCIL_DESC depthStencilDesc;
        ZeroMemory(&depthStencilDesc, sizeof(depthStencilDesc));
        depthStencilDesc.DepthEnable = TRUE;
        depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
        depthStencilDesc.StencilEnable = FALSE;

        // create a depth stencil descriptor heap so we can get a pointer to the depth stencil buffer
        D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
        dsvHeapDesc.NumDescriptors = 2;
        dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(m_dsvHeap.GetAddressOf())));
        m_dsvHeapDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

        D3D12_RESOURCE_DESC depthStencilResourceDesc;
        depthStencilResourceDesc.Dimension =
            D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        depthStencilResourceDesc.Alignment = 0;
        depthStencilResourceDesc.Width = 1366;
        depthStencilResourceDesc.Height = 768;
        depthStencilResourceDesc.DepthOrArraySize = 1;
        depthStencilResourceDesc.MipLevels = 1;
        depthStencilResourceDesc.Format = DXGI_FORMAT_D32_FLOAT;
        depthStencilResourceDesc.SampleDesc.Count = 1;
        depthStencilResourceDesc.SampleDesc.Quality = 0;
        depthStencilResourceDesc.Layout =
            D3D12_TEXTURE_LAYOUT_UNKNOWN;
        depthStencilResourceDesc.Flags =
            D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE optClear;
        optClear.Format = DXGI_FORMAT_D32_FLOAT;
        optClear.DepthStencil.Depth = 1.0f;
        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &depthStencilResourceDesc,
            D3D12_RESOURCE_STATE_COMMON,
            &optClear,
            IID_PPV_ARGS(m_depthStencilBuffer.GetAddressOf())));

        // Describe and create the graphics pipeline state object (PSO).
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
        psoDesc.pRootSignature = m_rootSignature.Get();
        psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
        psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
        psoDesc.RasterizerState = rasterizerDesc;
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);;
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.SampleDesc.Count = 1;

        psoDesc.BlendState.RenderTarget[0].BlendEnable = true;
        psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA; // Source blending factor (e.g., source alpha)
        psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA; // Destination blending factor (e.g., (1 - source alpha))
        psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD; // Blending operation (e.g., addition)
        psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE; // Source alpha blending factor (e.g., 1)
        psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO; // Destination alpha blending factor (e.g., 0)
        psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD; // Alpha blending operation (e.g., addition)
        psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL; // Write mask for color channels (e.g., RGBA)


        ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));

        D3D12_GRAPHICS_PIPELINE_STATE_DESC shadowPipelineDesc = {};
        shadowPipelineDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
        shadowPipelineDesc.pRootSignature = m_rootSignature.Get();
        shadowPipelineDesc.VS = CD3DX12_SHADER_BYTECODE(shadowVertexShader.Get());
        shadowPipelineDesc.PS = CD3DX12_SHADER_BYTECODE(shadowPixelShader.Get());
        shadowPipelineDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        shadowPipelineDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        shadowPipelineDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);;
        shadowPipelineDesc.SampleMask = UINT_MAX;
        shadowPipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        shadowPipelineDesc.NumRenderTargets = 0;
        shadowPipelineDesc.SampleDesc.Count = 1;
        shadowPipelineDesc.RasterizerState.DepthBias = 0;
        shadowPipelineDesc.RasterizerState.DepthBiasClamp = 0.0f;
        shadowPipelineDesc.RasterizerState.SlopeScaledDepthBias = 0.0f;
        shadowPipelineDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        shadowPipelineDesc.RasterizerState.FrontCounterClockwise = TRUE;
        shadowPipelineDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;

        ThrowIfFailed(m_device->CreateGraphicsPipelineState(&shadowPipelineDesc, IID_PPV_ARGS(&m_shadowPipelineState)));

    }

    // Create the command list.
    ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), m_pipelineState.Get(), IID_PPV_ARGS(&m_commandList)));

    // Command lists are created in the recording state, but there is nothing
    // to record yet. The main loop expects it to be closed, so close it now.
    ThrowIfFailed(m_commandList->Close());

    /*
    * Create shadow pipeline state
    */

    ThrowIfFailed(m_device->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), m_shadowPipelineState.Get(), 
        IID_PPV_ARGS(&m_ShadowCommandList)));

    ThrowIfFailed(m_ShadowCommandList->Close());

    // Create shadow map
    {
        m_shadowMap = new ShadowMap(m_device.Get(), 1024, 1024);
        CD3DX12_CPU_DESCRIPTOR_HANDLE hSrv(m_cbvHeap -> GetCPUDescriptorHandleForHeapStart());
        hSrv.Offset(1, m_cbvHeapDescriptorSize);

        CD3DX12_CPU_DESCRIPTOR_HANDLE hDsv(m_dsvHeap -> GetCPUDescriptorHandleForHeapStart());
        hDsv.Offset(1, m_dsvHeapDescriptorSize);

        CD3DX12_GPU_DESCRIPTOR_HANDLE hSrvGpu(m_cbvHeap -> GetGPUDescriptorHandleForHeapStart());
        hSrvGpu.Offset(1, m_cbvHeapDescriptorSize);

        m_shadowMap->BuildDescriptors(hSrv, hSrvGpu, hDsv);
    }

    // Create the constant buffer.
    {
        const UINT constantBufferSize = sizeof(SceneConstantBuffer);    // CB size is required to be 256-byte aligned.

        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_constantBuffer)));

        // Describe and create a constant buffer view.
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
        cbvDesc.BufferLocation = m_constantBuffer->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes = constantBufferSize;
        m_device->CreateConstantBufferView(&cbvDesc, m_cbvHeap->GetCPUDescriptorHandleForHeapStart());

        // Map and initialize the constant buffer. We don't unmap this until the
        // app closes. Keeping things mapped for the lifetime of the resource is okay.
        CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
        ThrowIfFailed(m_constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_pCbvDataBegin)));
        memcpy(m_pCbvDataBegin, &m_constantBufferData, sizeof(m_constantBufferData));
    }

    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
    m_device->CreateDepthStencilView(m_depthStencilBuffer.Get(), nullptr, dsvHandle);
    m_commandList->Reset(m_commandAllocator.Get(), m_pipelineState.Get());
    m_commandList->ResourceBarrier(
        1,
        &CD3DX12_RESOURCE_BARRIER::Transition(
            m_depthStencilBuffer.Get(),
            D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_STATE_DEPTH_WRITE));

    // Create synchronization objects and wait until assets have been uploaded to the GPU.
    {
        ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
        m_fenceValue = 1;

        // Create an event handle to use for frame synchronization.
        m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (m_fenceEvent == nullptr)
        {
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }

        // Wait for the command list to execute; we are reusing the same command 
        // list in our main loop but for now, we just want to wait for setup to 
        // complete before continuing.
        WaitForPreviousFrame();
    }
}

void BasicGameEngine::loadObjects()  {
    
}

void BasicGameEngine::loadModels() {
    bufferManager = new BufferManager(m_device, m_commandQueue, m_commandList);
    GLTF_Loader::loadGltf("./Models/old_vehicle.glb", model);
    bufferManager->loadBuffers(model.buffers);
    bufferManager->loadMaterials(model.materials);
    bufferManager->loadImages(model);

    ThrowIfFailed(m_commandList->Close());
    ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
    WaitForPreviousFrame();
    ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), m_pipelineState.Get()));

    bufferManager->loadImagesHeap(model.images);
    bufferManager->loadBufferViews(model);
}

// Update frame-based values.
void BasicGameEngine::OnUpdate()
{   
    updateTime();
    updateCamera();
    const float translationSpeed = 0.005f;
    const float offsetBounds = 1.25;
    m_constantBufferData.PV = XMMatrixMultiply( *(m_camera.viewMatrix()), m_projectionMatrix );
    m_constantBufferData.LPV = directionLight.lightViewProjectionMatrix;
    m_constantBufferData.eye = { 
        XMVectorGetX(m_camera.eye), 
        XMVectorGetY(m_camera.eye), 
        XMVectorGetZ(m_camera.eye) };
    m_constantBufferData.light = { 
        XMVectorGetX(directionLight.getDirection()), 
        XMVectorGetY(directionLight.getDirection()), 
        XMVectorGetZ(directionLight.getDirection()) };
    memcpy(m_pCbvDataBegin, &m_constantBufferData, sizeof(m_constantBufferData));
}

// Render the scene.
void BasicGameEngine::OnRender()
{
    // Record all the commands we need to render the scene into the command list.

    drawShadowMap();

    PopulateCommandList();

    // Execute the command list.
    ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    // Present the frame.
    ThrowIfFailed(m_swapChain->Present(1, 0));

    WaitForPreviousFrame();
}

void BasicGameEngine::OnDestroy()
{
    // Ensure that the GPU is no longer referencing resources that are about to be
    // cleaned up by the destructor.
    WaitForPreviousFrame();

    CloseHandle(m_fenceEvent);
}

void BasicGameEngine::drawShadowMap() {
    ThrowIfFailed(m_commandAllocator->Reset());
    ThrowIfFailed(m_ShadowCommandList->Reset(m_commandAllocator.Get(), m_shadowPipelineState.Get()));

    m_ShadowCommandList->SetGraphicsRootSignature(shadowRootSignature.Get());
    ID3D12DescriptorHeap* ppHeaps[] = { m_cbvHeap.Get() };
    m_ShadowCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    CD3DX12_GPU_DESCRIPTOR_HANDLE cbv_srv_handle(m_cbvHeap->GetGPUDescriptorHandleForHeapStart());
    m_ShadowCommandList->SetGraphicsRootDescriptorTable(0, cbv_srv_handle);

    m_ShadowCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_shadowMap->Resource(),
        D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE));

    m_ShadowCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_ShadowCommandList->RSSetViewports(1, &m_shadowMap->Viewport());
    m_ShadowCommandList->RSSetScissorRects(1, &m_shadowMap->ScissorRect());
    m_ShadowCommandList->ClearDepthStencilView(m_shadowMap->Dsv(), 
        D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    m_ShadowCommandList->OMSetRenderTargets(0, nullptr, false, &m_shadowMap->Dsv());

    for (auto& meshPrimitive : bufferManager->meshPrimitives) {
        D3D12_VERTEX_BUFFER_VIEW bufferViews[] = {
            meshPrimitive.vbViewPosition, 
            meshPrimitive.vbViewNormal, 
            meshPrimitive.vbViewUV };
        m_ShadowCommandList->IASetVertexBuffers(0, 3, bufferViews);
        m_ShadowCommandList->IASetIndexBuffer(&meshPrimitive.indexBufferView);
        m_ShadowCommandList->DrawIndexedInstanced(meshPrimitive.indexCount, 1, 0, 0, 0);
    }

    m_ShadowCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_shadowMap->Resource(),
        D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ));

    m_ShadowCommandList->Close();

    // Execute the command list.
    ID3D12CommandList* ppCommandLists[] = { m_ShadowCommandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
    
    WaitForPreviousFrame();
}

// Fill the command list with all the render commands and dependent state.
void BasicGameEngine::PopulateCommandList()
{
    // Command list allocators can only be reset when the associated 
    // command lists have finished execution on the GPU; apps should use 
    // fences to determine GPU execution progress.
    ThrowIfFailed(m_commandAllocator->Reset());

    // However, when ExecuteCommandList() is called on a particular command 
    // list, that command list can then be reset at any time and must be before 
    // re-recording.
    ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), m_pipelineState.Get()));

    // Set necessary state.
    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

    ID3D12DescriptorHeap* ppHeaps[] = { m_cbvHeap.Get() };
    m_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    CD3DX12_GPU_DESCRIPTOR_HANDLE cbv_srv_handle(m_cbvHeap->GetGPUDescriptorHandleForHeapStart());
    m_commandList->SetGraphicsRootDescriptorTable(0, cbv_srv_handle);
    cbv_srv_handle.Offset(1, m_cbvHeapDescriptorSize);
    m_commandList->SetGraphicsRootDescriptorTable(1, cbv_srv_handle);
    cbv_srv_handle.Offset(1, m_cbvHeapDescriptorSize);
    m_commandList->RSSetViewports(1, &m_viewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);
    m_commandList->ClearDepthStencilView(m_dsvHeap->GetCPUDescriptorHandleForHeapStart(), 
        D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);


    // Indicate that the back buffer will be used as a render target.
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
    m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    // Record commands.
    const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
    m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    for (auto& meshPrimitive : bufferManager->meshPrimitives) {
        D3D12_VERTEX_BUFFER_VIEW bufferViews[] = {
            meshPrimitive.vbViewPosition,
            meshPrimitive.vbViewNormal,
            meshPrimitive.vbViewUV };
        D3D12_GPU_VIRTUAL_ADDRESS materialHeapAddress = bufferManager->
            getGpuVirtualAddressForMaterial(max(0, meshPrimitive.primitive.material));
    
        m_commandList->IASetVertexBuffers(0, 3, bufferViews);
        m_commandList->IASetIndexBuffer(&meshPrimitive.indexBufferView);
        m_commandList->SetGraphicsRootConstantBufferView(2, materialHeapAddress);
        if(meshPrimitive.hasBaseColorTexture)
            m_commandList->SetGraphicsRootDescriptorTable(3, meshPrimitive.baseColorTextureGpuhandle);
        else {
           m_commandList->SetGraphicsRootDescriptorTable(3, cbv_srv_handle);
        }

        m_commandList->DrawIndexedInstanced(meshPrimitive.indexCount, 1, 0, 0, 0);
    }

    for (auto& meshPrimitive : bufferManager->meshPrimitivesTransparent) {
        D3D12_VERTEX_BUFFER_VIEW bufferViews[] = {
            meshPrimitive.vbViewPosition,
            meshPrimitive.vbViewNormal,
            meshPrimitive.vbViewUV };
        D3D12_GPU_VIRTUAL_ADDRESS materialHeapAddress = bufferManager->
            getGpuVirtualAddressForMaterial(max(0, meshPrimitive.primitive.material));
    
        m_commandList->IASetVertexBuffers(0, 3, bufferViews);
        m_commandList->IASetIndexBuffer(&meshPrimitive.indexBufferView);
        m_commandList->SetGraphicsRootConstantBufferView(2, materialHeapAddress);
        if(meshPrimitive.hasBaseColorTexture)
            m_commandList->SetGraphicsRootDescriptorTable(3, meshPrimitive.baseColorTextureGpuhandle);
        else {
           m_commandList->SetGraphicsRootDescriptorTable(3, cbv_srv_handle);
        }

        m_commandList->DrawIndexedInstanced(meshPrimitive.indexCount, 1, 0, 0, 0);
    }

    // Indicate that the back buffer will now be used to present.
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    ThrowIfFailed(m_commandList->Close());
}

void BasicGameEngine::WaitForPreviousFrame()
{
    // WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
    // This is code implemented as such for simplicity. The D3D12HelloFrameBuffering
    // sample illustrates how to use fences for efficient resource usage and to
    // maximize GPU utilization.

    // Signal and increment the fence value.
    const UINT64 fence = m_fenceValue;
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fence));
    m_fenceValue++;

    // Wait until the previous frame is finished.
    if (m_fence->GetCompletedValue() < fence)
    {
        ThrowIfFailed(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}

void BasicGameEngine::updateTime() {
    auto end = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed_seconds = end - m_time_point;
    m_timeInSeconds += elapsed_seconds;

    double m_deltaTime = elapsed_seconds.count();
    double fps = 1.0 / m_deltaTime;

    //_RPT1(0, "Fps: %lf\n", fps);
    //_RPT1(0, "Frame Time: %lf ms\n\n", m_deltaTime * 1000);
    m_time_point = end;
}

void BasicGameEngine::updateCamera() {
    m_camera.translate(m_moveForward * m_speed * m_deltaTime, m_moveRight * m_speed * m_deltaTime);
    if (m_mouseClicked)
        m_camera.rotateCam(m_mouse_dx, m_mouse_dy, m_deltaTime);
}

void BasicGameEngine::OnKeyDown(UINT8 keyCode) {
    switch (keyCode) {
    case 'W':
        m_moveForward = 1;
        break;
    case 'S':
        m_moveForward = -1;
        break;
    case 'D':
        m_moveRight = 1;
        break;
    case 'A':
        m_moveRight = -1;
        break;
    default:
        ;
    }
}

void BasicGameEngine::OnKeyUp(UINT8 keyCode) {
    switch (keyCode) {
    case 'W':
    case 'S':
        m_moveForward = 0;
        break;
    case 'D':
    case 'A':
        m_moveRight = 0;
        break;
    default:
        ;
    }
}

void BasicGameEngine::OnMouseMove(int xPos, int yPos) {
    _RPT1(0, "mouse dx are: %i, %i\n\n", m_mouse_dx, m_mouse_dy);
    if (m_mouseX >= 0) {
        m_mouse_dx = xPos - m_mouseX;
    }
    if (m_mouseY >= 0) {
        m_mouse_dy = yPos - m_mouseY;
    }

    m_mouseX = xPos;
    m_mouseY = yPos;

    int dxAbs = abs(m_mouse_dx) + 2;
    int dyAbs = abs(m_mouse_dy) + 2;

    POINT cursorPos;
    GetCursorPos(&cursorPos);

    if (m_mouseClicked) {
        if (xPos < dxAbs) {
            m_mouseX = -1;
            m_mouseY = -1;
            SetCursorPos(m_width - 1 + cursorPos.x - dxAbs, cursorPos.y);
        }
        if (yPos < dyAbs) {
            m_mouseX = -1;
            m_mouseY = -1;
            SetCursorPos(cursorPos.x, m_height - 1 + cursorPos.y - dyAbs);
        }
        if (xPos > m_width - dxAbs) {
            m_mouseX = -1;
            m_mouseY = -1;
            SetCursorPos(dxAbs + cursorPos.x - xPos, cursorPos.y);
        }
        if (yPos > m_height - dyAbs) {
            m_mouseX = -1;
            m_mouseY = -1;
            SetCursorPos(cursorPos.x, 0 + cursorPos.y - yPos + dyAbs);
        }
    }

}

void BasicGameEngine::OnMouseDown(int b) {
    m_mouseClicked = true;
}

void BasicGameEngine::OnMouseUp(int b) {
    m_mouseClicked = false;
}

void BasicGameEngine::OnMouseLeave() {
    m_mouseX = -1;
    m_mouseY = -1;
    m_mouseClicked = false;
}

void BasicGameEngine::createTexture2D(int width, int height, ComPtr<ID3D12Resource> texture ) {
    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.MipLevels = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.Width = width;
    textureDesc.Height = height;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    textureDesc.DepthOrArraySize = 1;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

    ThrowIfFailed(m_device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&texture)));
}

std::vector<UINT8> GenerateTextureData(int TextureWidth, int TexturePixelSize)
{
    const UINT rowPitch = TextureWidth * TexturePixelSize;
    const UINT cellPitch = rowPitch >> 3;        // The width of a cell in the checkboard texture->
    const UINT cellHeight = TextureWidth >> 3;    // The height of a cell in the checkerboard texture->
    const UINT textureSize = rowPitch * TextureWidth;

    std::vector<UINT8> data(textureSize);
    UINT8* pData = &data[0];

    for (UINT n = 0; n < textureSize; n += TexturePixelSize)
    {
        UINT x = n % rowPitch;
        UINT y = n / rowPitch;
        UINT i = x / cellPitch;
        UINT j = y / cellHeight;

        if (i % 2 == j % 2)
        {
            pData[n] = 0x00;        // R
            pData[n + 1] = 0x00;    // G
            pData[n + 2] = 0x00;    // B
            pData[n + 3] = 0xff;    // A
        }
        else
        {
            pData[n] = 0xff;        // R
            pData[n + 1] = 0xff;    // G
            pData[n + 2] = 0xff;    // B
            pData[n + 3] = 0xff;    // A
        }
    }

    return data;
}


void BasicGameEngine::loadTextureFromFile(Texture* texture) {
    ThrowIfFailed(DirectX::LoadWICTextureFromFile(m_device.Get(), texture->filename.c_str(),
        texture->resource.GetAddressOf(), texture->decodedData, texture->subresource));

    const UINT64 uploadBufferSize = GetRequiredIntermediateSize(texture->resource.Get(), 0, 1);
    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
    auto desc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);

    ThrowIfFailed(m_device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(texture->uploadHeap.GetAddressOf())
    ));

    UpdateSubresources(m_commandList.Get(), texture->resource.Get(), texture->uploadHeap.Get(), 0, 0, 1, &texture->subresource);
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(texture->resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    m_commandList->ResourceBarrier(1, &barrier);
    

    ThrowIfFailed(m_commandList->Close());
    ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);


    // Create synchronization objects and wait until assets have been uploaded to the GPU.
    {
        ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
        m_fenceValue = 1;

        // Create an event handle to use for frame synchronization.
        m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (m_fenceEvent == nullptr)
        {
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }

        // Wait for the command list to execute; we are reusing the same command 
        // list in our main loop but for now, we just want to wait for setup to 
        // complete before continuing.
        WaitForPreviousFrame();
    }

    loadSrvHeapResources(texture);
}

void BasicGameEngine::loadSrvHeapResources(Texture* texture) {
    CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(
        m_cbvHeap -> GetCPUDescriptorHandleForHeapStart());
    hDescriptor.Offset(2, m_cbvHeapDescriptorSize);
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = texture->resource -> GetDesc().Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = texture->resource -> GetDesc().MipLevels;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
    m_device->CreateShaderResourceView(texture->resource.Get(), &srvDesc, hDescriptor);

}
