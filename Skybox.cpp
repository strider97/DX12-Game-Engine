#include "stdafx.h"
#include "Skybox.h"

Skybox::Skybox(ComPtr<ID3D12Device>& device, ComPtr<ID3D12GraphicsCommandList> commandList)
{
    this->device = device;
    this->commandList = commandList;
    compileShader();
    createRootSignature();
    loadPipeline();
    loadResources();
}

void addCubeVertices(std::vector<float> &vertices) {
    vertices = {
    -0.5f, -0.5f, -0.5f,
     0.5f, -0.5f, -0.5f,
     0.5f,  0.5f, -0.5f,
     0.5f,  0.5f, -0.5f,
    -0.5f,  0.5f, -0.5f,
    -0.5f, -0.5f, -0.5f,

    -0.5f, -0.5f,  0.5f,
     0.5f,  0.5f,  0.5f,
     0.5f, -0.5f,  0.5f,
     0.5f,  0.5f,  0.5f,
    -0.5f, -0.5f,  0.5f,
    -0.5f,  0.5f,  0.5f,

    -0.5f,  0.5f,  0.5f,
    -0.5f, -0.5f, -0.5f,
    -0.5f,  0.5f, -0.5f,
    -0.5f, -0.5f, -0.5f,
    -0.5f,  0.5f,  0.5f,
    -0.5f, -0.5f,  0.5f,

     0.5f,  0.5f,  0.5f,
     0.5f,  0.5f, -0.5f,
     0.5f, -0.5f, -0.5f,
     0.5f, -0.5f, -0.5f,
     0.5f, -0.5f,  0.5f,
     0.5f,  0.5f,  0.5f,

    -0.5f, -0.5f, -0.5f,
     0.5f, -0.5f,  0.5f,
     0.5f, -0.5f, -0.5f,
     0.5f, -0.5f,  0.5f,
    -0.5f, -0.5f, -0.5f,
    -0.5f, -0.5f,  0.5f,

    -0.5f,  0.5f, -0.5f,
     0.5f,  0.5f, -0.5f,
     0.5f,  0.5f,  0.5f,
     0.5f,  0.5f,  0.5f,
    -0.5f,  0.5f,  0.5f,
    -0.5f,  0.5f, -0.5f
    };

}

void loadVertices(ComPtr<ID3D12Device>& device, ComPtr<ID3D12GraphicsCommandList> &commandList, 
    ComPtr<ID3D12Resource> &vertexBuffer, ComPtr<ID3D12Resource> &uploadBuffer, std::vector<float> &vertices)
{
    addCubeVertices(vertices);
    UINT32 bufferSize = vertices.size() * sizeof(float);
    device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(vertexBuffer.GetAddressOf())
    );

    device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&uploadBuffer)
    );

    void* uploadData;
    uploadBuffer->Map(0, nullptr, &uploadData);
    memcpy(uploadData, &vertices[0], bufferSize);
    uploadBuffer->Unmap(0, nullptr);

    commandList->CopyBufferRegion(vertexBuffer.Get(), 0, uploadBuffer.Get(), 0, bufferSize);
    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        vertexBuffer.Get(), 
        D3D12_RESOURCE_STATE_COPY_DEST, 
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
    ));
}

void Skybox::loadPipeline()
{   
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
    psoDesc.pRootSignature = rootSignature.Get();
    psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
    psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;

    psoDesc.BlendState.RenderTarget[0].BlendEnable = true;
    psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA; // Source blending factor (e.g., source alpha)
    psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA; // Destination blending factor (e.g., (1 - source alpha))
    psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD; // Blending operation (e.g., addition)
    psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE; // Source alpha blending factor (e.g., 1)
    psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO; // Destination alpha blending factor (e.g., 0)
    psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD; // Alpha blending operation (e.g., addition)
    psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL; // Write mask for color channels (e.g., RGBA)

    psoDesc.DepthStencilState.DepthEnable = false;
    // psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;


    ThrowIfFailed(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&skyboxPSO)));
}

