#include "stdafx.h"
using namespace DirectX;

class Camera {
	XMMATRIX m_viewMatrix;
	XMMATRIX R;
	XMVECTOR front;
	XMVECTOR right;
	XMVECTOR up;
	const XMVECTOR UP = { 0, 1, 0 };
	const XMVECTOR INITIAL_FRONT = { 0, 0, -1 };

	float yaw = 0;
	float pitch = 0;
	const float roll = 0;
	const float MOUSE_SENS = 0.5f;

private:
	void updateLookAt() {
		up = XMVector3Cross(right, front);
		m_viewMatrix = XMMatrixLookToRH(eye, front, UP);
	}

public:
	XMVECTOR eye = { 0, 2, 7 };
	Camera() {
		front = INITIAL_FRONT;
		right = XMVector3Normalize(XMVector3Cross(front, UP));
		updateLookAt();
	}

	void translate(float f, float r) {
		eye += XMVectorScale(front, f);
		eye += XMVectorScale(right, r);
		updateLookAt();
	}

	void rotateCam(int &dx, int &dy, float dt) {
		if (dx == 0 && dy == 0) {
			return;
		}
		pitch -= dy * dt * MOUSE_SENS;
		yaw -= dx * dt * MOUSE_SENS;
		pitch = max(-XM_PIDIV2 + 0.1, min(XM_PIDIV2 - 0.1, pitch));
		R = XMMatrixRotationRollPitchYaw(pitch, yaw, roll);
		front = XMVector4Transform({ XMVectorGetX(INITIAL_FRONT), XMVectorGetY(INITIAL_FRONT), 
			XMVectorGetZ(INITIAL_FRONT), 0}, R);
		right = XMVector3Normalize(XMVector3Cross(front, UP));
		updateLookAt();

		if (dx != 0) {
			dx = 0;
		}
		if (dy != 0) { 
			dy = 0;
		};
	}

	XMMATRIX* viewMatrix() {
		return &m_viewMatrix;
	}

};