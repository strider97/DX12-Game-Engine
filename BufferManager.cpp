#include "stdafx.h"
#include "BufferManager.h"
#include "WICTextureLoader12.h"

BufferManager::BufferManager(ComPtr<ID3D12Device> &m_device, 
		ComPtr<ID3D12CommandQueue> &commandQueue, 
		ComPtr<ID3D12GraphicsCommandList>& commandList) {
	this->device = m_device;
	this->commandQueue = commandQueue;
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
		this->buffers.push_back(vertexBuffer);

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

		commandList->CopyBufferRegion(this->buffers[i].Get(), 0, uploadBuffers[i].Get(), 0, bufferSize);
		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
			this->buffers[i].Get(), 
			D3D12_RESOURCE_STATE_COPY_DEST, 
			D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
		));
  	}
}

void BufferManager::loadMaterials(std::vector<tinygltf::Material> &gltfMaterials) {
	std::vector<MaterialProperties> materials;
	for (int i = 0; i<gltfMaterials.size(); i++) {
		auto& gltfMat = gltfMaterials[i];
		MaterialProperties material;
		material.roughness = gltfMat.pbrMetallicRoughness.roughnessFactor;
		auto &color = gltfMat.pbrMetallicRoughness.baseColorFactor;
		auto &emission = gltfMat.emissiveFactor;
		material.baseColor = { (float)color[0], (float)color[1], (float)color[2], (float) color[3] };
		material.metallic = gltfMat.pbrMetallicRoughness.metallicFactor;
		material.emission = { (float)emission[0], (float)emission[1], (float)emission[2] };
		materials.push_back(material);
	}

	int bufferSize = sizeof(MaterialProperties) * materials.size();
	device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(materialBuffer.GetAddressOf())
	);

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
			if (primitive.material < 0)
				continue;
			CD3DX12_CPU_DESCRIPTOR_HANDLE handleCpu = CD3DX12_CPU_DESCRIPTOR_HANDLE(materialHeap.Get()->GetCPUDescriptorHandleForHeapStart());
			handleCpu.Offset(primitive.material, heapDescriptorSize);
			CD3DX12_GPU_DESCRIPTOR_HANDLE handleGpu = CD3DX12_GPU_DESCRIPTOR_HANDLE(materialHeap.Get()->GetGPUDescriptorHandleForHeapStart());
			handleGpu.Offset(primitive.material, heapDescriptorSize);

			MeshPrimitive meshPrimitive (buffers, primitive, model, handleCpu, handleGpu);
			meshPrimitive.loadTextureHeaps(images, device, heapDescriptorSize, model.textures, defaultTexture, defaultNormalMap);

			if(meshPrimitive.material.alphaMode != "OPAQUE")
				meshPrimitivesTransparent.push_back(meshPrimitive);
			else
				meshPrimitives.push_back(meshPrimitive);
		}
	}
	meshPrimitives.insert(meshPrimitives.end(), 
	meshPrimitivesTransparent.begin(), meshPrimitivesTransparent.end());
}


D3D12_GPU_VIRTUAL_ADDRESS BufferManager::getGpuVirtualAddressForMaterial(int index) {
	D3D12_GPU_VIRTUAL_ADDRESS address = materialBuffer.Get()->GetGPUVirtualAddress();
	address += index * sizeof(MaterialProperties);
	return address;
}


void BufferManager::loadImages2(tinygltf::Model &model) {
	auto &images = model.images;
	for (int i = 0; i<images.size(); i++) {
		auto &image = images[i];
		ComPtr<ID3D12Resource> imageBuffer;
		auto &bufferView = model.bufferViews[image.bufferView];
		auto &buffer = model.buffers[bufferView.buffer];
		int imageSize = bufferView.byteLength;
        device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(imageSize),
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(imageBuffer.GetAddressOf())
        );
		imageBuffers.push_back(imageBuffer);

		ComPtr<ID3D12Resource> imageUploadBuffer;
		device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(imageSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&imageUploadBuffer)
		);
		imageUploadBuffers.push_back(imageUploadBuffer);

		void* uploadData;
		imageUploadBuffer->Map(0, nullptr, &uploadData);
		memcpy(uploadData, &buffer.data[0] + bufferView.byteOffset, imageSize);
		imageUploadBuffer->Unmap(0, nullptr);

		commandList->CopyBufferRegion(imageBuffers[i].Get(), 0, imageUploadBuffers[i].Get(), 0, imageSize);
		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
			imageBuffers[i].Get(), 
			D3D12_RESOURCE_STATE_COPY_DEST, 
			D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
		));
	}
}


