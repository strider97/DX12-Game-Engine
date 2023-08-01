#pragma once
#include "DXSample.h"
#include "BufferManager.h"

class Skybox
{
public:
    Skybox(ComPtr<ID3D12Device> &device, ComPtr<ID3D12GraphicsCommandList> commandList);
    ComPtr<ID3D12PipelineState> skyboxPSO;
    ComPtr<ID3DBlob> vertexShader;
    ComPtr<ID3DBlob> pixelShader;
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12GraphicsCommandList> commandList;
    ComPtr<ID3D12RootSignature> rootSignature;
	ComPtr<ID3D12Resource> uploadBuffer;
	ComPtr<ID3D12Resource> vertexBuffer;
    std::vector<float> vertices;
    ComPtr<ID3D12DescriptorHeap> descriptorHeap;
    Texture* texture;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView;

    virtual void loadPipeline();
    virtual void createRootSignature();
    virtual void loadResources();
    void compileShader();
    void draw(D3D12_GPU_VIRTUAL_ADDRESS cBufferAddress);
};

