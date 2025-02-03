#pragma once
#include "stdafx.h"
#include "Object.h"
#include "ResourceManager.h"
#include <utility>

class GameTimer;

class Scene
{
public:
    Scene() = default;
    Scene(UINT width, UINT height, wstring name);

    virtual void OnInit(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
    virtual void OnUpdate(GameTimer& gTimer);
    virtual void CheckCollision();
    virtual void LateUpdate(GameTimer& gTimer);
    virtual void OnRender(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
    virtual void OnResize(UINT width, UINT height);
    virtual void OnDestroy();

    virtual void OnKeyDown(UINT8 key);
    virtual void OnKeyUp(UINT8 key);

    void SetState(ID3D12GraphicsCommandList* commandList);
    void SetDescriptorHeaps(ID3D12GraphicsCommandList* commandList);

    wstring GetSceneName() const;
    ResourceManager& GetResourceManager();

    template<typename T>
    void AddObj(const wstring& name, T&& object) { m_objects.emplace(name, move(object)); }

    template<typename T>
    T& GetObj(const wstring& name) { return get<T>(m_objects.at(name)); }

    void* GetConstantBufferMappedData();
    ID3D12DescriptorHeap* GetDescriptorHeap();

    UINT CalcConstantBufferByteSize(UINT byteSize);

private:
    void BuildRootSignature(ID3D12Device* device);
    void BuildPSO(ID3D12Device* device);
    void BuildVertexBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
    void BuildIndexBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
    void BuildVertexBufferView();
    void BuildIndexBufferView();
    void BuildConstantBuffer(ID3D12Device* device);
    void BuildConstantBufferView(ID3D12Device* device);
    void BuildTextureBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
    void BuildTextureBufferView(ID3D12Device* device);
    void BuildDescriptorHeap(ID3D12Device* device);
    void BuildProjMatrix();
    void BuildObjects(ID3D12Device* device);
    void LoadMeshAnimationTexture();

    wstring m_name;
    unordered_map<wstring, ObjectVariant> m_objects;
    unique_ptr<ResourceManager> m_resourceManager;
    //
    CD3DX12_VIEWPORT m_viewport;
    CD3DX12_RECT m_scissorRect;
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12PipelineState> m_pipelineState;
    // App resources.
    ComPtr<ID3D12DescriptorHeap> m_descriptorHeap;
    UINT m_cbvsrvuavDescriptorSize;
    //
    ComPtr<ID3D12Resource> m_vertexBuffer_default;
    ComPtr<ID3D12Resource> m_indexBuffer_default;
    ComPtr<ID3D12Resource> m_vertexBuffer_upload;
    ComPtr<ID3D12Resource> m_indexBuffer_upload;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView;
    //
    unordered_map<wstring, int> m_subTextureData;
    vector<wstring> m_DDSFileName;
    vector<ComPtr<ID3D12Resource>> m_textureBuffer_defaults;
    vector<ComPtr<ID3D12Resource>> m_textureBuffer_uploads;
    //
    ComPtr<ID3D12Resource> m_constantBuffer;
    void* m_mappedData;
    //
    XMFLOAT4X4 m_proj;
};
