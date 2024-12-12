#include "Object.h"
#include "GameTimer.h"
#include "Scene.h"
#include "DXSampleHelper.h"

Object::Object(Scene* root) : m_root{ root }, m_mappedData{nullptr}
{
}

void Object::BuildConstantBuffer(ID3D12Device* device)
{
    const UINT constantBufferSize = m_root->CalcConstantBufferByteSize(sizeof(ObjectCB));    // CB size is required to be 256-byte aligned.

    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_constantBuffer)));

    // Map and initialize the constant buffer. We don't unmap this until the
    // app closes. Keeping things mapped for the lifetime of the resource is okay.
    CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
    ThrowIfFailed(m_constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(& m_mappedData)));
}

PlayerObject::PlayerObject(Scene* root) : Object{ root } , mRotation{ XMMatrixIdentity()}
{
}

void PlayerObject::OnUpdate(GameTimer& gTimer)
{
    OnKeyboardInput(gTimer);

    ResourceManager& rm = m_root->GetResourceManager();
    // terrain Y �� player Y �����ϱ�.
    XMFLOAT4 pos = GetComponent<Position>().mFloat4;
    float newY = 0.f;
    int width = rm.GetTerrainData().terrainWidth; 
    int height = rm.GetTerrainData().terrainHeight;
    int terrainScale = rm.GetTerrainData().terrainScale;

    if (pos.x > 0 && pos.z > 0 && pos.x < width * terrainScale && pos.z < height * terrainScale) {
        vector<Vertex>& vertexBuffer = rm.GetVertexBuffer();
        UINT startVertex = m_root->GetObj<TerrainObject>(L"TerrainObject").GetComponent<Mesh>().mSubMeshData.startVertexLocation;

        int indexX = (int)(pos.x / terrainScale);
        int indexZ = (int)(pos.z / terrainScale);

        float leftBottom = vertexBuffer[startVertex + indexZ * width + indexX].position.y;
        float rightBottom = vertexBuffer[startVertex + indexZ * width + indexX + 1].position.y;
        float leftTop = vertexBuffer[startVertex + (indexZ + 1) * width + indexX].position.y;
        float rightTop = vertexBuffer[startVertex + (indexZ + 1) * width + indexX + 1].position.y;

        float offsetX = pos.x / terrainScale - indexX;
        float offsetZ = pos.z / terrainScale - indexZ;

        float lerpXBottom = (1 - offsetX) * leftBottom + offsetX * rightBottom;
        float lerpXTop = (1 - offsetX) * leftTop + offsetX * rightTop;

        float lerpZ = (1 - offsetZ) * lerpXBottom + offsetZ * lerpXTop;

        newY = lerpZ;
    }

    XMVECTOR newPos = XMVECTOR{pos.x, newY, pos.z};
    GetComponent<Position>().SetXMVECTOR(newPos);
    // terrain Y �� player Y �����ϱ�. end

    XMMATRIX scale = XMMatrixScalingFromVector(GetComponent<Scale>().GetXMVECTOR());

    GetComponent<Rotation>().SetXMVECTOR(GetComponent<Rotation>().GetXMVECTOR() + GetComponent<Rotate>().GetXMVECTOR() * gTimer.DeltaTime());
    XMMATRIX rotate = XMMatrixRotationRollPitchYawFromVector(GetComponent<Rotation>().GetXMVECTOR() * (XM_PI / 180.0f));



    if (FindComponent<Gravity>()) {
        float& t = GetComponent<Gravity>().mGravityTime;
        float y = XMVectorGetY(GetComponent<Position>().GetXMVECTOR());
        if (y > newY) {
            t += gTimer.DeltaTime();
            //currentFileName = "1P(boy-jump).fbx";
            GetComponent<Velocity>().SetXMVECTOR(XMVectorSetY(GetComponent<Velocity>().GetXMVECTOR(), 0.5 * -9.8 * (t * t)));
        }
        else {
            t = 0;
        }
    }

    XMVECTOR velocity = GetComponent<Velocity>().GetXMVECTOR();
    string currentFileName{};
    if (XMVector4Equal(velocity, XMVectorZero())) {
        currentFileName = "1P(boy-idle).fbx";
    }
    else if (XMVectorGetY(velocity) != 0.f) {
        currentFileName = "1P(boy-jump).fbx";
    }
    else if (XMVectorGetX(velocity) != 0 || XMVectorGetZ(velocity) != 0) {
        currentFileName = "1P(boy-walk).fbx";
    }

    GetComponent<Position>().SetXMVECTOR(GetComponent<Position>().GetXMVECTOR() + GetComponent<Velocity>().GetXMVECTOR() * gTimer.DeltaTime());
    GetComponent<Velocity>().SetXMVECTOR(XMVectorZero());
    XMMATRIX translate = XMMatrixTranslationFromVector(GetComponent<Position>().GetXMVECTOR());

    XMMATRIX world = XMMatrixIdentity();

    // ���� ��� = ũ�� ��� * ȸ�� ��� * �̵� ���
    if (currentFileName == "1P(boy-jump).fbx")
        world = scale * mRotation * translate;
    else
        world = scale * mRotation * rotate * translate;

    memcpy(m_mappedData , &XMMatrixTranspose(world), sizeof(XMMATRIX)); // ó�� �Ű������� �����ּ�

    int isAnimate = FindComponent<Animation>();
    if (isAnimate) {
        vector<XMFLOAT4X4> finalTransforms{90};
        Animation& animComponent = GetComponent<Animation>();
        SkinnedData& animData = animComponent.mAnimData->at(currentFileName);
        animComponent.mAnimationTime += gTimer.DeltaTime();
        string clipName = "Take 001";
        if (currentFileName == "1P(boy-jump).fbx") clipName = "mixamo.com";
        if (animComponent.mAnimationTime >= animData.GetClipEndTime(clipName)) animComponent.mAnimationTime = 0.f;
        animData.GetFinalTransforms(clipName, animComponent.mAnimationTime, finalTransforms);
        memcpy(m_mappedData + sizeof(XMMATRIX), finalTransforms.data(), sizeof(XMMATRIX) * 90); // ó�� �Ű������� �����ּ�
    }
    memcpy(m_mappedData + sizeof(XMMATRIX) * 91, &isAnimate, sizeof(int));
};

