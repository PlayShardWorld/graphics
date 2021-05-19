#pragma once
#include "GameObject.h"
#include <string>

class Platform : public GameObject
{
public:
	Platform(std::string name, std::string geoname, std::string matname, float size)
	{
		Transform.SetWorldPosition(0.0f, -1.0f, 0.0f);
		Transform.SetWorldScale(size, size, size);
		Transform.SetWorldRotation(0.0f, 0.0f, 0.0f);
		Name = name;
		Geometry = geoname;
		Material = matname;
		Type = PrimitiveType::Plane;
	}


	void Update(const GameTimer& gt) override
	{
	}
};