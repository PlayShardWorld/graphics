#pragma once
#include "GameObject.h"
#include <string>

class Moon : public GameObject
{
public:
	Moon(std::string name, std::string geoname, std::string matname, float size, GameObject* parent, float distance_ae, float day_in_year)
	{
		Transform.SetWorldScale(size, size, size);
		Transform.SetWorldRotation(0.0f, 0.0f, 180.0f);
		Name = name;
		Geometry = geoname;
		Material = matname;
		Type = PrimitiveType::Sphere;
		RenderLayer = RenderLayer::Opaque;

		Transform.SetParent(&parent->Transform);

		_distance_ae = distance_ae;
		_day_in_year = day_in_year;
	}


	void Update(const GameTimer& gt) override
	{
		const float PI = 3.14159265358979323846f;
		const float IN_RAD = PI / 180.0f;

		if (_distance_ae != 0.0f && _day_in_year != 0.0f)
		{
			Transform.SetWorldPosition(
				_distance_ae * 8.0f * cos(IN_RAD * gt.TotalTime() * 6 * 365.25f / _day_in_year / 100),
				0.0f,
				_distance_ae * 8.0f * sin(IN_RAD * gt.TotalTime() * 6 * 365.25f / _day_in_year / 100)
			);
		}

		//OutputDebugStringA((std::to_string(Transform.Position.X) + "\n").c_str());
	}

private:
	float _distance_ae = 0.0f;
	float _day_in_year = 0.0f;
};