#pragma once
#include "GameObject.h"
#include <string>

class Particle : public GameObject
{
public:
	Particle(std::string name, std::string geoname, std::string matname, float size)
	{
		Transform.SetWorldScale(size, size, size);
		Transform.SetWorldRotation(0.0f, 0.0f, 180.0f);
		Name = name;
		Geometry = geoname;
		Material = matname;
		Type = PrimitiveType::Sprite;
		RenderLayer = RenderLayer::AlphaTestedTreeSprites;
	}


	void Update(const GameTimer& gt) override
	{
	}

private:
};