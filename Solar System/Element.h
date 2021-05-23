#pragma once
#include "GameObject.h"
#include "Katamari.h"
#include <string>

class Element : public GameObject
{
public:
	Element(std::string name, std::string geoname, std::string matname, float size, Katamari* obj, float x, float y, float z)
	{
		Transform.SetWorldPosition(x, y, z);
		Transform.SetWorldScale(size, size, size);
		Transform.SetWorldRotation(0.0f, 0.0f, 180.0f);

		Name = name;
		Geometry = geoname;
		Material = matname;
		Type = PrimitiveType::Sphere;
		RenderLayer = RenderLayer::Opaque;
		katamari = obj;
	}

	void Update(const GameTimer& gt) override
	{
		if (!Transform.GetParent())
		{
			float dist = sqrt(
				pow(katamari->Transform.Position.X - Transform.Position.X, 2) +
				pow(katamari->Transform.Position.Y - Transform.Position.Y, 2) +
				pow(katamari->Transform.Position.Z - Transform.Position.Z, 2));

			if (dist <= Transform.Scale.X + katamari->ColliderSize())
			{
				Transform.SetParent(&katamari->Transform);
				katamari->AddColliderSize();
			}
		}
	}

private:
	Katamari* katamari;
};