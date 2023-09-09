#pragma once
#include "LightingPassCompute.h"

class SSRPass :
    public ComputePipeline
{   
public:
    SSRPass(LPCWSTR shaderFile, LPCSTR shaderName, ComPtr<ID3D12Device> &device,
        ComPtr<ID3D12CommandAllocator> &commandAllocator)
            : ComputePipeline(shaderFile, shaderName, device, commandAllocator) {}

    Texture backBufferTextures[2];
    ComPtr<ID3D12DescriptorHeap> backBuffersHeap;
    UINT numBackBuffers = 2;
    UINT cbvSrvUavHeapDescriptorSize;
    ComPtr<ID3D12DescriptorHeap> rtvHeap;
    ComPtr<ID3D12DescriptorHeap> dsvHeap;
    ComPtr<ID3D12DescriptorHeap> cbvHeap;
    ComPtr<ID3D12DescriptorHeap> diffuseHeap;
    IBLResources iblResources;

    virtual void loadPipeline() override;
    virtual void loadResources() override;
    virtual void createRootSignature() override;
    virtual void executeTasks() override;
    void executeTasks(UINT currentFrameIndex);

    void setResources(ComPtr<ID3D12DescriptorHeap> &rtvHeap, ComPtr<ID3D12DescriptorHeap> &dsvHeap, 
        ComPtr<ID3D12DescriptorHeap> &cbvHeap, ComPtr<ID3D12DescriptorHeap> &diffuseHeap, IBLResources &iblResources)
    {
        this->rtvHeap = rtvHeap;
        this->dsvHeap = dsvHeap;
        this->cbvHeap = cbvHeap;
        this->iblResources = iblResources;
        this->diffuseHeap = diffuseHeap;
    }
};

