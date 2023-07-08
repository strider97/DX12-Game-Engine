#include "stdafx.h"
#include <DirectXMath.h>

class DirectionLight {
	float intensity = 0.1f;
	DirectX::XMVECTOR color = { 1, 1, 1 };
	DirectX::XMVECTOR position = { 0, 28, 0 };
	DirectX::XMVECTOR UP = { 0, 1, 0 };
	DirectX::XMVECTOR direction = { 0.4, -0.95f, -0.4 };
public:
	DirectX::XMMATRIX lightOrthoMatrix = DirectX::XMMatrixOrthographicOffCenterRH(
		-30.f,
		30.f,
		-30.f,
		30.f,
		0.0f,
		50.f
	);
	DirectX::XMMATRIX lightViewMatrix;
	DirectX::XMMATRIX lightViewProjectionMatrix;

	DirectionLight() {
		direction = DirectX::XMVector3Normalize(direction);
		lightViewMatrix = DirectX::XMMatrixLookToRH(position, direction, UP);
		lightViewProjectionMatrix = DirectX::XMMatrixMultiply(lightViewMatrix, lightOrthoMatrix);
	}

	DirectX::XMVECTOR getDirection() {
		return DirectX::XMVectorScale(direction, -1.0f);
	}
};