void PlayerObject::OnRender(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
    CD3DX12_GPU_DESCRIPTOR_HANDLE hDescriptor(m_root->GetDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
    hDescriptor.Offset(1+GetComponent<Texture>().mDescriptorStartIndex, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
    commandList->SetGraphicsRootDescriptorTable(1, hDescriptor);
    commandList->SetGraphicsRootConstantBufferView(2, m_constantBuffer.Get()->GetGPUVirtualAddress());
    SubMeshData& data = GetComponent<Mesh>().mSubMeshData;
    //commandList->DrawIndexedInstanced(data.indexCountPerInstance, 1, data.startIndexLocation, data.baseVertexLocation, 0);
    commandList->DrawInstanced(data.vertexCountPerInstance, 1, data.startVertexLocation, 0);
}

void PlayerObject::OnKeyboardInput(const GameTimer& gTimer)
{
    float speed = 30;
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
        speed = 100;
    }

    XMMATRIX view = m_root->GetObj<CameraObject>(L"CameraObject").GetXMMATRIX();
    XMMATRIX invView = XMMatrixInverse(NULL, view);
    XMMATRIX transposeView = XMMatrixTranspose(view);

    XMVECTOR forward = XMVECTOR{ 0.f, 0.f, 1.f, 0.f };
    XMVECTOR forwardInv = XMVector4Normalize(XMVector4Transform(forward, invView));
    XMVECTOR right = XMVECTOR{ 1.f, 0.f, 0.f, 0.f };
    XMVECTOR rightInv = XMVector4Normalize(XMVector4Transform(right, invView));

    forward = XMVector4Normalize(XMVectorSetY(forward, 0.f));
    forwardInv = XMVector4Normalize(XMVectorSetY(forwardInv, 0.f));
    rightInv = XMVector4Normalize(XMVectorSetY(rightInv, 0.f));
    XMVECTOR up = XMVECTOR{ 0.f, 1.f, 0.f, 0.f };
    XMVECTOR eyePos = XMVECTOR{ 0.f, 0.f, 0.f, 0.f };

    XMVECTOR velocity = XMVectorZero();

    if (GetKeyState('W') & 0x8000) {
        velocity += forwardInv;
    }
    if (GetKeyState('A') & 0x8000) {
        velocity += -rightInv;
    }
    if (GetKeyState('S') & 0x8000) {
        velocity += -forwardInv;
    }
    if (GetKeyState('D') & 0x8000) {
        velocity += rightInv;
    }

    //float& t = GetComponent<Animation>().mSleepTime; // ��������
    //if (GetKeyState('V') & 0x8000) {
    //    t += gTimer.DeltaTime();
    //    if (t > 0.3f) {
    //        velocity += up;
    //        t = 0.f;
    //    };
    //}

    if (GetAsyncKeyState('P') & 0x8000) {
        XMFLOAT4 pos = GetComponent<Position>().mFloat4;
        OutputDebugStringA(string{ to_string(pos.x) + "," + to_string(pos.y) + "," + to_string(pos.z) + "\n" }.c_str());
    }

    GetComponent<Velocity>().SetXMVECTOR(XMVector4Normalize(velocity) * speed);

    if (XMVectorGetX(velocity) == 0.f && XMVectorGetZ(velocity) == 0.f) return;

    // mRotation ����� ���鶧 y ��ǥ�� �׻� 0 �̿����Ѵ�.
    velocity = XMVectorSetY(velocity, 0.f);
    velocity = XMVector4Normalize(velocity);
    mRotation = XMMATRIX(XMVector3Cross(up, velocity), up, velocity, XMVECTOR{ 0.f, 0.f, 0.f, 1.f });
}

TestObject::TestObject(Scene* root) : Object{root}
{
}

void TestObject::OnUpdate(GameTimer& gTimer)
{
    //GetComponent<Scale>().SetXMVECTOR(GetComponent<Scale>().GetXMVECTOR() * GetComponent<Scale>().mScaleValue);
    XMMATRIX scale = XMMatrixScalingFromVector(GetComponent<Scale>().GetXMVECTOR());

    GetComponent<Rotation>().SetXMVECTOR(GetComponent<Rotation>().GetXMVECTOR() + GetComponent<Rotate>().GetXMVECTOR() * gTimer.DeltaTime());
    XMMATRIX rotate = XMMatrixRotationRollPitchYawFromVector(GetComponent<Rotation>().GetXMVECTOR() * (XM_PI / 180.0f));
    
    if (FindComponent<Gravity>()) {
        float& t = GetComponent<Gravity>().mGravityTime;
        t += gTimer.DeltaTime(); // t�� �ʱ�ȭ �ϴ� ���ǵ� �����ؾ���.
        float y = XMVectorGetY(GetComponent<Position>().GetXMVECTOR());
        if (y > 0) {
            GetComponent<Velocity>().SetXMVECTOR(XMVectorSetY(GetComponent<Velocity>().GetXMVECTOR(), 0.5 * -9.8 * (t * t)));
        }
        else {
            t = 0;
        }
    }

    GetComponent<Position>().SetXMVECTOR(GetComponent<Position>().GetXMVECTOR() + GetComponent<Velocity>().GetXMVECTOR() * gTimer.DeltaTime());
    GetComponent<Velocity>().SetXMVECTOR(XMVECTOR{ 0,0,0,0 });
    XMMATRIX translate = XMMatrixTranslationFromVector(GetComponent<Position>().GetXMVECTOR());

    // ���� ��� = ũ�� ��� * ȸ�� ��� * �̵� ���
    XMMATRIX world = scale * rotate * translate;
    
    //�ִϸ��̼� ����
    memcpy(m_mappedData, &XMMatrixTranspose(world), sizeof(XMMATRIX)); // ó�� �Ű������� �����ּ�

    int isAnimate = FindComponent<Animation>();
    if (isAnimate) {
        vector<XMFLOAT4X4> finalTransforms{ 90 };
        Animation& animComponent = GetComponent<Animation>();
        SkinnedData& animData = animComponent.mAnimData->at("humanoid.fbx");
        animComponent.mAnimationTime += gTimer.DeltaTime();
        if (animComponent.mAnimationTime > animData.GetClipEndTime("shot")) animComponent.mAnimationTime = 0.f;
        animData.GetFinalTransforms("shot", animComponent.mAnimationTime, finalTransforms);
        memcpy(m_mappedData + sizeof(XMFLOAT4X4), finalTransforms.data(), sizeof(XMFLOAT4X4) * 90); // ó�� �Ű������� �����ּ�
    }
    memcpy(m_mappedData + sizeof(XMFLOAT4X4) * 91, &isAnimate, sizeof(int));
}

void TestObject::OnRender(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
    CD3DX12_GPU_DESCRIPTOR_HANDLE hDescriptor(m_root->GetDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
    hDescriptor.Offset(1 + GetComponent<Texture>().mDescriptorStartIndex, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
    commandList->SetGraphicsRootDescriptorTable(1, hDescriptor);
    commandList->SetGraphicsRootConstantBufferView(2, m_constantBuffer.Get()->GetGPUVirtualAddress());
    SubMeshData& data = GetComponent<Mesh>().mSubMeshData;
    //commandList->DrawIndexedInstanced(data.indexCountPerInstance, 1, data.startIndexLocation, data.baseVertexLocation, 0);
    commandList->DrawInstanced(data.vertexCountPerInstance, 1, data.startVertexLocation, 0);
}

CameraObject::CameraObject(float radius, Scene* root) :
    Object{ root }, 
    mLastPosX{ -1 }, 
    mLastPosY{ -1 }, 
    mTheta{XMConvertToRadians(-90)}, 
    mPhi{ XMConvertToRadians(70) }, 
    mRadius{radius}
{
}

void CameraObject::OnUpdate(GameTimer& gTimer)
{
    float x = mRadius * sinf(mPhi) * cosf(mTheta);
    float y = mRadius * cosf(mPhi);
    float z = mRadius * sinf(mPhi) * sinf(mTheta);

    XMVECTOR playerPos = m_root->GetObj<PlayerObject>(L"PlayerObject").GetComponent<Position>().GetXMVECTOR();
    GetComponent<Position>().SetXMVECTOR(playerPos + XMVECTOR{ x, y, z, 0.f });

    // ī�޶� ��ȯ ���.
    XMVECTOR eye = GetComponent<Position>().GetXMVECTOR();
    XMVECTOR target = playerPos + XMVECTOR{0.f, 5.f, 0.f, 0.f};
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    SetXMMATRIX(XMMatrixLookAtLH(eye, target, up));
    // ī�޶� ��ȯ ��� ���̴��� ����
    memcpy(m_root->GetConstantBufferMappedData(), &XMMatrixTranspose(GetXMMATRIX()), sizeof(XMMATRIX)); // ó�� �Ű������� �����ּ�
}

void CameraObject::OnRender(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
}

void CameraObject::OnMouseInput(WPARAM wParam, HWND hWnd)
{
    // ���� wnd�� ���� ��ǥ�� �˾ƿ´�
    RECT clientRect{};
    GetWindowRect(hWnd, &clientRect);
    int width = int(clientRect.right - clientRect.left);
    int height = int(clientRect.bottom - clientRect.top);
    int centerX = clientRect.left + width / 2;
    int centerY = clientRect.top + height / 2;

    POINT currentMousePos;
    GetCursorPos(&currentMousePos);
    float dx = static_cast<float>(currentMousePos.x - centerX);
    float dy = static_cast<float>(currentMousePos.y - centerY);
    mTheta -= XMConvertToRadians(dx * 0.02f);
    mPhi -= XMConvertToRadians(dy * 0.02f);

    // ���� clamp
    float min = 0.1f;
    float max = XM_PI - 0.1f;
    mPhi = mPhi < min ? min : (mPhi > max ? max : mPhi);

    SetCursorPos(centerX, centerY);
}

void CameraObject::SetXMMATRIX(XMMATRIX& m)
{
    XMStoreFloat4x4(&mViewMatrix, m);
}

XMMATRIX CameraObject::GetXMMATRIX()
{
    return XMLoadFloat4x4(&mViewMatrix);
}

TerrainObject::TerrainObject(Scene* root) : Object{ root }
{
}

void TerrainObject::OnUpdate(GameTimer& gTimer)
{
    XMMATRIX scale = XMMatrixScalingFromVector(GetComponent<Scale>().GetXMVECTOR());

    GetComponent<Rotation>().SetXMVECTOR(GetComponent<Rotation>().GetXMVECTOR() + GetComponent<Rotate>().GetXMVECTOR() * gTimer.DeltaTime());
    XMMATRIX rotate = XMMatrixRotationRollPitchYawFromVector(GetComponent<Rotation>().GetXMVECTOR() * (XM_PI / 180.0f));

    if (FindComponent<Gravity>()) {
        float& t = GetComponent<Gravity>().mGravityTime;
        t += gTimer.DeltaTime(); // t�� �ʱ�ȭ �ϴ� ���ǵ� �����ؾ���.
        float y = XMVectorGetY(GetComponent<Position>().GetXMVECTOR());
        if (y > 0) {
            GetComponent<Velocity>().SetXMVECTOR(XMVectorSetY(GetComponent<Velocity>().GetXMVECTOR(), 0.5 * -9.8 * (t * t)));
        }
        else {
            t = 0;
        }
    }

    GetComponent<Position>().SetXMVECTOR(GetComponent<Position>().GetXMVECTOR() + GetComponent<Velocity>().GetXMVECTOR() * gTimer.DeltaTime());
    GetComponent<Velocity>().SetXMVECTOR(XMVECTOR{ 0,0,0,0 });
    XMMATRIX translate = XMMatrixTranslationFromVector(GetComponent<Position>().GetXMVECTOR());

    // ���� ��� = ũ�� ��� * ȸ�� ��� * �̵� ���
    XMMATRIX world = scale * rotate * translate;
    memcpy(m_mappedData, &XMMatrixTranspose(world), sizeof(XMMATRIX)); // ó�� �Ű������� �����ּ�

    //�ִϸ��̼� ����
    int isAnimate = FindComponent<Animation>();
    if (isAnimate) {
        vector<XMFLOAT4X4> finalTransforms{ 90 };
        Animation& animComponent = GetComponent<Animation>();
        SkinnedData& animData = animComponent.mAnimData->at("humanoid.fbx");
        animComponent.mAnimationTime += gTimer.DeltaTime();
        if (animComponent.mAnimationTime > animData.GetClipEndTime("shot")) animComponent.mAnimationTime = 0.f;
        animData.GetFinalTransforms("shot", animComponent.mAnimationTime, finalTransforms);
        memcpy(m_mappedData + sizeof(XMFLOAT4X4), finalTransforms.data(), sizeof(XMFLOAT4X4) * 90); // ó�� �Ű������� �����ּ�
    }
    memcpy(m_mappedData + sizeof(XMFLOAT4X4) * 91, &isAnimate, sizeof(int));
}

void TerrainObject::OnRender(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
    CD3DX12_GPU_DESCRIPTOR_HANDLE hDescriptor(m_root->GetDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
    hDescriptor.Offset(1 + GetComponent<Texture>().mDescriptorStartIndex, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
    commandList->SetGraphicsRootDescriptorTable(1, hDescriptor);
    commandList->SetGraphicsRootConstantBufferView(2, m_constantBuffer.Get()->GetGPUVirtualAddress());
    SubMeshData& data = GetComponent<Mesh>().mSubMeshData;
    commandList->DrawIndexedInstanced(data.indexCountPerInstance, 1, data.startIndexLocation, data.baseVertexLocation, 0);
    //commandList->DrawInstanced(data.vertexCountPerInstance, 1, data.startVertexLocation, 0);
}
