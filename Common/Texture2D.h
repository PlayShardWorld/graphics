#pragma once
#include <string>
#include "DirectXMath.h"

class Texture2D
{
public:
	// General
	std::string Name;
	std::string Path;

	// Settings
	DirectX::XMFLOAT4 DiffuseAlbedo;
	DirectX::XMFLOAT3 FresnelR0;
	float Roughness;

	void SetDiffuseAlbedo(float x, float y, float z, float w)
	{
		DiffuseAlbedo = DirectX::XMFLOAT4(x, y, z, w);
	}

	void SetFresnelR0(float x, float y, float z)
	{
		FresnelR0 = DirectX::XMFLOAT3(x, y, z);
	}

	void SetRoughness(float x)
	{
		Roughness = x;
	}

	std::wstring GetPath()
	{
		return std::wstring(Path.begin(), Path.end());
	}
};