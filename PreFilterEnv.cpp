#include "stdafx.h"
#include "PreFilterEnv.h"

void PreFilterEnv::loadPipeline()
{
    createRootSignature();
    createPSO();
    createCommandList();
    loadResources();
}

void PreFilterEnv::loadResources()
{   
    UINT textureWidth = 1280; // Adjust the width and height as needed
    UINT textureHeight = 640;
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

void PreFilterEnv::createRootSignature() {
    CD3DX12_ROOT_PARAMETER1 slotRootParameter[2];
    CD3DX12_DESCRIPTOR_RANGE1 range;
    range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
    slotRootParameter[0].InitAsDescriptorTable(1, &range);
    
    CD3DX12_DESCRIPTOR_RANGE1 range2;
    range2.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    slotRootParameter[1].InitAsDescriptorTable(1, &range2);

    // CD3DX12_DESCRIPTOR_RANGE1 range3;
    // range3.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);
    // slotRootParameter[2].InitAsDescriptorTable(1, &range3);

    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.MipLODBias = 0;
    sampler.MaxAnisotropy = 0;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    //sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    sampler.MinLOD = 0.0f;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;
    sampler.RegisterSpace = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    rootSignatureDesc.Init_1_1(_countof(slotRootParameter), slotRootParameter, 1, &sampler,
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

void PreFilterEnv::executeTasks() {
    ID3D12DescriptorHeap* descriptorHeaps[] = { textureHeap.Get() };
    // CD3DX12_GPU_DESCRIPTOR_HANDLE hDescriptor(textureHeap -> GetGPUDescriptorHandleForHeapStart());
    // UINT hDescriptorIncrementSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    commandList->SetDescriptorHeaps(1, descriptorHeaps);
    commandList->SetComputeRootSignature(rootSignature.Get());
    commandList->SetPipelineState(computePSO.Get());
    commandList->SetComputeRootDescriptorTable(0, textureHeap -> GetGPUDescriptorHandleForHeapStart());
    
    commandList->SetComputeRootDescriptorTable(1, skyboxTextureHandle);

    const UINT dispatchWidth = 80;
    const UINT dispatchHeight = 40;
    commandList->Dispatch(dispatchWidth, dispatchHeight, 1);
}

