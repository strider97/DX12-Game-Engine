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

DXGI_FORMAT getDXGIFormat (int type, int componentType) {
	if(componentType == TINYGLTF_COMPONENT_TYPE_SHORT)
		return DXGI_FORMAT_R16_SNORM;
	switch (type)
	{
	case TINYGLTF_TYPE_SCALAR:
		return DXGI_FORMAT_R32_FLOAT;
	case TINYGLTF_TYPE_VEC2:
		return DXGI_FORMAT_R32G32_FLOAT;
	case TINYGLTF_TYPE_VEC3:
		return DXGI_FORMAT_R32G32B32_FLOAT;
	case TINYGLTF_TYPE_VEC4:
		return DXGI_FORMAT_R32G32B32A32_FLOAT;
	default:
		return DXGI_FORMAT_R32_FLOAT;
	}
}

void BufferManager::loadBufferViews(
	std::vector<tinygltf::BufferView> &bufferViews, 
	std::vector<tinygltf::Accessor> &accessors,
	ComPtr<ID3D12DescriptorHeap> &bufferViewHeap) 
{	
	D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
	cbvHeapDesc.NumDescriptors = accessors.size();
	cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	ThrowIfFailed(device->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&bufferViewHeap)));
	int heapDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHeapHandle(bufferViewHeap->GetCPUDescriptorHandleForHeapStart());

	for(int i = 0; i<accessors.size(); i++) {
		auto& accessor = accessors[i];
		auto& bufferView = bufferViews[accessor.bufferView];

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = getDXGIFormat(accessor.type, accessor.componentType); // Specify the format according to your data type
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Buffer.FirstElement = bufferView.byteOffset; // Assuming float data
		srvDesc.Buffer.NumElements = accessor.count;
		srvDesc.Buffer.StructureByteStride = bufferView.byteStride;
		auto& vertexBuffer = vertexBuffers[bufferView.buffer];
		device->CreateShaderResourceView(vertexBuffer.Get(), &srvDesc, cpuHeapHandle);

		cpuHeapHandle.Offset(heapDescriptorSize);
	}
}

// { size=167652 }
