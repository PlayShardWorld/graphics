#pragma once

#include "d3dx12.h"
#include "../Common/MathHelper.h"

// Перечисление слоёв отрисовки
enum class RenderLayer : int
{
	Opaque = 0,
	ShadowDebug,
	SsaoDebug,
	AlphaTestedTreeSprites,
	Debug,
	Sky, 
	Count
};

// Структура, хранящая параметры объекта отрисовки
struct RenderItem
{
	RenderItem() = default;
	RenderItem(const RenderItem& rhs) = delete;

	// Матрица, определяющее положение, ориентацию и масштаб объекта в мировых координатах
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();

	DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	// Dirty flag indicating the object data has changed and we need to update the constant buffer.
	// Because we have an object cbuffer for each FrameResource, we have to apply the
	// update to each FrameResource.  Thus, when we modify obect data we should set 
	// NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
	int NumFramesDirty = gNumFrameResources;

	// Индекс в буфере констант GPU соответствующий ObjectCB для этого объекта.
	UINT ObjCBIndex = -1;

	Material* Mat = nullptr;
	MeshGeometry* Geo = nullptr;

	// Тип примитивной топологии
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// DrawIndexedInstanced параметры
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;
};