#pragma once
#include "GameObject.h"
#include "GameInput.h"
#include "Camera.h"
#include <string>

class Katamari : public GameObject
{
public:
	Katamari(std::string name, std::string geoname, std::string matname, float size, Camera* camera)
	{
		Transform.SetWorldScale(size, size, size);
		Transform.SetWorldRotation(0.0f, 0.0f, 180.0f);
		Name = name;
		Geometry = geoname;
		Material = matname;
		Type = PrimitiveType::Sphere;
		RenderLayer = RenderLayer::Opaque;

		_colliderSize = size;
		_camera = camera;
	}


	void Update(const GameTimer& gt) override
	{
		float forward = 0, right = 0;

		if (GameInput::IsKeyPressed(KEYBOARD_W)) { forward += 1; }
		if (GameInput::IsKeyPressed(KEYBOARD_A)) { right += 1; }
		if (GameInput::IsKeyPressed(KEYBOARD_S)) { forward -= 1; }
		if (GameInput::IsKeyPressed(KEYBOARD_D)) { right -= 1; }

		XMVECTOR rAxis = { right, 0, -forward };
		XMFLOAT3 temp;
		XMStoreFloat3(&temp, rAxis);
		if (temp.x == 0 && temp.y == 0 && temp.z == 0)
			return;

		float rx, ry, rz;
		ExtractPitchYawRollFromXMMatrix(&rx, &ry, &rz, &(Transform.GetWorldRotation() * XMMatrixRotationAxis(rAxis, gt.DeltaTime() * _rotationSpeed)));

		Transform.SetWorldPosition(Transform.Position.X + forward * _movingSpeed * gt.DeltaTime(), 0, Transform.Position.Z + right * _movingSpeed * gt.DeltaTime());
		Transform.SetWorldRotation(rx, ry, rz);

		_camera->mPosition.x = Transform.Position.X - 20;
		_camera->mPosition.y = 15;
		_camera->mPosition.z = Transform.Position.Z;
	}

	void AddColliderSize()
	{
		_colliderSize += _colliderStep;
	}

	float ColliderSize()
	{
		return _colliderSize;
	}

private:
	float _movingSpeed = 15.0f;
	float _rotationSpeed = 10.0f;

	float _colliderSize = 0.0f;
	float _colliderStep = 0.0f;

	Camera* _camera = nullptr;
};