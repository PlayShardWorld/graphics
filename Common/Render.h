#pragma once
#include <unordered_map>
#include <string>
#include <vector>

#include "d3dx12.h"
#include "d3dUtil.h"
#include "GeometryGenerator.h"

using Microsoft::WRL::ComPtr;

enum ShaderType
{
	VertexShader,
	PixelShader
};

class Render
{
public:
	Render()
	{
		InitializeMaterialMap();

		mInputLayout =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};
	}

#pragma region Geometry
	void SetGeometry(ID3D12Device* device, ID3D12GraphicsCommandList* gcl, std::string name, GeometryGenerator::MeshData md)
	{
		std::vector<Vertex> vertices(md.Vertices.size());
		for (size_t i = 0; i < md.Vertices.size(); ++i)
		{
			auto& p = md.Vertices[i].Position;
			vertices[i].Pos = p;
			vertices[i].Normal = md.Vertices[i].Normal;
			vertices[i].TexC = md.Vertices[i].TexC;
			vertices[i].TangentU = md.Vertices[i].TangentU;
		}

		const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);

		std::vector<std::uint16_t> indices = md.GetIndices16();
		const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

		auto geo = std::make_unique<MeshGeometry>();
		geo->Name = name;

		ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
		CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

		ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
		CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

		geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(device, gcl, 
			vertices.data(), vbByteSize, geo->VertexBufferUploader);

		geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(device, gcl, 
			indices.data(), ibByteSize, geo->IndexBufferUploader);

		geo->VertexByteStride = sizeof(Vertex);
		geo->VertexBufferByteSize = vbByteSize;
		geo->IndexFormat = DXGI_FORMAT_R16_UINT;
		geo->IndexBufferByteSize = ibByteSize;

		SubmeshGeometry submesh;
		submesh.IndexCount = (UINT)indices.size();
		submesh.StartIndexLocation = 0;
		submesh.BaseVertexLocation = 0;

		geo->DrawArgs[name] = submesh;

		geometryMap[name] = std::move(geo);
	}

	MeshGeometry* GetGeometry(std::string name)
	{
		if (geometryMap.find(name) != geometryMap.end())
			return geometryMap[name].get();
		else
			return geometryMap["default"].get();
	}
#pragma endregion

#pragma region Texture
	void SetTexture(ID3D12Device* device, ID3D12GraphicsCommandList* gcl, std::string name, std::wstring path)
	{
		auto texture = std::make_unique<Texture>();
		texture->Name = name;
		texture->Filename = path;

		// Загружаем DDS текстуру
		ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(
			device,
			gcl,
			texture->Filename.c_str(),
			texture->Resource,
			texture->UploadHeap)
		);

		// Перемещаем текстуру в список
		textureMap.push_back(std::move(texture));
	}

	Texture* GetTexture(std::string name)
	{
		for (int i = 0; i < textureMap.size(); ++i)
			if (textureMap[i]->Name == name)
				return textureMap[i].get();

		return nullptr;
	}

	int GetTextureIndex(std::string name)
	{
		for (int i = 0; i < textureMap.size(); ++i)
			if (textureMap[i]->Name == name)
				return i;

		return -1;
	}

	std::vector<std::unique_ptr<Texture>>& GetTextureMap()
	{
		return textureMap;
	}

	int TexturesCount()
	{
		return textureMap.size();
	}
#pragma endregion

#pragma region Material
	void SetMaterial(std::string name, std::string texname, std::string norname,
		float difal_x, float difal_y, float difal_z, float difal_w,
		float fre_x, float fre_y, float fre_z,
		float roughness)
	{
		if (materialMap.size() > 0 && materialMap.find(name) != materialMap.end())
		{
			auto material = std::make_unique<Material>();
			material->Name = name;
			material->DiffuseAlbedo = DirectX::XMFLOAT4(difal_x, difal_y, difal_z, difal_w);
			material->FresnelR0 = DirectX::XMFLOAT3(fre_x, fre_y, fre_z);
			material->Roughness = roughness;
			material->MatCBIndex = materialMap[name].get()->MatCBIndex;
			material->DiffuseSrvHeapIndex = GetTextureIndex(texname);
			material->NormalSrvHeapIndex = GetTextureIndex(norname);
			materialMap[name] = std::move(material);
		}
		else
		{
			auto material = std::make_unique<Material>();
			material->Name = name;
			material->DiffuseAlbedo = DirectX::XMFLOAT4(difal_x, difal_y, difal_z, difal_w);
			material->FresnelR0 = DirectX::XMFLOAT3(fre_x, fre_y, fre_z);
			material->Roughness = roughness;
			material->MatCBIndex = materialMap.size();
			material->DiffuseSrvHeapIndex = GetTextureIndex(texname);
			material->NormalSrvHeapIndex = GetTextureIndex(norname);
			materialMap[name] = std::move(material);
		}
	}

	Material* GetMaterial(std::string name)
	{
		if (materialMap.find(name) != materialMap.end())
			return materialMap[name].get();
		else
			return materialMap["default"].get();
	}

	std::unordered_map<std::string, std::unique_ptr<Material>>& GetMaterialMap()
	{
		return materialMap;
	}

	int MaterialsCount()
	{
		return materialMap.size();
	}
#pragma endregion

#pragma region Shader
	void CreateShader(std::string name, std::wstring path, const std::string entryPoint, const std::string target, const D3D_SHADER_MACRO* defines = nullptr)
	{
		shaderMap[name] = d3dUtil::CompileShader(path, nullptr, entryPoint, target);
		//mShaders["standardVS"] = d3dUtil::CompileShader(L"Default.hlsl", nullptr, "VS", "vs_5_1");
	}

	D3D12_SHADER_BYTECODE GetShaderBytecode(std::string name)
	{
		return {
			reinterpret_cast<BYTE*>(shaderMap[name]->GetBufferPointer()),
			shaderMap[name]->GetBufferSize()
		};
	}

	std::vector<D3D12_INPUT_ELEMENT_DESC>& GetShaderInputLayout()
	{
		return mInputLayout;
	}
#pragma endregion

private:
	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> geometryMap;
	std::vector<std::unique_ptr<Texture>> textureMap;
	std::unordered_map<std::string, std::unique_ptr<Material>> materialMap;

	std::unordered_map<std::string, ComPtr<ID3DBlob>> shaderMap;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> psoMap;
	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	void InitializeMaterialMap()
	{
		//SetMaterial("default", "", 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	}

	void InitializeGeometryMap()
	{
		//GeometryGenerator geoGen;
		//SetGeometry("default", geoGen.CreateSphere(1.0f, 4, 4));
	}
};