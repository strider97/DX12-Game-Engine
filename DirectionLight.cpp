#include "stdafx.h"

class DirectionLight {
	float intensity = 0.1f;
	DirectX::XMVECTOR direction = { 0, 1.05f, 0 };
	DirectX::XMVECTOR color = { 1, 1, 1 };
	DirectX::XMVECTOR position = { 0, 15, 0 };
	DirectX::XMVECTOR UP = { 0, 1, 0 };
	DirectX::XMMATRIX lightOrthoMatrix = DirectX::XMMatrixOrthographicOffCenterLH(
		-20.f,
		20.f,
		20.f,
		-20.f,
		-1000.f,
		1000.f
	);
	DirectX::XMMATRIX lightViewMatrix;
public:
	DirectX::XMMATRIX lightViewProjectionMatrix;

	DirectionLight(DirectX::XMVECTOR direction) {
		this->direction = direction;
		lightViewMatrix = DirectX::XMMatrixLookToLH(position, this->direction, UP);
		lightViewProjectionMatrix = DirectX::XMMatrixMultiply(lightOrthoMatrix, lightViewMatrix);
	}
};