#pragma once
#include "stdafx.h"
#include "Info.h"
#include <variant>
#include "FbxExtractor.h"
class Object;

struct Component // ��ü�� ������ �ʴ� Ŭ����
{
	Component() = default;
	Component(Object* root) : mRoot{root} {}
	virtual ~Component() = default;
	Object* mRoot;
};

struct NeedVector : Component // ��ü�� ������ �ʴ� Ŭ����
{
	NeedVector() = default;
	NeedVector(float x, float y, float z, float w, Object* root) : Component{root}, mFloat4 { x, y, z, w } {}
	XMVECTOR& GetXMVECTOR() { return XMLoadFloat4(&mFloat4); }
	void SetXMVECTOR(XMVECTOR& v) {
		XMStoreFloat4(&mFloat4, v);
	}
	XMFLOAT4 mFloat4;
};

//struct World : public Component
//{
//	World() = default;
//	World(Object* root) : Component{ root }, mWorld{} {}
//	XMMATRIX GetXMMATRIX() { return XMLoadFloat4x4(&mWorld); }
//	void SetXMMATRIX(XMMATRIX& m) { XMStoreFloat4x4(&mWorld, m); }
//	XMFLOAT4X4 mWorld;
//};

struct Mesh : public Component
{ 
	Mesh() = default;
	Mesh(SubMeshData& subMeshData, Object* root) : Component{ root }, mSubMeshData{ subMeshData } {}
	SubMeshData mSubMeshData;
};

struct Texture : public Component
{
	Texture() = default;
	Texture(int descriptorStartIndex, Object* root) : Component{ root }, mDescriptorStartIndex{descriptorStartIndex} {}
	int mDescriptorStartIndex;
};

struct Animation : public Component
{
	Animation() = default;
	Animation(unordered_map<string, SkinnedData>& animData, Object* root) : Component{ root }, mAnimData{ &animData }, time{0.f} {}
	unordered_map<string, SkinnedData>* mAnimData;
	float time;
};

struct Position : public NeedVector
{
	Position() = default;
	Position(float x, float y, float z, float w, Object* root) : NeedVector{ x, y, z, w , root}, mModify(true) {}
	bool mModify;
};

struct Velocity : public NeedVector
{
	Velocity() = default;
	Velocity(float x, float y, float z, float w, Object* root) : NeedVector{ x, y, z, w, root } {}
};

struct Rotation : public NeedVector
{
	Rotation() = default;
	Rotation(float x, float y, float z, float w, Object* root) : NeedVector{ x, y, z, w, root }, mModify(true) {}
	bool mModify;
};

struct Rotate : public NeedVector
{
	Rotate() = default;
	Rotate(float x, float y, float z, float w, Object* root) : NeedVector{ x, y, z, w, root } {}
};

struct Scale : public NeedVector
{
	Scale() = default;
	Scale(float x, Object* root) : NeedVector{ x, x, x, x, root }, mScaleValue{x} {}
	float mScaleValue;
};

struct Gravity : public NeedVector
{
	Gravity() = default;
	Gravity(float value, Object* root) : NeedVector{ 0.f, -value, 0.f, 0.f, root }, mGravityTime{0.f} {}
	float mGravityTime;
};

//struct CameraPosition : NeedVector
//{
//	CameraPosition() = default;
//	CameraPosition(float x, float y, float z, float w, Object* root) :
//		NeedVector{ x,y,z,w,root }, mLastPosX{}, mLastPosY{}, mTheta{}, mPhi{} {}
//	int mLastPosX;
//	int mLastPosY;
//	float mTheta;
//	float mPhi;
//};

using ComponentVariant = variant<Mesh, Position, Velocity, Rotation, Rotate, Scale, Texture, Animation, Gravity>;
