#pragma once
#include "stdafx.h"
#include "DXSample.h"
#include "tiny_gltf.h"

class BufferManager {
public:
	BufferManager(ComPtr<ID3D12Device> &m_device, ComPtr<ID3D12GraphicsCommandList>& commandList);
	void loadBuffers(std::vector<tinygltf::Buffer> &buffers);
	void loadBufferViews(std::vector<tinygltf::BufferView>& bufferViews, 
		std::vector<tinygltf::Accessor>& accessors, 
		ComPtr<ID3D12DescriptorHeap>& bufferViewHeap);

private:
	std::vector<ComPtr<ID3D12Resource>> vertexBuffers;
	std::vector<ComPtr<ID3D12Resource>> uploadBuffers;
	ComPtr<ID3D12Device> device;
	ComPtr<ID3D12GraphicsCommandList> commandList;
};

