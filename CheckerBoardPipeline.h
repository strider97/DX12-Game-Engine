#pragma once
#include "stdafx.h"
#include "ComputePipeline.h"
#include "BufferManager.h"

class CheckerBoardPipeline : public ComputePipeline
{
public:
    CheckerBoardPipeline(LPCWSTR shaderFile, LPCSTR shaderName, ComPtr<ID3D12Device> &device,
        ComPtr<ID3D12CommandAllocator> &commandAllocator)
            : ComputePipeline(shaderFile, shaderName, device, commandAllocator) {}
    Texture texture;
    ComPtr<ID3D12DescriptorHeap> textureHeap;
    CD3DX12_GPU_DESCRIPTOR_HANDLE textureGpuHandle;

    virtual void loadPipeline() override;
    virtual void loadResources() override;
    virtual void createRootSignature() override;
    virtual void executeTasks() override;
};
