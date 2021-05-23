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
	void RenderUpdate() { DirectX::XMStoreFloat4x4(&ri->World, Transform.GetTransformMatrix()); }
	virtual void OnDestroy() {};

	void ExtractPitchYawRollFromXMMatrix(float* flt_p_PitchOut, float* flt_p_YawOut, float* flt_p_RollOut, const DirectX::XMMATRIX* XMMatrix_p_Rotation)
	{
		DirectX::XMFLOAT4X4 XMFLOAT4X4_Values;
		DirectX::XMStoreFloat4x4(&XMFLOAT4X4_Values, DirectX::XMMatrixTranspose(*XMMatrix_p_Rotation));
		*flt_p_PitchOut = (float)asin(-XMFLOAT4X4_Values._23);
		*flt_p_YawOut = (float)atan2(XMFLOAT4X4_Values._13, XMFLOAT4X4_Values._33);
		*flt_p_RollOut = (float)atan2(XMFLOAT4X4_Values._21, XMFLOAT4X4_Values._22);
	}
};