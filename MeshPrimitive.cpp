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
    indexBufferView.Format = DXGI_FORMAT_R16_UINT;

    return indexBufferView;
}

MeshPrimitive::MeshPrimitive(
    std::vector<ComPtr<ID3D12Resource>>& vertexBuffers,
    tinygltf::Primitive& primitive,
    std::vector <tinygltf::BufferView>& bufferViews,
    std::vector <tinygltf::Accessor>& accessors,
    tinygltf::Material& material)
{
    this->material = material;

    int positionIndex = primitive.attributes["POSITION"];
    int normalsIndex = primitive.attributes["NORMAL"];
    int uvIndex = primitive.attributes["TEXCOORD_0"];

    tinygltf::Accessor& accessor = accessors[positionIndex];
    tinygltf::BufferView& bufferView = bufferViews[accessor.bufferView];
    ComPtr<ID3D12Resource>& vertexBuffer = vertexBuffers[bufferView.buffer];
    this->vbViewPosition = getVertexBufferView(
        accessor,
        bufferView,
        vertexBuffer.Get()->GetGPUVirtualAddress()
    );

    accessor = accessors[normalsIndex];
    bufferView = bufferViews[accessor.bufferView];
    vertexBuffer = vertexBuffers[bufferView.buffer];
    this->vbViewNormal = getVertexBufferView(
        accessor,
        bufferView,
        vertexBuffer.Get()->GetGPUVirtualAddress()
    );

    accessor = accessors[uvIndex];
    bufferView = bufferViews[accessor.bufferView];
    vertexBuffer = vertexBuffers[bufferView.buffer];
    this->vbViewUV = getVertexBufferView(
        accessor,
        bufferView,
        vertexBuffer.Get()->GetGPUVirtualAddress()
    );

    accessor = accessors[primitive.indices];
    bufferView = bufferViews[accessor.bufferView];
    vertexBuffer = vertexBuffers[bufferView.buffer];
    this->indexBufferView = getIndexBufferView(
        accessor,
        bufferView,
        vertexBuffer.Get()->GetGPUVirtualAddress()
    );

}