#include "stdafx.h"
#include "LightingPassCompute.h"

void LightingPassCompute::loadPipeline()
{
    createRootSignature();
    createPSO();
    createCommandList();
    loadResources();
}

void LightingPassCompute::loadResources()
{   
    D3D12_DESCRIPTOR_HEAP_DESC textureHeapDesc = {};
    textureHeapDesc.NumDescriptors = 2;
    textureHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    textureHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(device->CreateDescriptorHeap(&textureHeapDesc, IID_PPV_ARGS(&backBuffersHeap)));

    // Create the texture resource
    UINT textureWidth = 1366;
    UINT textureHeight = 768;
    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Alignment = 0;
    textureDesc.Width = textureWidth;
    textureDesc.Height = textureHeight;
    textureDesc.DepthOrArraySize = 1;
    textureDesc.MipLevels = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
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
                                    IID_PPV_ARGS(backBufferTextures[0].resource.GetAddressOf()));
    device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &textureDesc,
                                    D3D12_RESOURCE_STATE_COMMON, nullptr,
                                    IID_PPV_ARGS(backBufferTextures[1].resource.GetAddressOf()));

    CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(backBuffersHeap -> GetCPUDescriptorHandleForHeapStart());
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    uavDesc.Texture2D.MipSlice = 0;
    uavDesc.Texture2D.PlaneSlice = 0;

    device->CreateUnorderedAccessView(backBufferTextures[0].resource.Get(), nullptr, &uavDesc, hDescriptor);
    UINT hDescriptorIncrementSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    hDescriptor.Offset(hDescriptorIncrementSize);
    device->CreateUnorderedAccessView(backBufferTextures[1].resource.Get(), nullptr, &uavDesc, hDescriptor);

    cbvSrvUavHeapDescriptorSize = 
        device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void LightingPassCompute::createRootSignature() {
    CD3DX12_ROOT_PARAMETER1 slotRootParameter[3];
    CD3DX12_DESCRIPTOR_RANGE1 range;
    range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
    slotRootParameter[0].InitAsDescriptorTable(1, &range);
    
    CD3DX12_DESCRIPTOR_RANGE1 range2;
    range2.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0);
    slotRootParameter[1].InitAsDescriptorTable(1, &range2);

    CD3DX12_DESCRIPTOR_RANGE1 range3;
    range3.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3);
    slotRootParameter[2].InitAsDescriptorTable(1, &range3);

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

    D3D12_STATIC_SAMPLER_DESC samplerExact = {};
    samplerExact.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    samplerExact.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerExact.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerExact.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerExact.MipLODBias = 0;
    samplerExact.MaxAnisotropy = 0;
    samplerExact.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    //sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    samplerExact.MinLOD = 0.0f;
    samplerExact.MaxLOD = D3D12_FLOAT32_MAX;
    samplerExact.RegisterSpace = 0;
    samplerExact.ShaderRegister = 1;
    samplerExact.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_STATIC_SAMPLER_DESC samplers[2] = { sampler, samplerExact };

    rootSignatureDesc.Init_1_1(_countof(slotRootParameter), slotRootParameter, 2, samplers,
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

void LightingPassCompute::executeTasks() {}

void LightingPassCompute::executeTasks(UINT currentFrameIndex) {
    //ID3D12DescriptorHeap* descriptorHeaps[] = { backBuffersHeap.Get(), rtvHeap.Get(), dsvHeap.Get() };
    //commandList->SetDescriptorHeaps(3, descriptorHeaps);
    commandList->SetComputeRootSignature(rootSignature.Get());
    commandList->SetPipelineState(computePSO.Get());

    CD3DX12_GPU_DESCRIPTOR_HANDLE handleBackBuffers(backBuffersHeap->GetGPUDescriptorHandleForHeapStart(), 
        currentFrameIndex, cbvSrvUavHeapDescriptorSize);
    CD3DX12_GPU_DESCRIPTOR_HANDLE handleRtv(rtvHeap->GetGPUDescriptorHandleForHeapStart(),
        currentFrameIndex * 3, cbvSrvUavHeapDescriptorSize);

    commandList->SetComputeRootDescriptorTable(0, handleBackBuffers);
    commandList->SetComputeRootDescriptorTable(1, handleRtv);
    commandList->SetComputeRootDescriptorTable(2, dsvGpuHandle);

    const UINT dispatchWidth = (1366 + 32 - 1) / 32;
    const UINT dispatchHeight = (768 + 32 - 1) / 32;
    commandList->Dispatch(dispatchWidth, dispatchHeight, 1);
}