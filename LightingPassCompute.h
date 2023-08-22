#pragma once
#include "stdafx.h"
#include "ComputePipeline.h"
#include "BufferManager.h"

struct GBufferResources {
    ComPtr<ID3D12DescriptorHeap>* rtvHeap;
    ComPtr<ID3D12DescriptorHeap>* dsvHeap;
};

class LightingPassCompute :
    public ComputePipeline
{   
public:
    LightingPassCompute(LPCWSTR shaderFile, LPCSTR shaderName, ComPtr<ID3D12Device> &device,
        ComPtr<ID3D12CommandAllocator> &commandAllocator)
            : ComputePipeline(shaderFile, shaderName, device, commandAllocator) {}

    ComPtr<IDXGISwapChain3> swapChain;
    GBufferResources gBufferResources;
    Texture backBufferTextures[2];
    ComPtr<ID3D12DescriptorHeap> backBuffersHeap;
    UINT numBackBuffers = 2;
    UINT cbvSrvUavHeapDescriptorSize;
    ComPtr<ID3D12DescriptorHeap> rtvHeap;
    ComPtr<ID3D12DescriptorHeap> dsvHeap;
    D3D12_GPU_DESCRIPTOR_HANDLE rtvGpuHandle;
    D3D12_GPU_DESCRIPTOR_HANDLE dsvGpuHandle;

    virtual void loadPipeline() override;
    virtual void loadResources() override;
    virtual void createRootSignature() override;
    virtual void executeTasks() override;
    void executeTasks(UINT currentFrameIndex);

    void setResources(ComPtr<IDXGISwapChain3> &swapChain,
        ComPtr<ID3D12DescriptorHeap> &rtvHeap, ComPtr<ID3D12DescriptorHeap> &dsvHeap, 
        D3D12_GPU_DESCRIPTOR_HANDLE rtvGpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE dsvGpuHandle)
    {
        this->gBufferResources = gBufferResources;
        this->swapChain = swapChain;
        this->rtvHeap = rtvHeap;
        this->dsvHeap = dsvHeap;
        this->rtvGpuHandle = rtvGpuHandle;
        this->dsvGpuHandle = dsvGpuHandle;
    }
};

