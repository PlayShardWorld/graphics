#pragma once
#include <string>
#include "GameTimer.h"

#include "Transform.h"
#include "RenderItem.h"

enum PrimitiveType
{
	Custom,
	Skysphere,
	Sphere,
	Sprite,
	Plane
};

class GameObject
{
public:
	// General
	std::string Name;

	// Transform
	Transform Transform;

	// Rendering
	std::unique_ptr<RenderItem> ri;
	std::string Geometry;
	std::string Material;
	RenderLayer RenderLayer = RenderLayer::Opaque;

	// Primitive Type
	PrimitiveType Type;


	// Игровой цикл
	virtual void Awake() {};
	virtual void Start() {};
	virtual void FixedUpdate() {};
	virtual void Update(const GameTimer& gt) = 0;
	virtual void LateUpdate() {};
	void RenderUpdate()
	{
		if (Transform.IsDirty())
		{
			DirectX::XMVECTOR S = DirectX::XMVectorSet(Transform.Scale.X, Transform.Scale.Y, Transform.Scale.Z, 0.0f);
			DirectX::XMVECTOR T = DirectX::XMVectorSet(Transform.Position.X, Transform.Position.Y, Transform.Position.Z, 0.0f);
			DirectX::XMVECTOR Q = DirectX::XMQuaternionRotationRollPitchYaw(
				DirectX::XMConvertToRadians(Transform.Rotation.X), 
				DirectX::XMConvertToRadians(Transform.Rotation.Y), 
				DirectX::XMConvertToRadians(Transform.Rotation.Z));
			DirectX::XMVECTOR zero = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);

			DirectX::XMStoreFloat4x4(&ri->World, DirectX::XMMatrixAffineTransformation(S, zero, Q, T));
		}
	}
	virtual void OnDestroy() {};
};