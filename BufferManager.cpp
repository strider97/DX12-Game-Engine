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

void BufferManager::loadMaterials(std::vector<tinygltf::Material> &gltfMaterials) {
	std::vector<Material> materials;
	for (int i = 0; i<gltfMaterials.size(); i++) {
		auto& gltfMat = gltfMaterials[i];
		Material material;
		material.roughness = gltfMat.pbrMetallicRoughness.roughnessFactor;
		auto &color = gltfMat.pbrMetallicRoughness.baseColorFactor;
		material.baseColor = { (float)color[0], (float)color[1], (float)color[2] };
		materials.push_back(material);
	}

	int bufferSize = sizeof(Material) * materials.size();
	device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(materialBuffer.GetAddressOf())
	);
	vertexBuffers.push_back(materialBuffer);

	device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&materialUploadBuffer)
	);

	void* uploadData;
	materialUploadBuffer->Map(0, nullptr, &uploadData);
	memcpy(uploadData, &materials[0], bufferSize);
	materialUploadBuffer->Unmap(0, nullptr);

	commandList->CopyBufferRegion(materialBuffer.Get(), 0, materialUploadBuffer.Get(), 0, bufferSize);
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		materialBuffer.Get(), 
		D3D12_RESOURCE_STATE_COPY_DEST, 
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
	));

	D3D12_DESCRIPTOR_HEAP_DESC materialHeapDesc = {};
        materialHeapDesc.NumDescriptors = materials.size();
        materialHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        materialHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        ThrowIfFailed(device->CreateDescriptorHeap(&materialHeapDesc, IID_PPV_ARGS(&materialHeap)));
        heapDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}


void BufferManager::loadBufferViews(tinygltf::Model &model) 
{	
	for(auto &mesh : model.meshes) {
		for (auto &primitive : mesh.primitives) {
			CD3DX12_CPU_DESCRIPTOR_HANDLE handleCpu = CD3DX12_CPU_DESCRIPTOR_HANDLE(materialHeap.Get()->GetCPUDescriptorHandleForHeapStart());
			handleCpu.Offset(primitive.material, heapDescriptorSize);
			CD3DX12_GPU_DESCRIPTOR_HANDLE handleGpu = CD3DX12_GPU_DESCRIPTOR_HANDLE(materialHeap.Get()->GetGPUDescriptorHandleForHeapStart());
			handleGpu.Offset(primitive.material, heapDescriptorSize);
			MeshPrimitive meshPrimitive (vertexBuffers, primitive, model, handleCpu, handleGpu);
			meshPrimitives.push_back(meshPrimitive);
		}
	}
}


D3D12_GPU_VIRTUAL_ADDRESS BufferManager::getGpuVirtualAddressForMaterial(int index) {
	D3D12_GPU_VIRTUAL_ADDRESS address = materialBuffer.Get()->GetGPUVirtualAddress();
	address += index * sizeof(Material);
	return address;
}