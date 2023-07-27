#include "stdafx.h"
#include "BasicGameEngine.h"
#include "BufferManager.h"

D3D12_VERTEX_BUFFER_VIEW getVertexBufferView(tinygltf::Accessor &accessor, 
    tinygltf::BufferView &bufferView, UINT64 gpuVirtualAddress) 
{
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
    UINT32 byteStride = accessor.ByteStride(bufferView);
    UINT32 byteOffset = bufferView.byteOffset + accessor.byteOffset;
    vertexBufferView.BufferLocation = gpuVirtualAddress + byteOffset;
    vertexBufferView.SizeInBytes = bufferView.byteLength;
    vertexBufferView.StrideInBytes = byteStride;

    return vertexBufferView;
}

D3D12_INDEX_BUFFER_VIEW getIndexBufferView(tinygltf::Accessor &accessor, 
    tinygltf::BufferView &bufferView, UINT64 gpuVirtualAddress) 
{
    D3D12_INDEX_BUFFER_VIEW indexBufferView;
    UINT32 byteStride = accessor.ByteStride(bufferView);
    UINT32 byteOffset = bufferView.byteOffset + accessor.byteOffset;
    indexBufferView.BufferLocation = gpuVirtualAddress + byteOffset;
    indexBufferView.SizeInBytes = 2 * accessor.count;
    indexBufferView.Format = DXGI_FORMAT_R16_SNORM;

    return indexBufferView;
}

MeshPrimitive::MeshPrimitive(
    std::vector<ComPtr<ID3D12Resource>>& buffers,
    tinygltf::Primitive& primitive,
    tinygltf::Model &model,
    CD3DX12_CPU_DESCRIPTOR_HANDLE materialHeapCpuhandle,
	CD3DX12_GPU_DESCRIPTOR_HANDLE materialHeapGpuhandle)
{   
    this->primitive = primitive;
    this->materialHeapCpuhandle = materialHeapCpuhandle;
    this->materialHeapGpuhandle = materialHeapGpuhandle;
    auto &material = model.materials[max(0, primitive.material)];
    int textureIndex = material.pbrMetallicRoughness.baseColorTexture.index;
    if (textureIndex >= 0)
        this->hasBaseColorTexture = true;

    auto &bufferViews = model.bufferViews;
    auto &accessors = model.accessors;
    this->material = material;

    int positionIndex = primitive.attributes["POSITION"];
    int normalsIndex = primitive.attributes["NORMAL"];
    int uvIndex = primitive.attributes["TEXCOORD_0"];
    int tangentIndex = primitive.attributes["TANGENT"];

    tinygltf::Accessor& accessor = accessors[positionIndex];
    tinygltf::BufferView& bufferView = bufferViews[accessor.bufferView];
    ComPtr<ID3D12Resource>& vertexBuffer = buffers[bufferView.buffer];
    this->vbViewPosition = getVertexBufferView(
        accessor,
        bufferView,
        vertexBuffer.Get()->GetGPUVirtualAddress()
    );

    accessor = accessors[normalsIndex];
    bufferView = bufferViews[accessor.bufferView];
    vertexBuffer = buffers[bufferView.buffer];
    this->vbViewNormal = getVertexBufferView(
        accessor,
        bufferView,
        vertexBuffer.Get()->GetGPUVirtualAddress()
    );

    accessor = accessors[uvIndex];
    bufferView = bufferViews[accessor.bufferView];
    vertexBuffer = buffers[bufferView.buffer];
    this->vbViewUV = getVertexBufferView(
        accessor,
        bufferView,
        vertexBuffer.Get()->GetGPUVirtualAddress()
    );

    accessor = accessors[tangentIndex];
    bufferView = bufferViews[accessor.bufferView];
    vertexBuffer = buffers[bufferView.buffer];
    this->vbViewTangent = getVertexBufferView(
        accessor,
        bufferView,
        vertexBuffer.Get()->GetGPUVirtualAddress()
    );

    accessor = accessors[primitive.indices];
    bufferView = bufferViews[accessor.bufferView];
    vertexBuffer = buffers[bufferView.buffer];
    this->indexCount = accessor.count;
    this->indexBufferView = getIndexBufferView(
        accessor,
        bufferView,
        vertexBuffer.Get()->GetGPUVirtualAddress()
    );
}

void loadTextureHeap (
    ComPtr<ID3D12Resource> &image, 
    ComPtr<ID3D12Device> &device, 
    CD3DX12_CPU_DESCRIPTOR_HANDLE &textureHeapCpuhandle) 
{
    D3D12_SHADER_RESOURCE_VIEW_DESC imageDesc = {};
    imageDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    imageDesc.Format = image.Get()->GetDesc().Format;
    imageDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    imageDesc.Texture2D.MostDetailedMip = 0;
    imageDesc.Texture2D.MipLevels = image.Get() -> GetDesc().MipLevels;
    imageDesc.Texture2D.ResourceMinLODClamp = 0.0f;
    device->CreateShaderResourceView(image.Get(), &imageDesc, textureHeapCpuhandle);
}

void MeshPrimitive::loadTextureHeaps(
    std::vector<Texture*> &images,  
    ComPtr<ID3D12Device> &device,
    int heapDescriptorSize,
    std::vector<tinygltf::Texture> &gltfTextures,
    Texture* defaultTexture,
    Texture* defaultNormalMap)
{
    int albedoTexture = material.pbrMetallicRoughness.baseColorTexture.index;
    int normalMapTexture = material.normalTexture.index;
    int metallicRoughnessTexture = material.pbrMetallicRoughness.metallicRoughnessTexture.index;
    int occlussionIndex = material.occlusionTexture.index;
    int emissiveIndex = material.emissiveTexture.index;

    D3D12_DESCRIPTOR_HEAP_DESC texturesHeapDesc = {};
    texturesHeapDesc.NumDescriptors = 5;
    texturesHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    texturesHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    ThrowIfFailed(device->CreateDescriptorHeap(&texturesHeapDesc, IID_PPV_ARGS(&texturesHeap)));

    CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(texturesHeap -> GetCPUDescriptorHandleForHeapStart());
    std::vector<int> textureIndices = {albedoTexture, normalMapTexture, metallicRoughnessTexture, occlussionIndex, emissiveIndex };
    for (int textureIndex : textureIndices) {
        if(textureIndex == -1) {
            loadTextureHeap(defaultTexture->resource, device, hDescriptor);
        } else {
            auto &image = images[gltfTextures[textureIndex].source]->resource;
            loadTextureHeap(image, device, hDescriptor);
        }
        hDescriptor.Offset(heapDescriptorSize);
    }

    if(normalMapTexture == -1) {
        CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(texturesHeap -> GetCPUDescriptorHandleForHeapStart());
        hDescriptor.Offset(heapDescriptorSize);
        loadTextureHeap(defaultNormalMap->resource, device, hDescriptor);
    }

}