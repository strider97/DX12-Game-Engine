#pragma once
#include "stdafx.h"
#include <string.h>
#include "DXSample.h"
#include "tiny_gltf.h"

struct alignas(256) Material {
	DirectX::XMFLOAT3 baseColor;
	float roughness;
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
};

class MeshPrimitive {
public:
	MeshPrimitive(
		std::vector<ComPtr<ID3D12Resource>>& buffers,
		tinygltf::Primitive& primitive,
		tinygltf::Model &model,
		CD3DX12_CPU_DESCRIPTOR_HANDLE materialBufferViewHandleCpu,
		CD3DX12_GPU_DESCRIPTOR_HANDLE materialHeapGpuhandle,
		CD3DX12_CPU_DESCRIPTOR_HANDLE imageHandleCpu,
		CD3DX12_GPU_DESCRIPTOR_HANDLE imageHandleGpu);
	
	D3D12_INDEX_BUFFER_VIEW indexBufferView;
	D3D12_VERTEX_BUFFER_VIEW vbViewPosition;
	D3D12_VERTEX_BUFFER_VIEW vbViewNormal;
	D3D12_VERTEX_BUFFER_VIEW vbViewUV;
	CD3DX12_CPU_DESCRIPTOR_HANDLE materialHeapCpuhandle;
	CD3DX12_GPU_DESCRIPTOR_HANDLE materialHeapGpuhandle;
	CD3DX12_CPU_DESCRIPTOR_HANDLE baseColorTextureCpuhandle;
	CD3DX12_GPU_DESCRIPTOR_HANDLE baseColorTextureGpuhandle;

	int indexCount = 0;
	bool hasBaseColorTexture;
	tinygltf::Material material;
	tinygltf::Primitive primitive;
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
	int heapDescriptorSize;
	D3D12_GPU_VIRTUAL_ADDRESS getGpuVirtualAddressForMaterial(int index);
	void loadImagesHeap(std::vector<tinygltf::Image> &images);
	void loadImages(tinygltf::Model &model);

private:
	std::vector<ComPtr<ID3D12Resource>> buffers;
	std::vector<ComPtr<ID3D12Resource>> uploadBuffers;
	ComPtr<ID3D12Resource> materialBuffer;
	ComPtr<ID3D12Resource> materialUploadBuffer;
	ComPtr<ID3D12DescriptorHeap> materialHeap;
	std::vector<ComPtr<ID3D12Resource>> imageBuffers;
	std::vector<Texture*> images;
	std::vector<ComPtr<ID3D12Resource>> imageUploadBuffers;
	ComPtr<ID3D12DescriptorHeap> imagesHeap;
	ComPtr<ID3D12Device> device;
	ComPtr<ID3D12GraphicsCommandList> commandList;
	ComPtr<ID3D12CommandQueue> commandQueue;
};

