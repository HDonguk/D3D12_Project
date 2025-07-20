#pragma once
#include "stdafx.h"
#include "Component.h"

class GameTimer;
class Scene;
class Object
{
public:
	virtual ~Object();
	Object(Scene* scene, uint32_t id, uint32_t parentId = -1);
	virtual void OnUpdate(GameTimer& gTimer);
	virtual void OnProcessCollision(Object& other, XMVECTOR collisionNormal, float penetration);
	virtual void LateUpdate(GameTimer& gTimer);
	virtual void OnRender(ID3D12Device* device, ID3D12GraphicsCommandList * commandList);
	void BuildConstantBuffer(ID3D12Device* device);
	void AddComponent(Component* component);
	Scene* GetScene() { return m_scene; }
	void ProcessAnimation(GameTimer& gTimer);
	uint32_t GetId();
	bool GetValid();
	void Delete();

	template <typename T>
	T* GetComponent() 
	{
		T* temp = nullptr;
		for (Component* component : m_components){
			temp = dynamic_cast<T*>(component);
			if (temp) break;
		}
		return temp; 
	}

protected:
	Scene* m_scene = nullptr;
	uint32_t m_id = -1;
	uint32_t m_parent_id = -1;
	bool m_valid = true;
	vector<Component*> m_components;

	// ������Ʈ ���� �������� CB
	UINT8* m_mappedData = nullptr;
	ComPtr<ID3D12Resource> m_constantBuffer;
};

class PlayerObject : public Object
{
public:
	using Object::Object;
	void OnUpdate(GameTimer& gTimer) override;
	void OnProcessCollision(Object& other, XMVECTOR collisionNormal, float penetration) override;
private:
	void OnKeyboardInput(const GameTimer& gTimer);
};

class CameraObject : public Object
{
public:
	using Object::Object;
	void OnUpdate(GameTimer& gTimer) override;
	void LateUpdate(GameTimer& gTimer) override;
	void OnMouseInput(WPARAM wParam, HWND hWnd);
private:
	int mLastPosX = -1;
	int mLastPosY = -1;
	float mTheta = XMConvertToRadians(-90.0f);
	float mPhi = XMConvertToRadians(70.0f);
	float mRadius = 70.0f;
};

class TerrainObject : public Object
{
public:
	using Object::Object;
};

class TestObject : public Object
{
public:
	using Object::Object;
	void OnProcessCollision(Object& other, XMVECTOR collisionNormal, float penetration) override;
};

class TreeObject : public Object
{
public:
	using Object::Object;
};

class TigerObject : public Object
{
public:
	using Object::Object;
	void OnUpdate(GameTimer& gTimer) override;
	void OnProcessCollision(Object& other, XMVECTOR collisionNormal, float penetration) override;
private:
	void TigerBehavior(GameTimer& gTimer);
	void RandomVelocity(GameTimer& gTimer);
	float mTimer = 0.0f;
	XMFLOAT3 mTempVelocity{};
};

class StoneObject : public Object
{
public:
	using Object::Object;
};