void createTextureFromMemory(ComPtr<ID3D12Device> &device, ComPtr<ID3D12GraphicsCommandList> &commandList, const uint8_t* textureData, const size_t textureDataSize, Texture *texture)
{	

	/*
	HRESULT DirectX::LoadWICTextureFromMemoryEx(
    ID3D12Device* d3dDevice,
    const uint8_t* wicData,
    size_t wicDataSize,
    size_t maxsize,
    D3D12_RESOURCE_FLAGS resFlags,
    WIC_LOADER_FLAGS loadFlags,
    ID3D12Resource** texture,
    std::unique_ptr<uint8_t[]>& decodedData,
    D3D12_SUBRESOURCE_DATA& subresource) noexcept

	ID3D12Device* d3dDevice,
    const uint8_t* wicData,
    size_t wicDataSize,
    ID3D12Resource** texture,
    std::unique_ptr<uint8_t[]>& decodedData,
    D3D12_SUBRESOURCE_DATA& subresource,
    size_t maxsize) noexcept
	*/
    ThrowIfFailed(DirectX::LoadWICTextureFromMemoryEx(device.Get(), textureData, textureDataSize,
		0Ui64, D3D12_RESOURCE_FLAG_NONE, DirectX::WIC_LOADER_IGNORE_SRGB,
        texture->resource.GetAddressOf(), texture->decodedData, texture->subresource));

    const UINT64 uploadBufferSize = GetRequiredIntermediateSize(texture->resource.Get(), 0, 1);
    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
    auto desc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);

    ThrowIfFailed(device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(texture->uploadHeap.GetAddressOf())
    ));

    UpdateSubresources(commandList.Get(), texture->resource.Get(), texture->uploadHeap.Get(), 0, 0, 1, &texture->subresource);
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(texture->resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    commandList->ResourceBarrier(1, &barrier);
}

void loadTextureFromFile(ComPtr<ID3D12Device> &m_device, 
ComPtr<ID3D12GraphicsCommandList> &commandList, Texture *texture) {
    ThrowIfFailed(DirectX::LoadWICTextureFromFile(m_device.Get(), texture->filename.c_str(),
        texture->resource.GetAddressOf(), texture->decodedData, texture->subresource));

    const UINT64 uploadBufferSize = GetRequiredIntermediateSize(texture->resource.Get(), 0, 1);
    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
    auto desc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);

    ThrowIfFailed(m_device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(texture->uploadHeap.GetAddressOf())
    ));

    UpdateSubresources(commandList.Get(), texture->resource.Get(), texture->uploadHeap.Get(), 0, 0, 1, &texture->subresource);
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(texture->resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    commandList->ResourceBarrier(1, &barrier);
}

void BufferManager::loadImages(tinygltf::Model &model) {
	auto &images = model.images;
	for (int i = 0; i<images.size(); i++) {
		auto &image = images[i];
		auto &bufferView = model.bufferViews[image.bufferView];
		auto &buffer = buffers[bufferView.buffer];
		auto &gltfBuffer = model.buffers[bufferView.buffer];
		Texture* texture = new Texture();
		this->images.push_back(texture);
		uint8_t* imageData = &gltfBuffer.data[0] + bufferView.byteOffset;
		size_t imageSize = bufferView.byteLength;
		createTextureFromMemory(device, commandList, imageData, imageSize, this->images[i]);
	}
	defaultTexture = new Texture(L"./Textures/spree_bank_2k.tiff");
	loadTextureFromFile(device, commandList, defaultTexture);
	
	defaultNormalMap = new Texture(L"./Textures/default-normal.png");
	loadTextureFromFile(device, commandList, defaultNormalMap);
}



DXGI_FORMAT getDXGIFormatForImage(tinygltf::Image &image) {
	return DXGI_FORMAT_R8G8B8A8_SNORM;
}

void BufferManager::loadImagesHeap(std::vector<tinygltf::Image> &gltfImages) {
	if(images.size() == 0 && imageBuffers.size() == 0)
		return;

	D3D12_DESCRIPTOR_HEAP_DESC imagesHeapDesc = {};
	imagesHeapDesc.NumDescriptors = gltfImages.size();
	imagesHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	imagesHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	ThrowIfFailed(device->CreateDescriptorHeap(&imagesHeapDesc, IID_PPV_ARGS(&imagesHeap)));

	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(imagesHeap -> GetCPUDescriptorHandleForHeapStart());
	for (int i = 0; i<gltfImages.size(); i++) {
		auto &image = images[i]->resource;
		D3D12_SHADER_RESOURCE_VIEW_DESC imageDesc = {};
		imageDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		imageDesc.Format = image.Get()->GetDesc().Format;
		imageDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		imageDesc.Texture2D.MostDetailedMip = 0;
		imageDesc.Texture2D.MipLevels = image.Get() -> GetDesc().MipLevels;
		imageDesc.Texture2D.ResourceMinLODClamp = 0.0f;
			device->CreateShaderResourceView(image.Get(), &imageDesc, hDescriptor);
		hDescriptor.Offset(heapDescriptorSize);
	}
}