#pragma once
#include "stdafx.h"
#include <string.h>
#include "DXSample.h"
#include "tiny_gltf.h"
#include "WICTextureLoader12.h"

struct alignas(256) MaterialProperties {
	DirectX::XMFLOAT4 baseColor;
	float roughness;
	float metallic;
	float pad[2];
	DirectX::XMFLOAT3 emission;
};

struct Texture
{
    // Unique material name for lookup.
    std::wstring filename;
    Microsoft::WRL::ComPtr<ID3D12Resource> resource =
        nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> uploadHeap =
        nullptr;
    std::unique_ptr<uint8_t[]> decodedData;
    D3D12_SUBRESOURCE_DATA subresource;

    Texture(std::wstring filename) {
        this->filename = filename;
    }
    Texture() {}
	static void createTextureFromMemory(ComPtr<ID3D12Device> &device, 
		ComPtr<ID3D12GraphicsCommandList> &commandList, const uint8_t* textureData, 
		const size_t textureDataSize, Texture *texture);
	static void loadTextureFromFile(ComPtr<ID3D12Device> &m_device, 
		ComPtr<ID3D12GraphicsCommandList> &commandList, Texture *texture);
};

class MeshPrimitive {
public:
	MeshPrimitive(
		std::vector<ComPtr<ID3D12Resource>>& buffers,
		tinygltf::Primitive& primitive,
		tinygltf::Model &model,
		CD3DX12_CPU_DESCRIPTOR_HANDLE materialBufferViewHandleCpu,
		CD3DX12_GPU_DESCRIPTOR_HANDLE materialHeapGpuhandle);
	
	D3D12_INDEX_BUFFER_VIEW indexBufferView;
	D3D12_VERTEX_BUFFER_VIEW vbViewPosition;
	D3D12_VERTEX_BUFFER_VIEW vbViewNormal;
	D3D12_VERTEX_BUFFER_VIEW vbViewUV;
	D3D12_VERTEX_BUFFER_VIEW vbViewTangent;
	CD3DX12_CPU_DESCRIPTOR_HANDLE materialHeapCpuhandle;
	CD3DX12_GPU_DESCRIPTOR_HANDLE materialHeapGpuhandle;

	int indexCount = 0;
	bool hasBaseColorTexture = false;
	tinygltf::Material material;
	tinygltf::Primitive primitive;
	ComPtr<ID3D12DescriptorHeap> texturesHeap;
	void loadTextureHeaps(std::vector<Texture*> &imageBuffers, 
		ComPtr<ID3D12Device> &device,
		int heapDescriptorSize,
    	std::vector<tinygltf::Texture> &textures,
		Texture* defaultTexture,
    	Texture* defaultNormalMap);

private:
};

class BufferManager {
public:
	BufferManager(
		ComPtr<ID3D12Device> &m_device, 
		ComPtr<ID3D12CommandQueue> &commandQueue, 
		ComPtr<ID3D12GraphicsCommandList>& commandList);
	void loadBuffers(std::vector<tinygltf::Buffer> &buffers);
	void loadBufferViews(tinygltf::Model & model);
	void loadMaterials(std::vector<tinygltf::Material> &materials);
	void loadImages(std::vector<tinygltf::Image> &images);
	std::vector<MeshPrimitive> meshPrimitives;
	std::vector<MeshPrimitive> meshPrimitivesTransparent;
	int heapDescriptorSize;
	D3D12_GPU_VIRTUAL_ADDRESS getGpuVirtualAddressForMaterial(int index);
	void loadImagesHeap(std::vector<tinygltf::Image> &images);
	void loadImages(tinygltf::Model &model);
	void loadImages2(tinygltf::Model &model);
	ComPtr<ID3D12DescriptorHeap> imagesHeap;
	std::vector<ComPtr<ID3D12Resource>> imageBuffers;
	Texture* defaultTexture;
	Texture* defaultNormalMap;

private:
	std::vector<ComPtr<ID3D12Resource>> buffers;
	std::vector<ComPtr<ID3D12Resource>> uploadBuffers;
	ComPtr<ID3D12Resource> materialBuffer;
	ComPtr<ID3D12Resource> materialUploadBuffer;
	ComPtr<ID3D12DescriptorHeap> materialHeap;
	std::vector<Texture*> images;
	std::vector<ComPtr<ID3D12Resource>> imageUploadBuffers;
	ComPtr<ID3D12Device> device;
	ComPtr<ID3D12GraphicsCommandList> commandList;
	ComPtr<ID3D12CommandQueue> commandQueue;
};