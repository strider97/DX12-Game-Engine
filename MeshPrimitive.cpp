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
    vertexBufferView.SizeInBytes = byteStride * accessor.count;
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
	CD3DX12_GPU_DESCRIPTOR_HANDLE materialHeapGpuhandle,
    CD3DX12_CPU_DESCRIPTOR_HANDLE imageHandleCpu,
    CD3DX12_GPU_DESCRIPTOR_HANDLE imageHandleGpu)
{   
    this->primitive = primitive;
    this->materialHeapCpuhandle = materialHeapCpuhandle;
    this->materialHeapGpuhandle = materialHeapGpuhandle;
    this->baseColorTextureCpuhandle = imageHandleCpu;
    this->baseColorTextureGpuhandle = imageHandleGpu;
    if (material.pbrMetallicRoughness.baseColorTexture.index >= 0);
        this->hasBaseColorTexture = true;

    auto &bufferViews = model.bufferViews;
    auto &accessors = model.accessors;
    auto &material = model.materials[max(0, primitive.material)];
    this->material = material;

    int positionIndex = primitive.attributes["POSITION"];
    int normalsIndex = primitive.attributes["NORMAL"];
    int uvIndex = primitive.attributes["TEXCOORD_0"];

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