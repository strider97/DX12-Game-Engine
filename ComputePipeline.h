#pragma once
#include "DXSample.h"

class ComputePipeline
{
public:
    ComputePipeline(LPCWSTR shaderFile, LPCSTR shaderName, ComPtr<ID3D12Device> &device,
        ComPtr<ID3D12CommandAllocator> &commandAllocator);
    virtual void loadPipeline();
    virtual void createRootSignature();
    virtual void executeTasks();
    virtual void loadResources();
    
    void compileShader(LPCWSTR shaderFile, LPCSTR shaderName);
    void createPSO();
    void createCommandList();

    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc;
    ComPtr<ID3D12PipelineState> computePSO;
    ComPtr<ID3DBlob> computeShader;
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12CommandAllocator> commandAllocator;
    ComPtr<ID3D12GraphicsCommandList> commandList;
    ComPtr<ID3D12RootSignature> rootSignature;
};

