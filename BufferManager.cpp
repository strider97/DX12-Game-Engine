#include "stdafx.h"
#include "DXSample.h"

class BufferManager {
public:
	BufferManager () {}
	void loadBuffers() {}

private:
	
    ComPtr<ID3D12Resource>* m_vertexBuffers;

};