#pragma once
#include "SkyboxIrradiance.h"
class PreFilterEnv :
    public ComputePipeline
{   
public:
    PreFilterEnv(LPCWSTR shaderFile, LPCSTR shaderName, ComPtr<ID3D12Device> &device,
        ComPtr<ID3D12CommandAllocator> &commandAllocator)
            : ComputePipeline(shaderFile, shaderName, device, commandAllocator) {}
    Texture texture;
    Texture* skyboxTexture;
    ComPtr<ID3D12DescriptorHeap> textureHeap;
    CD3DX12_GPU_DESCRIPTOR_HANDLE textureGpuHandle;
    CD3DX12_GPU_DESCRIPTOR_HANDLE skyboxTextureHandle;

    virtual void loadPipeline() override;
    virtual void loadResources() override;
    virtual void createRootSignature() override;
    virtual void executeTasks() override;
};

