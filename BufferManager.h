#pragma once
#include "stdafx.h"
#include "DXSample.h"
#include "tiny_gltf.h"

class MeshPrimitive {
public:
	MeshPrimitive(
		std::vector<ComPtr<ID3D12Resource>> &vertexBuffers,
		tinygltf::Primitive &primitive, 
		std::vector <tinygltf::BufferView> &bufferViews,
		std::vector <tinygltf::Accessor> &accessors,
		tinygltf::Material &material);
private:
	D3D12_VERTEX_BUFFER_VIEW vbViewPosition;
	D3D12_VERTEX_BUFFER_VIEW vbViewNormal;
	D3D12_VERTEX_BUFFER_VIEW vbViewUV;
	D3D12_INDEX_BUFFER_VIEW indexBufferView;
	tinygltf::Material material;
};

class BufferManager {
public:
	BufferManager(ComPtr<ID3D12Device> &m_device, ComPtr<ID3D12GraphicsCommandList>& commandList);
	void loadBuffers(std::vector<tinygltf::Buffer> &buffers);
	void loadBufferViews(tinygltf::Model & model);

private:
	std::vector<ComPtr<ID3D12Resource>> vertexBuffers;
	std::vector<ComPtr<ID3D12Resource>> uploadBuffers;
	ComPtr<ID3D12Device> device;
	ComPtr<ID3D12GraphicsCommandList> commandList;
	std::vector<MeshPrimitive> meshPrimitives;
};

