#pragma once

class Transform
{
public:
	Transform()
	{
		SetWorldPosition(0.0f, 0.0f, 0.0f);
		SetWorldScale(0.0f, 0.0f, 0.0f);
		SetWorldRotation(0.0f, 0.0f, 0.0f);
		IsChanged = true;
	}

	// Variables
	struct Position { float X, Y, Z; } Position;
	struct Scale { float X, Y, Z; } Scale;
	struct Rotation { float X, Y, Z; } Rotation;

	// Установка позиции в мировых координатах
	void SetWorldPosition(float x, float y, float z)
	{
		Position.X = x;
		Position.Y = y;
		Position.Z = z;

		// Set dirty flag
		IsChanged = true;
	}

	// Установка размера в мировых координатах
	void SetWorldScale(float x, float y, float z)
	{
		Scale.X = x;
		Scale.Y = y;
		Scale.Z = z;

		// Set dirty flag
		IsChanged = true;
	}

	// Установка вращения в мировых координатах
	void SetWorldRotation(float x, float y, float z)
	{
		Rotation.X = x;
		Rotation.Y = y;
		Rotation.Z = z;

		// Set dirty flag
		IsChanged = true;
	}

	bool IsDirty()
	{
		return IsChanged;
	}

	void ClearDirtyFlag()
	{
		IsChanged = false;
	}

private:
	// Dirty flag
	bool IsChanged = true;
};