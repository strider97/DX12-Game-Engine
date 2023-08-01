#pragma once
#include "stdafx.h"
#include "ComputePipeline.h"
#include "BufferManager.h"

class SkyboxIrradiance :
    public ComputePipeline
{
public:
    SkyboxIrradiance(LPCWSTR shaderFile, LPCSTR shaderName, ComPtr<ID3D12Device> &device,
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

