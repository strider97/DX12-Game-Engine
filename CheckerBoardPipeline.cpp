#include "stdafx.h"
#include "CheckerBoardPipeline.h"

void CheckerBoardPipeline::loadPipeline()
{
    createRootSignature();
    createPSO();
    createCommandList();
    loadResources();
}

void CheckerBoardPipeline::loadResources()
{
    UINT textureWidth = 1024; // Adjust the width and height as needed
    UINT textureHeight = 1024;
    DXGI_FORMAT textureFormat = DXGI_FORMAT_R32G32B32A32_FLOAT; // Choose a suitable format

    // Create the texture resource
    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Alignment = 0;
    textureDesc.Width = textureWidth;
    textureDesc.Height = textureHeight;
    textureDesc.DepthOrArraySize = 1;
    textureDesc.MipLevels = 1;
    textureDesc.Format = textureFormat;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    // Create a default heap to store the texture data
    D3D12_HEAP_PROPERTIES heapProperties = {};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
    heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProperties.CreationNodeMask = 1;
    heapProperties.VisibleNodeMask = 1;

    device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &textureDesc,
                                    D3D12_RESOURCE_STATE_COMMON, nullptr,
                                    IID_PPV_ARGS(texture.resource.GetAddressOf()));

    D3D12_DESCRIPTOR_HEAP_DESC textureHeapDesc = {};
    textureHeapDesc.NumDescriptors = 2;
    textureHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    textureHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(device->CreateDescriptorHeap(&textureHeapDesc, IID_PPV_ARGS(&textureHeap)));

    CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(textureHeap -> GetCPUDescriptorHandleForHeapStart());
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = textureFormat;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    uavDesc.Texture2D.MipSlice = 0;
    uavDesc.Texture2D.PlaneSlice = 0;
    device->CreateUnorderedAccessView(texture.resource.Get(), nullptr, &uavDesc, hDescriptor);

    UINT hDescriptorIncrementSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    hDescriptor.Offset(hDescriptorIncrementSize);
    D3D12_SHADER_RESOURCE_VIEW_DESC imageDesc = {};
    imageDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    imageDesc.Format = texture.resource.Get()->GetDesc().Format;
    imageDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    imageDesc.Texture2D.MostDetailedMip = 0;
    imageDesc.Texture2D.MipLevels = texture.resource.Get() -> GetDesc().MipLevels;
    imageDesc.Texture2D.ResourceMinLODClamp = 0.0f;
    device->CreateShaderResourceView(texture.resource.Get(), &imageDesc, hDescriptor);
    
    CD3DX12_GPU_DESCRIPTOR_HANDLE hDescriptorGPU(textureHeap -> GetGPUDescriptorHandleForHeapStart());
    hDescriptorGPU.Offset(hDescriptorIncrementSize);
    textureGpuHandle = hDescriptorGPU;
}

void CheckerBoardPipeline::createRootSignature() {
    CD3DX12_DESCRIPTOR_RANGE1 range;
    range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

    CD3DX12_ROOT_PARAMETER1 slotRootParameter[1];

    slotRootParameter[0].InitAsDescriptorTable(1, &range);
    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
    D3D12_STATIC_SAMPLER_DESC sampler = {};
    rootSignatureDesc.Init_1_1(_countof(slotRootParameter), slotRootParameter, 0, nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
    // This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
    featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
    if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
    {
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }
    ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));
    ThrowIfFailed(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature)));
}

void CheckerBoardPipeline::executeTasks() {
    ID3D12DescriptorHeap* descriptorHeaps[] = { textureHeap.Get() };
    commandList->SetDescriptorHeaps(1, descriptorHeaps);
    commandList->SetComputeRootSignature(rootSignature.Get());
    commandList->SetPipelineState(computePSO.Get());
    commandList->SetComputeRootDescriptorTable(0, textureHeap->GetGPUDescriptorHandleForHeapStart());

    const UINT dispatchWidth = 32;
    const UINT dispatchHeight = 32;
    commandList->Dispatch(dispatchWidth, dispatchHeight, 1);
}

