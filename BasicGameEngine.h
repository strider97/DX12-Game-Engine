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
#include "DXSample.h"
#include <chrono>
#include <ctime>  
#include "Camera.cpp"
#include "DirectionLight.cpp"
#include "ShadowMap.h"
#include "GLTF_Loader.h"
#include "tiny_gltf.h"
#include "BufferManager.h"
#include "CheckerBoardPipeline.h"
#include "SkyboxIrradiance.h"
#include "Skybox.h"
#include "PreFilterEnv.h"
#include "LightingPassCompute.h"

using namespace DirectX;

// Note that while ComPtr is used to manage the lifetime of resources on the CPU,
// it has no understanding of the lifetime of resources on the GPU. Apps must account
// for the GPU lifetime of resources to avoid destroying objects that may still be
// referenced by the GPU.
// An example of this can be found in the class method: OnDestroy().
using Microsoft::WRL::ComPtr;

struct Vertex
{
    XMFLOAT3 position;
    XMFLOAT3 normal;
    XMFLOAT2 uv;
};

// struct Texture
// {
//     // Unique material name for lookup.
//     std::wstring filename;
//     Microsoft::WRL::ComPtr<ID3D12Resource> resource =
//         nullptr;
//     Microsoft::WRL::ComPtr<ID3D12Resource> uploadHeap =
//         nullptr;
//     std::unique_ptr<uint8_t[]> decodedData;
//     D3D12_SUBRESOURCE_DATA subresource;

//     Texture(std::wstring filename) {
//         this->filename = filename;
//     }
// };

class BasicGameEngine : public DXSample
{
public:
    BasicGameEngine(UINT width, UINT height, std::wstring name);

    virtual void OnInit();
    virtual void OnUpdate();
    virtual void OnRender();
    virtual void OnDestroy();
    void drawShadowMap();
    virtual void OnKeyDown(UINT8);
    virtual void OnKeyUp(UINT8);
    virtual void OnMouseMove(int, int);
    virtual void OnMouseDown(int);
    virtual void OnMouseUp(int);
    virtual void OnMouseLeave();

private:
    static const UINT FrameCount = 2;

    struct SceneConstantBuffer
    {
        DirectX::XMMATRIX PV;
        DirectX::XMMATRIX LPV;
        DirectX::XMMATRIX invPV;
        XMFLOAT3 eye;
        float pad;
        XMFLOAT3 light;
        float padding[9]; // Padding so the constant buffer is 256-byte aligned.
    };
    static_assert((sizeof(SceneConstantBuffer) % 256) == 0, "Constant Buffer size must be 256-byte aligned");

    // Pipeline objects.
    CD3DX12_VIEWPORT m_viewport;
    CD3DX12_RECT m_scissorRect;
    ComPtr<IDXGISwapChain3> m_swapChain;
    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12Resource> m_renderTargets[FrameCount * 3];
    ComPtr<ID3D12Resource> m_backBuffers[2];
    ComPtr<ID3D12CommandAllocator> m_commandAllocator;
    ComPtr<ID3D12CommandAllocator> m_computeCommandAllocator;
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<ID3D12CommandQueue> computeCommandQueue;
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12RootSignature> shadowRootSignature;
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    ComPtr<ID3D12DescriptorHeap> m_gbufferRtvHeap;
    ComPtr<ID3D12DescriptorHeap> m_cbvHeap;
    ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
    ComPtr<ID3D12DescriptorHeap> m_gbufferDsvHeap;
    ComPtr<ID3D12PipelineState> m_pipelineState;
    ComPtr<ID3D12PipelineState> m_shadowPipelineState;
    ComPtr<ID3D12GraphicsCommandList> m_commandList;
    ComPtr<ID3D12GraphicsCommandList> m_ShadowCommandList;
    ComPtr<ID3D12DescriptorHeap> bufferViewHeap;
    UINT m_rtvDescriptorSize;
    UINT m_cbvHeapDescriptorSize;
    UINT m_dsvHeapDescriptorSize;
    Skybox* skybox;
    bool isFirstFrame = true;
       
    // App resources.
    ComPtr<ID3D12Resource> m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
    ComPtr<ID3D12Resource> m_constantBuffer;
    ComPtr<ID3D12Resource> m_depthStencilBuffer;
    SceneConstantBuffer m_constantBufferData;
    UINT8* m_pCbvDataBegin;
    std::chrono::duration<double> m_timeInSeconds = std::chrono::duration<double> (0);
    std::chrono::system_clock::time_point m_time_point = std::chrono::system_clock::now();
    double m_deltaTime = 1.0 / 144;
    float m_FoV = 60;
    float m_speed = 10;
    int m_moveForward = 0;
    int m_moveRight = 0;
    int m_mouseX = -1;
    int m_mouseY = -1;
    int m_mouse_dx = 0;
    int m_mouse_dy = 0;
    bool m_mouseClicked = false;
    std::vector<Vertex> m_vertices;
    Texture* sometexture;
    CheckerBoardPipeline* checkerboardPipeline;
    SkyboxIrradiance* skyboxIrradianceMap;
    PreFilterEnv* preFilterEnvMap;
    LightingPassCompute* lightingPass;

    DirectX::XMMATRIX m_projectionMatrix = XMMatrixPerspectiveFovRH(XMConvertToRadians(m_FoV), 16.0/9, 0.1f, 100.0f);
    Camera m_camera = Camera();
    DirectionLight directionLight = DirectionLight();
    ShadowMap* m_shadowMap;
    tinygltf::Model model;
    BufferManager* bufferManager;

    // Synchronization objects.
    UINT m_frameIndex;
    HANDLE m_fenceEvent;
    ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValue;
    HANDLE m_fenceEventCompute;
    ComPtr<ID3D12Fence> m_fenceCompute;
    UINT64 m_fenceValueCompute;

    void LoadPipeline();
    void LoadPipelineAssets();
    void PopulateCommandList();
    void WaitForPreviousFrame();
    void WaitForComputeTask();
    void updateTime();
    void updateCamera();
    void loadObjects();
    void loadModels();
    void loadGltfModel();
    void createTexture2D(int width, int height, ComPtr<ID3D12Resource> texture);
    void loadTextureFromFile(Texture* texture);
    void loadSrvHeapResources(Texture* texture);
};