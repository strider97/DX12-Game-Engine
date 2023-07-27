#include "stdafx.h"
#include "ComputePipeline.h"

ComputePipeline::ComputePipeline(LPCWSTR shaderFile, LPCSTR shaderName, ComPtr<ID3D12Device> &device,
                                 ComPtr<ID3D12CommandAllocator> &commandAllocator)
{
    this->device = device;
    this->commandAllocator = commandAllocator;
    compileShader(shaderFile, shaderName);
}

void ComputePipeline::loadPipeline() {
    createRootSignature();
    createPSO();
    createCommandList();
    loadResources();
}

void ComputePipeline::loadResources(){}

void ComputePipeline::compileShader(LPCWSTR shaderFile, LPCSTR shaderName)
{
    ThrowIfFailed(D3DCompileFromFile(shaderFile, nullptr, nullptr, shaderName, "cs_5_0", D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
                                     0, &this->computeShader, nullptr));
}

void ComputePipeline::createPSO()
{
    psoDesc.CS = CD3DX12_SHADER_BYTECODE(this->computeShader.Get());
    psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
    psoDesc.pRootSignature = rootSignature.Get();
    ThrowIfFailed(device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&computePSO)));
}

void ComputePipeline::createCommandList()
{
    ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE,
                                            commandAllocator.Get(), computePSO.Get(), IID_PPV_ARGS(&commandList)));
}

void ComputePipeline::createRootSignature()
{
    CD3DX12_DESCRIPTOR_RANGE srvTable;
    srvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    CD3DX12_DESCRIPTOR_RANGE uavTable;
    uavTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

    CD3DX12_ROOT_PARAMETER slotRootParameter[3];
    // Perfomance TIP: Order from most frequent to least frequent.

    slotRootParameter[0].InitAsConstants(12, 0);
    slotRootParameter[1].InitAsDescriptorTable(1, &srvTable);
    slotRootParameter[2].InitAsDescriptorTable(1, &uavTable);
    // A root signature is an array of root parameters.
    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(3, slotRootParameter, 0, nullptr,
                                            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    // create a root signature with a single slot which points to a
    // descriptor range consisting of a single constant buffer
    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    HRESULT hr =
        D3D12SerializeRootSignature(&rootSigDesc,
                                    D3D_ROOT_SIGNATURE_VERSION_1,
                                    serializedRootSig.GetAddressOf(),
                                    errorBlob.GetAddressOf());
    if (errorBlob != nullptr)
    {
        ::OutputDebugStringA((char *)errorBlob->GetBufferPointer());
    }
    ThrowIfFailed(hr);
    ThrowIfFailed(device->CreateRootSignature(
        0,
        serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(rootSignature.GetAddressOf())));
}

void ComputePipeline::executeTasks() {}