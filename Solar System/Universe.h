#pragma once
#include "GameObject.h"
#include <string>

class Universe : public GameObject
{
public:
	Universe(std::string name, std::string geoname, std::string matname)
	{
		Transform.SetWorldScale(1000000.0f, 1000000.0f, 1000000.0f);
		Name = name;
		Geometry = geoname;
		Material = matname;
		Type = PrimitiveType::Skysphere;
		RenderLayer = RenderLayer::Opaque;
	}

	void Update(const GameTimer& gt) override
	{

	}
};