void Skybox::createRootSignature()
{
    CD3DX12_DESCRIPTOR_RANGE1 range;
    range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

    CD3DX12_ROOT_PARAMETER1 slotRootParameter[2];

    slotRootParameter[0].InitAsConstantBufferView(0);
    slotRootParameter[1].InitAsDescriptorTable(1, &range);

    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
    sampler.MipLODBias = 0;
    sampler.MaxAnisotropy = 0;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    //sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    sampler.MinLOD = 0.0f;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;
    sampler.RegisterSpace = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init_1_1(_countof(slotRootParameter), &slotRootParameter[0], 1, &sampler,
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

void Skybox::loadResources()
{   
    loadVertices(device, commandList, vertexBuffer, uploadBuffer, vertices);
    vertexBufferView.BufferLocation = vertexBuffer.Get()->GetGPUVirtualAddress();
    vertexBufferView.SizeInBytes = vertices.size() * sizeof(float);
    vertexBufferView.StrideInBytes = 3 * sizeof(float);

    // texture = new Texture(L"./Textures/ulmer_4k.tiff");
    // Texture::loadTextureFromFile(device, commandList, texture);

    texture = new Texture(L"./Textures/pretville_cinema_2k.hdr");
    Texture::loadHDRTexture(device, commandList, texture);

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.NumDescriptors = 1;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    ThrowIfFailed(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(descriptorHeap.GetAddressOf())));

    D3D12_SHADER_RESOURCE_VIEW_DESC imageDesc = {};
    imageDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    imageDesc.Format = texture->resource.Get()->GetDesc().Format;
    imageDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    imageDesc.Texture2D.MostDetailedMip = 0;
    imageDesc.Texture2D.MipLevels = texture->resource.Get()->GetDesc().MipLevels;
    imageDesc.Texture2D.ResourceMinLODClamp = 0.0f;
    device->CreateShaderResourceView(texture->resource.Get(), &imageDesc, descriptorHeap->GetCPUDescriptorHandleForHeapStart());

}

void Skybox::compileShader()
{
    UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
    ThrowIfFailed(D3DCompileFromFile(L"skyboxShader.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr));
    ThrowIfFailed(D3DCompileFromFile(L"skyboxShader.hlsl", nullptr, nullptr, "skyboxPS", "ps_5_0", compileFlags, 0, &pixelShader, nullptr));
}

void Skybox::draw(D3D12_GPU_VIRTUAL_ADDRESS cBufferAddress)
{
    commandList->SetGraphicsRootSignature(rootSignature.Get());
    commandList->SetPipelineState(skyboxPSO.Get());
    D3D12_VERTEX_BUFFER_VIEW bufferViews[] = { vertexBufferView };
    
    commandList->IASetVertexBuffers(0, 1, bufferViews);
    commandList->SetGraphicsRootConstantBufferView(0, cBufferAddress);
    commandList->SetGraphicsRootDescriptorTable(1, descriptorHeap.Get()->GetGPUDescriptorHandleForHeapStart());
    
    commandList->DrawInstanced(36, 1, 0, 0);
}

void Skybox::draw(D3D12_GPU_VIRTUAL_ADDRESS cBufferAddress, 
    D3D12_GPU_DESCRIPTOR_HANDLE &gpuTextureHandle) 
{
    commandList->SetGraphicsRootSignature(rootSignature.Get());
    commandList->SetPipelineState(skyboxPSO.Get());
    D3D12_VERTEX_BUFFER_VIEW bufferViews[] = { vertexBufferView };
    
    commandList->IASetVertexBuffers(0, 1, bufferViews);
    commandList->SetGraphicsRootConstantBufferView(0, cBufferAddress);
    commandList->SetGraphicsRootDescriptorTable(1, gpuTextureHandle);
    
    commandList->DrawInstanced(36, 1, 0, 0);
}
