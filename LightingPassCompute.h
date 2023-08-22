#pragma once
#include "stdafx.h"
#include "ComputePipeline.h"
#include "BufferManager.h"

struct IBLResources {
    D3D12_GPU_DESCRIPTOR_HANDLE lut;
    D3D12_GPU_DESCRIPTOR_HANDLE irradianceMap;
    D3D12_GPU_DESCRIPTOR_HANDLE preFilterEnvMap;
};

class LightingPassCompute :
    public ComputePipeline
{   
public:
    LightingPassCompute(LPCWSTR shaderFile, LPCSTR shaderName, ComPtr<ID3D12Device> &device,
        ComPtr<ID3D12CommandAllocator> &commandAllocator)
            : ComputePipeline(shaderFile, shaderName, device, commandAllocator) {}

    Texture backBufferTextures[2];
    ComPtr<ID3D12DescriptorHeap> backBuffersHeap;
    UINT numBackBuffers = 2;
    UINT cbvSrvUavHeapDescriptorSize;
    ComPtr<ID3D12DescriptorHeap> rtvHeap;
    ComPtr<ID3D12DescriptorHeap> dsvHeap;
    ComPtr<ID3D12DescriptorHeap> cbvHeap;
    IBLResources iblResources;

    virtual void loadPipeline() override;
    virtual void loadResources() override;
    virtual void createRootSignature() override;
    virtual void executeTasks() override;
    void executeTasks(UINT currentFrameIndex);

    void setResources(ComPtr<ID3D12DescriptorHeap> &rtvHeap, ComPtr<ID3D12DescriptorHeap> &dsvHeap, 
        ComPtr<ID3D12DescriptorHeap> &cbvHeap, IBLResources &iblResources)
    {
        this->rtvHeap = rtvHeap;
        this->dsvHeap = dsvHeap;
        this->cbvHeap = cbvHeap;
        this->iblResources = iblResources;
    }
};

