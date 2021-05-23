#pragma once

#include "d3dx12.h"
#include "../Common/MathHelper.h"
#include <vector>
//#include "GameObject.h"

using namespace DirectX;

struct Vector3
{
	float X;
	float Y;
	float Z;
};

class Transform
{
private:
	bool _isChanged = true;			// Dirty flag
	Transform* _parent = nullptr;

	XMFLOAT4X4 world;
	XMMATRIX local;

public:
	struct Vector3 Position;
	struct Vector3 Scale;
	struct Vector3 Rotation;
	std::vector<Transform*> childs;

	Transform()
	{
		SetWorldPosition(0.0f, 0.0f, 0.0f);
		SetWorldScale(0.0f, 0.0f, 0.0f);
		SetWorldRotation(0.0f, 0.0f, 0.0f);
		_isChanged = true;
		CalculateMatrix();
	}

	void CalculateMatrix()
	{
		XMMATRIX parentMatrix = _parent ? _parent->GetLocalMatrix() : XMLoadFloat4x4(&MathHelper::Identity4x4());

		DirectX::XMVECTOR zero = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
		DirectX::XMVECTOR S = DirectX::XMVectorSet(Scale.X, Scale.Y, Scale.Z, 0.0f);
		DirectX::XMVECTOR T = DirectX::XMVectorSet(Position.X, Position.Y, Position.Z, 0.0f);
		DirectX::XMVECTOR Q = DirectX::XMQuaternionRotationRollPitchYaw(
			DirectX::XMConvertToRadians(Rotation.X),
			DirectX::XMConvertToRadians(Rotation.Y),
			DirectX::XMConvertToRadians(Rotation.Z));
		local = DirectX::XMMatrixAffineTransformation(S, zero, Q, T);

		local = XMMatrixMultiply(local, parentMatrix);
	}

	// Связь с рендером
	XMMATRIX GetTransformMatrix()
	{
		return XMLoadFloat4x4(&world);
	}
	
	XMMATRIX GetLocalMatrix()
	{
		return local;
	}

	XMMATRIX GetWorldRotation()
	{
		XMVECTOR newScale, newRot_quat, newTrans;
		XMMatrixDecompose(&newScale, &newRot_quat, &newTrans, XMLoadFloat4x4(&world));
		XMMATRIX rotMatrix = XMMatrixRotationQuaternion(newRot_quat);
		return rotMatrix;
	}

	void RefreshWorldMatrix()
	{
		world = GetGlobalWorldMatrix();

		for (auto& child : childs)
			child->RefreshWorldMatrix();
	}

	XMFLOAT4X4 GetGlobalWorldMatrix()
	{
		//recursively calculates world matrix from parents chain
		XMMATRIX parentMatrix = _parent ? XMLoadFloat4x4(&_parent->GetGlobalWorldMatrix()) : XMLoadFloat4x4(&MathHelper::Identity4x4());

		auto origin = XMVectorSet(0, 0, 0, 1);
		auto scale = XMVectorSet(Scale.X, Scale.Y, Scale.Z, 0);
		auto translation = XMVectorSet(Position.X, Position.Y, Position.Z, 0);
		auto rotation = DirectX::XMQuaternionRotationRollPitchYaw(
			Rotation.X,
			Rotation.Y,
			Rotation.Z);

		auto localMatrix = XMMatrixAffineTransformation(scale, origin, rotation, translation);

		XMFLOAT4X4 result;
		XMStoreFloat4x4(&result, localMatrix * parentMatrix);
		return result;
	}

	void RecalcTransformRelativeToParent()
	{
		XMFLOAT4X4 result;
		XMStoreFloat4x4(&result, XMLoadFloat4x4(&world) * XMMatrixInverse(nullptr, XMLoadFloat4x4(&_parent->world)));
		XMVECTOR newScale, newRot_quat, newTrans;
		XMMatrixDecompose(&newScale, &newRot_quat, &newTrans, XMLoadFloat4x4(&result));

		XMFLOAT3 pos;
		XMStoreFloat3(&pos, newTrans);
		SetWorldPosition(pos.x, pos.y, pos.z);

		float rx, ry, rz;
		ExtractPitchYawRollFromXMMatrix(&rx, &ry, &rz, &XMLoadFloat4x4(&result));
		SetWorldRotation(rx, ry, rz);

		XMFLOAT3 scale;
		XMStoreFloat3(&scale, newScale);
		SetWorldScale(scale.x, scale.y, scale.z); //TODO: bug if parent has non uniform scale

		RefreshWorldMatrix();
	}

	// Установка позиции в мировых координатах
	void SetWorldPosition(float x, float y, float z)
	{
		Position.X = x;
		Position.Y = y;
		Position.Z = z;
		_isChanged = true;
		CalculateMatrix();
		RefreshWorldMatrix();
	}

	// Установка размера в мировых координатах
	void SetWorldScale(float x, float y, float z)
	{
		Scale.X = x;
		Scale.Y = y;
		Scale.Z = z;
		_isChanged = true;
		CalculateMatrix();
		RefreshWorldMatrix();
	}

	// Установка вращения в мировых координатах
	void SetWorldRotation(float x, float y, float z)
	{
		Rotation.X = x;
		Rotation.Y = y;
		Rotation.Z = z;
		_isChanged = true;
		CalculateMatrix();
		RefreshWorldMatrix();
	}

	bool IsDirty()
	{
		return _isChanged;
	}

	void ClearDirtyFlag()
	{
		_isChanged = false;
	}

	Transform* GetParent()
	{
		return _parent;
	}

	void SetParent(Transform* transform)
	{
		_parent = transform;
		transform->childs.push_back(this);
		RecalcTransformRelativeToParent();
	}

	void SetChild(Transform* transform)
	{
		transform->SetParent(this);
	}

	void ExtractPitchYawRollFromXMMatrix(float* flt_p_PitchOut, float* flt_p_YawOut, float* flt_p_RollOut, const DirectX::XMMATRIX* XMMatrix_p_Rotation)
	{
		DirectX::XMFLOAT4X4 XMFLOAT4X4_Values;
		DirectX::XMStoreFloat4x4(&XMFLOAT4X4_Values, DirectX::XMMatrixTranspose(*XMMatrix_p_Rotation));
		*flt_p_PitchOut = (float)asin(-XMFLOAT4X4_Values._23);
		*flt_p_YawOut = (float)atan2(XMFLOAT4X4_Values._13, XMFLOAT4X4_Values._33);
		*flt_p_RollOut = (float)atan2(XMFLOAT4X4_Values._21, XMFLOAT4X4_Values._22);
	}
};