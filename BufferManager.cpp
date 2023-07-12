#include "stdafx.h"
#include "BufferManager.h"
#include "tiny_gltf.h"

BufferManager::BufferManager(ComPtr<ID3D12Device> &m_device, ComPtr<ID3D12GraphicsCommandList>& commandList) {
	this->device = m_device;
	this->commandList = commandList;
}

void BufferManager::loadBuffers(std::vector<tinygltf::Buffer> &buffers) {
	for (int i = 0; i<buffers.size(); i++) {
		auto& buffer = buffers[i];
		ComPtr<ID3D12Resource> vertexBuffer;
		int bufferSize = buffer.data.size();
        device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(vertexBuffer.GetAddressOf())
        );
		vertexBuffers.push_back(vertexBuffer);

		ComPtr<ID3D12Resource> uploadBuffer;
		device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&uploadBuffer)
		);
		uploadBuffers.push_back(uploadBuffer);

		void* uploadData;
		uploadBuffer->Map(0, nullptr, &uploadData);
		memcpy(uploadData, &buffer.data[0], bufferSize);
		uploadBuffer->Unmap(0, nullptr);

		commandList->CopyBufferRegion(vertexBuffers[i].Get(), 0, uploadBuffers[i].Get(), 0, bufferSize);
		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
			vertexBuffers[i].Get(), 
			D3D12_RESOURCE_STATE_COPY_DEST, 
			D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
		));
  	}
}

void BufferManager::loadBufferViews(tinygltf::Model &model) 
{	
	for(auto &mesh : model.meshes) {
		for (auto &primitive : mesh.primitives) {
			MeshPrimitive meshPrimitive (vertexBuffers, primitive, model.bufferViews, 
			model.accessors, model.materials[primitive.material]);
			meshPrimitives.push_back(meshPrimitive);
		}
	}
}