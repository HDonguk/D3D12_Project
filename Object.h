#pragma once
#include "stdafx.h"
#include "Component.h"
#include <variant>

class GameTimer;
class Scene;

class Object
{
public:
	Object() = default;
	Object(Scene* root);
	virtual ~Object() = default;

	//virtual void OnInit(ID3D12Device* device);
	virtual void OnUpdate(GameTimer& gTimer) = 0;
	virtual void OnRender(ID3D12Device* device, ID3D12GraphicsCommandList * commandList) = 0;
	//virtual void OnDestroy();

	void BuildConstantBuffer(ID3D12Device* device);

	template<typename T>
	void AddComponent(T&& component) { m_components.emplace(typeid(T).name(), move(component)); }

	template <typename T>
	T& GetComponent() { return get<T>(m_components.at(typeid(T).name())); }

	template <typename T>
	bool FindComponent() { 
		auto& it = m_components.find(typeid(T).name());
		return it != m_components.end();
	}

protected:
	Scene* m_root;
	unordered_map<string, ComponentVariant> m_components;

	// ������Ʈ ���� �������� CB
	UINT8* m_mappedData;
	ComPtr<ID3D12Resource> m_constantBuffer;
};

class PlayerObject : public Object
{
public:
	PlayerObject() = default;
	PlayerObject(Scene* root);
	void OnUpdate(GameTimer& gTimer) override;
	void OnRender(ID3D12Device* device, ID3D12GraphicsCommandList* commandList) override;
	void OnKeyboardInput(const GameTimer& gTimer);
private:
	XMMATRIX mRotation; // Ű���� ��ǲ �Լ����� ���ߴ� ī�޶� ��ǥ�踦 �������� �ϴ� ȸ������̴�. �� ���� �Լ� ���ο��� ������Ʈ�� rotate�� ���ϰ� �� ����� �ٽ� ������Ʈ�� �����ϸ� �� ������ ��� �ɵ�. ���߿� ��ġ��.
};

class CameraObject : public Object
{
public:
	CameraObject() = default;
	CameraObject(float radius, Scene* root);
	void OnUpdate(GameTimer& gTimer) override;
	void OnRender(ID3D12Device* device, ID3D12GraphicsCommandList* commandList) override;
	void OnMouseInput(WPARAM wParam, HWND hWnd);
	void SetXMMATRIX(XMMATRIX& m);
	XMMATRIX GetXMMATRIX();
private:
	int mLastPosX;
	int mLastPosY;
	float mTheta;
	float mPhi;
	float mRadius;
	XMFLOAT4X4 mViewMatrix;
};

class TerrainObject : public Object
{
public:
	TerrainObject() = default;
	TerrainObject(Scene* root);
	void OnUpdate(GameTimer& gTimer) override;
	void OnRender(ID3D12Device* device, ID3D12GraphicsCommandList* commandList) override;
};

class TestObject : public Object
{
public:
	TestObject() = default;
	TestObject(Scene* root);
	void OnUpdate(GameTimer& gTimer) override;
	void OnRender(ID3D12Device* device, ID3D12GraphicsCommandList* commandList) override;
};

class TreeObject : public Object
{
public:
	TreeObject() = default;
	TreeObject(Scene* root);
	void OnUpdate(GameTimer& gTimer) override;
	void OnRender(ID3D12Device* device, ID3D12GraphicsCommandList* commandList) override;
};

using ObjectVariant = variant<PlayerObject, CameraObject, TestObject, TerrainObject, TreeObject>;