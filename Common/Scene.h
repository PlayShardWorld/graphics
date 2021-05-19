#pragma once
#include <unordered_map>
#include <string>
#include <cassert>

#include "Camera.h"
#include "GameObject.h"

class Scene
{
public:
#pragma region Main Camera
	void SetMainCamera(Camera* camera)
	{
		_mainCamera = camera;
	}

	Camera* GetMainCamera()
	{
		return _mainCamera;
	}
#pragma endregion

	void Update(const GameTimer& gt)
	{
		// ���������� ������
		if (_mainCamera != nullptr)
		_mainCamera->Update(gt);

		// ���������� ���� �������� �� �����
		for (auto it : GetAllGameObjects())
			if (it.second != nullptr)
				it.second->Update(gt);
	}

	void RenderUpdate()
	{
		// ���������� ���� �������� �� �����
		for (auto it : GetAllGameObjects())
			if (it.second != nullptr)
				it.second->RenderUpdate();
	}

	std::unordered_map<int, GameObject*>& GetAllGameObjects()
	{
		return gameObjectMap;
	}

	void AddGameObject(GameObject* go)
	{
		// ��� ������ � �������� ��� �� �����
		assert(true);
		//assert(!go.Name.empty() && gameObjectMap.find(go.Name) != gameObjectMap.end());

		int hash = std::hash<std::string>{}(go->Name);
		gameObjectMap[hash] = go;
	}

	GameObject* GetGameObject(std::string name)
	{
		int hash = std::hash<std::string>{}(name);
		return gameObjectMap[hash];
	}



private:
	Camera* _mainCamera;
	std::unordered_map<int, GameObject*> gameObjectMap;
};