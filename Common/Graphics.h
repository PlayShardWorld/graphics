#pragma once
#include <unordered_map>

#include "Texture2D.h"

class Graphics
{
public:
	std::unordered_map<int, Texture2D>& GetAllTextures2D()
	{
		return texture2DMap;
	}

	void AddTexture2D(Texture2D& t2d)
	{
		// Имя задано и элемента нет на сцене
		//assert(!t2d.Name.empty() && gameObjectMap.find(t2d.Name) != gameObjectMap.end());

		int hash = std::hash<std::string>{}(t2d.Name);
		texture2DMap[hash] = t2d;
	}

	Texture2D& GetTexture2D(std::string name)
	{
		int hash = std::hash<std::string>{}(name);
		return texture2DMap[hash];
	}

private:
	std::unordered_map<int, Texture2D> texture2DMap;
};