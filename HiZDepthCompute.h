#pragma once
#include "stdafx.h"
#include "ComputePipeline.h"
#include "BufferManager.h"

class HiZDepthCompute :
    public ComputePipeline
{
public:
    HiZDepthCompute(LPCWSTR shaderFile, LPCSTR shaderName, ComPtr<ID3D12Device> &device,
        ComPtr<ID3D12CommandAllocator> &commandAllocator)
            : ComputePipeline(shaderFile, shaderName, device, commandAllocator) {}
    Texture texture;
    ComPtr<ID3D12DescriptorHeap> textureHeap;
    CD3DX12_GPU_DESCRIPTOR_HANDLE textureGpuHandle;
    CD3DX12_GPU_DESCRIPTOR_HANDLE depthTextureHandle;

    static int WIDTH;
    static int HEIGHT;

    virtual void loadPipeline() override;
    virtual void loadResources() override;
    virtual void createRootSignature() override;
    void executeTasks(UINT64 &m_fenceValue, ComPtr<ID3D12CommandQueue> &computeCommandQueue, 
        HANDLE &m_fenceEvent, ComPtr<ID3D12Fence> &m_fence);

private:
    void dispatchFor(int level, UINT dispatchWidth, UINT dispatchHeight);
    void HiZDepthCompute::WaitForComputeTask(UINT64 &m_fenceValue, 
        ComPtr<ID3D12CommandQueue> &computeCommandQueue, 
        HANDLE &m_fenceEvent, ComPtr<ID3D12Fence> &m_fence
    );
};

