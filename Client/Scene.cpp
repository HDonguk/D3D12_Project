#include "NetworkManager.h"
#include "Scene.h"
#include "DXSampleHelper.h"
#include "GameTimer.h"
#include "string"
#include "info.h"
#include "OtherPlayerManager.h"

Scene::Scene(UINT width, UINT height, std::wstring name) :
    m_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
    m_scissorRect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height)),
    m_name(name),
    m_mappedData(nullptr)
{
}

Scene::Scene(Scene&& other) noexcept :
    m_viewport(other.m_viewport),
    m_scissorRect(other.m_scissorRect),
    m_name(std::move(other.m_name)),
    m_objects(std::move(other.m_objects)),
    m_resourceManager(std::move(other.m_resourceManager)),
    m_rootSignature(std::move(other.m_rootSignature)),
    m_pipelineState(std::move(other.m_pipelineState)),
    m_descriptorHeap(std::move(other.m_descriptorHeap)),
    m_cbvsrvuavDescriptorSize(other.m_cbvsrvuavDescriptorSize),
    m_vertexBuffer_default(std::move(other.m_vertexBuffer_default)),
    m_indexBuffer_default(std::move(other.m_indexBuffer_default)),
    m_vertexBuffer_upload(std::move(other.m_vertexBuffer_upload)),
    m_indexBuffer_upload(std::move(other.m_indexBuffer_upload)),
    m_vertexBufferView(other.m_vertexBufferView),
    m_indexBufferView(other.m_indexBufferView),
    m_subTextureData(std::move(other.m_subTextureData)),
    m_DDSFileName(std::move(other.m_DDSFileName)),
    m_textureBuffer_defaults(std::move(other.m_textureBuffer_defaults)),
    m_textureBuffer_uploads(std::move(other.m_textureBuffer_uploads)),
    m_constantBuffer(std::move(other.m_constantBuffer)),
    m_mappedData(other.m_mappedData),
    m_proj(other.m_proj),
    m_device(other.m_device),
    m_pendingTigerSpawns(std::move(other.m_pendingTigerSpawns)),
    m_tigerInterpolationData(std::move(other.m_tigerInterpolationData))
{
    other.m_mappedData = nullptr;
    other.m_device = nullptr;
}

Scene& Scene::operator=(Scene&& other) noexcept
{
    if (this != &other)
    {
        m_viewport = other.m_viewport;
        m_scissorRect = other.m_scissorRect;
        m_name = std::move(other.m_name);
        m_objects = std::move(other.m_objects);
        m_resourceManager = std::move(other.m_resourceManager);
        m_rootSignature = std::move(other.m_rootSignature);
        m_pipelineState = std::move(other.m_pipelineState);
        m_descriptorHeap = std::move(other.m_descriptorHeap);
        m_cbvsrvuavDescriptorSize = other.m_cbvsrvuavDescriptorSize;
        m_vertexBuffer_default = std::move(other.m_vertexBuffer_default);
        m_indexBuffer_default = std::move(other.m_indexBuffer_default);
        m_vertexBuffer_upload = std::move(other.m_vertexBuffer_upload);
        m_indexBuffer_upload = std::move(other.m_indexBuffer_upload);
        m_vertexBufferView = other.m_vertexBufferView;
        m_indexBufferView = other.m_indexBufferView;
        m_subTextureData = std::move(other.m_subTextureData);
        m_DDSFileName = std::move(other.m_DDSFileName);
        m_textureBuffer_defaults = std::move(other.m_textureBuffer_defaults);
        m_textureBuffer_uploads = std::move(other.m_textureBuffer_uploads);
        m_constantBuffer = std::move(other.m_constantBuffer);
        m_mappedData = other.m_mappedData;
        m_proj = other.m_proj;
        m_device = other.m_device;
        m_pendingTigerSpawns = std::move(other.m_pendingTigerSpawns);
        m_tigerInterpolationData = std::move(other.m_tigerInterpolationData);

        other.m_mappedData = nullptr;
        other.m_device = nullptr;
    }
    return *this;
}

void Scene::OnInit(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
    m_device = device; // device 멤버 변수 저장
    Initialize(); // device 초기화 상태 확인
    
    LoadMeshAnimationTexture();
    BuildProjMatrix();
    BuildObjects(device);
    BuildRootSignature(device);
    BuildPSO(device);
    BuildVertexBuffer(device, commandList);
    BuildIndexBuffer(device, commandList);
    BuildTextureBuffer(device, commandList);
    BuildConstantBuffer(device);
    BuildDescriptorHeap(device);
    BuildVertexBufferView();
    BuildIndexBufferView();
    BuildConstantBufferView(device);
    BuildTextureBufferView(device);
}

void Scene::BuildObjects(ID3D12Device* device)
{
    ResourceManager& rm = GetResourceManager();
    auto& subMeshData = rm.GetSubMeshData();
    auto& animData = rm.GetAnimationData();

    Object* objectPtr = nullptr;

    AddObj(L"PlayerObject", PlayerObject{ this });
    objectPtr = &GetObj<PlayerObject>(L"PlayerObject");
    objectPtr->AddComponent(Position{ 600.f, 0.f, 600.f, 1.f, objectPtr });
    objectPtr->AddComponent(Velocity{ 0.f, 0.f, 0.f, 0.f, objectPtr });
    objectPtr->AddComponent(Rotation{ 0.0f, 180.0f, 0.0f, 0.0f, objectPtr });
    objectPtr->AddComponent(Rotate{ 0.0f, 0.0f, 0.0f, 0.0f, objectPtr });
    objectPtr->AddComponent(Scale{ 0.1f, objectPtr });
    objectPtr->AddComponent(Mesh{ subMeshData.at("1P(boy-idle).fbx"), objectPtr });
    objectPtr->AddComponent(Texture{ m_subTextureData.at(L"boy"), objectPtr });
    objectPtr->AddComponent(Animation{ animData, objectPtr });
    objectPtr->AddComponent(Gravity{ 2.f, objectPtr });
    objectPtr->AddComponent(Collider{ 0.f, 0.f, 0.f, 4.f, 50.f, 4.f, objectPtr });

    // 다른 플레이어 미리 생성 (초기에는 비활성화)
    AddObj(L"OtherPlayer", PlayerObject{ this });
    objectPtr = &GetObj<PlayerObject>(L"OtherPlayer");
    objectPtr->AddComponent(Position{ 600.f, 0.f, 600.f, 1.f, objectPtr });
    objectPtr->AddComponent(Velocity{ 0.f, 0.f, 0.f, 0.f, objectPtr });
    objectPtr->AddComponent(Rotation{ 0.0f, 180.0f, 0.0f, 0.0f, objectPtr });
    objectPtr->AddComponent(Rotate{ 0.0f, 0.0f, 0.0f, 0.0f, objectPtr });
    objectPtr->AddComponent(Scale{ 0.1f, objectPtr });
    objectPtr->AddComponent(Mesh{ subMeshData.at("1P(boy-idle).fbx"), objectPtr });
    objectPtr->AddComponent(Texture{ m_subTextureData.at(L"boy"), objectPtr });
    objectPtr->AddComponent(Animation{ animData, objectPtr });
    objectPtr->AddComponent(Gravity{ 2.f, objectPtr });
    objectPtr->AddComponent(Collider{ 0.f, 0.f, 0.f, 4.f, 50.f, 4.f, objectPtr });
    objectPtr->SetActive(false);  // 초기에는 비활성화
    
    AddObj(L"CameraObject", CameraObject{ 70.f, this });
    objectPtr = &GetObj<CameraObject>(L"CameraObject");
    objectPtr->AddComponent(Position{ 0.f, 0.f, 0.f, 0.f, objectPtr });

    AddObj(L"TerrainObject", TerrainObject{ this });
    objectPtr = &GetObj<TerrainObject>(L"TerrainObject");
    objectPtr->AddComponent(Position{ 0.f, 0.f, 0.f, 1.f, objectPtr });
    objectPtr->AddComponent(Velocity{ 0.f, 0.f, 0.f, 0.f, objectPtr });
    objectPtr->AddComponent(Rotation{ 0.0f, 0.0f, 0.0f, 0.0f, objectPtr });
    objectPtr->AddComponent(Rotate{ 0.0f, 0.0f, 0.0f, 0.0f, objectPtr });
    objectPtr->AddComponent(Scale{ 1.f, objectPtr });
    objectPtr->AddComponent(Mesh{ subMeshData.at("HeightMap.raw") , objectPtr });
    objectPtr->AddComponent(Texture{ m_subTextureData.at(L"grass"), objectPtr });

    //AddObj(L"TestObject", TestObject{ this });
    //objectPtr = &GetObj<TestObject>(L"TestObject");
    //objectPtr->AddComponent(Position{ 0.f, 0.f, 0.f, 1.f, objectPtr });
    //objectPtr->AddComponent(Velocity{ 0.f, 0.f, 0.f, 0.f, objectPtr });
    //objectPtr->AddComponent(Rotation{ 0.0f, 0.0f, 0.0f, 0.0f, objectPtr });
    //objectPtr->AddComponent(Rotate{ 0.0f, 0.0f, 0.0f, 0.0f, objectPtr });
    //objectPtr->AddComponent(Scale{ 0.01f, objectPtr });
    //objectPtr->AddComponent(Mesh{ subMeshData.at("house_1218_attach_fix.fbx") , objectPtr });
    //objectPtr->AddComponent(Texture{ m_subTextureData.at(L"PP_Color_Palette"), objectPtr });

    int repeat{ 17 };
    for (int i = 0; i < repeat; ++i) {
        for (int j = 0; j < repeat; ++j) {
            wstring objectName = L"TreeObject" + to_wstring(j + (repeat * i));
            AddObj(objectName, TreeObject{ this });
            objectPtr = &GetObj<TreeObject>(objectName);
            objectPtr->AddComponent(Position{ 100.f + 150.f * j, 0.f, 100.f + 150.f * i, 1.f, objectPtr });
            objectPtr->AddComponent(Velocity{ 0.f, 0.f, 0.f, 0.f, objectPtr });
            objectPtr->AddComponent(Rotation{ 0.0f, 0.0f, 0.0f, 0.0f, objectPtr });
            objectPtr->AddComponent(Rotate{ 0.0f, 0.0f, 0.0f, 0.0f, objectPtr });
            objectPtr->AddComponent(Scale{ 20.f, objectPtr });
            objectPtr->AddComponent(Mesh{ subMeshData.at("long_tree.fbx") , objectPtr });
            objectPtr->AddComponent(Texture{ m_subTextureData.at(L"longTree"), objectPtr });
            objectPtr->AddComponent(Collider{ 0.f, 0.f, 0.f, 3.f, 50.f, 3.f, objectPtr });
        }
    }
}

void Scene::BuildRootSignature(ID3D12Device* device)
{
    // Create a root signature consisting of a descriptor table with a single CBV.
    D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData{};

    // This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
    featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

    if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
    {
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }

    CD3DX12_DESCRIPTOR_RANGE1 ranges[2] = {};
    ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
    ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

    CD3DX12_ROOT_PARAMETER1 rootParameters[3] = {};
    rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_VERTEX);
    rootParameters[1].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_PIXEL);
    rootParameters[2].InitAsConstantBufferView(1);

    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.MipLODBias = 0;
    sampler.MaxAnisotropy = 0;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    sampler.MinLOD = 0.0f;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;
    sampler.RegisterSpace = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // Allow input layout and deny uneccessary access to certain pipeline stages.
    D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 1, &sampler, rootSignatureFlags);

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));
    ThrowIfFailed(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
}

void Scene::BuildPSO(ID3D12Device* device)
{
    // Create the pipeline state, which includes compiling and loading shaders.
    ComPtr<ID3DBlob> vertexShader;
    ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
    // Enable better shader debugging with the graphics debugging tools.
    UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    UINT compileFlags = 0;
#endif

    ThrowIfFailed(D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr));
    ThrowIfFailed(D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr));

    // Define the vertex input layout.
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "WEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BONEINDEX", 0, DXGI_FORMAT_R32G32B32A32_UINT, 0, 48, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    // Describe and create the graphics pipeline state object (PSO).
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
    psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    ThrowIfFailed(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
}

void Scene::BuildVertexBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
    const UINT vertexBufferSize = m_resourceManager->GetVertexBuffer().size() * sizeof(Vertex);
    // Create the vertex buffer.
    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(m_vertexBuffer_default.GetAddressOf())));

    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(m_vertexBuffer_upload.GetAddressOf())));

    D3D12_SUBRESOURCE_DATA subResourceData{};
    subResourceData.pData = m_resourceManager->GetVertexBuffer().data();
    subResourceData.RowPitch = vertexBufferSize;
    subResourceData.SlicePitch = subResourceData.RowPitch;

    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_vertexBuffer_default.Get(),
        D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
    UpdateSubresources(commandList, m_vertexBuffer_default.Get(), m_vertexBuffer_upload.Get(), 0, 0, 1, &subResourceData);
    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_vertexBuffer_default.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));
}

void Scene::BuildIndexBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
    // Create the index buffer.
    const UINT indexBufferSize = m_resourceManager->GetIndexBuffer().size() * sizeof(uint32_t);

    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize),
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(m_indexBuffer_default.GetAddressOf())));

    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(m_indexBuffer_upload.GetAddressOf())));

    D3D12_SUBRESOURCE_DATA subResourceData = {};
    subResourceData.pData = m_resourceManager->GetIndexBuffer().data();
    subResourceData.RowPitch = indexBufferSize;
    subResourceData.SlicePitch = subResourceData.RowPitch;

    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_indexBuffer_default.Get(),
        D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
    UpdateSubresources(commandList, m_indexBuffer_default.Get(), m_indexBuffer_upload.Get(), 0, 0, 1, &subResourceData);
    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_indexBuffer_default.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));
}

void Scene::BuildVertexBufferView()
{
    // Initialize the vertex buffer view.
    m_vertexBufferView.BufferLocation = m_vertexBuffer_default->GetGPUVirtualAddress();
    m_vertexBufferView.StrideInBytes = sizeof(Vertex);
    m_vertexBufferView.SizeInBytes = m_resourceManager->GetVertexBuffer().size() * sizeof(Vertex);
}

void Scene::BuildIndexBufferView()
{
    m_indexBufferView.BufferLocation = m_indexBuffer_default->GetGPUVirtualAddress();
    m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
    m_indexBufferView.SizeInBytes = m_resourceManager->GetIndexBuffer().size() * sizeof(uint32_t);
}

void Scene::BuildDescriptorHeap(ID3D12Device* device)
{
    D3D12_DESCRIPTOR_HEAP_DESC HeapDesc = {};
    HeapDesc.NumDescriptors = static_cast<UINT>(1 + m_DDSFileName.size());
    HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    HeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    ThrowIfFailed(device->CreateDescriptorHeap(&HeapDesc, IID_PPV_ARGS(m_descriptorHeap.GetAddressOf())));

    m_cbvsrvuavDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void Scene::BuildConstantBuffer(ID3D12Device* device)
{
    const UINT constantBufferSize = CalcConstantBufferByteSize(sizeof(CommonCB));    // CB size is required to be 256-byte aligned.

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
    ThrowIfFailed(m_constantBuffer->Map(0, &readRange, &m_mappedData));

    for (auto& [key, value] : m_objects) {
        visit([device](auto& arg) {arg.BuildConstantBuffer(device); }, value);
    }
}

void Scene::BuildConstantBufferView(ID3D12Device* device)
{
    // Describe and create a constant buffer view.
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
    cbvDesc.BufferLocation = m_constantBuffer->GetGPUVirtualAddress();
    cbvDesc.SizeInBytes = CalcConstantBufferByteSize(sizeof(CommonCB));

    CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(m_descriptorHeap->GetCPUDescriptorHandleForHeapStart());
    hDescriptor.Offset(0, m_cbvsrvuavDescriptorSize);

    device->CreateConstantBufferView(&cbvDesc, hDescriptor);

}

void Scene::BuildTextureBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
    // Create the texture.
    for (auto& fileName : m_DDSFileName)
    {
        ComPtr<ID3D12Resource> defaultBuffer;
        ComPtr<ID3D12Resource> uploadBuffer;

        // DDSTexture  ϴ 
        unique_ptr<uint8_t[]> ddsData;
        vector<D3D12_SUBRESOURCE_DATA> subresources;
        ThrowIfFailed(LoadDDSTextureFromFile(device, fileName.c_str(), defaultBuffer.GetAddressOf(), ddsData, subresources));

        //// DirectTex ϴ 
        //ScratchImage image;
        //TexMetadata metadata;

        //ThrowIfFailed(LoadFromDDSFile(L"./Textures/grass.dds", DDS_FLAGS_NONE, &metadata, image));
        ////metadata = image.GetMetadata(); // ڵ带 ϰ  ڵ 3° ڸ nullptr ص ȴ.

        //ThrowIfFailed(CreateTexture(device, metadata, m_textureBuffer_default.GetAddressOf()));
        //ThrowIfFailed(PrepareUpload(device, image.GetImages(), image.GetImageCount(), metadata, subresources));

        const UINT64 uploadBufferSize = GetRequiredIntermediateSize(defaultBuffer.Get(), 0, subresources.size());

        OutputDebugStringA(string{ "current texture subresource size = " + to_string(subresources.size()) + "\n" }.c_str());
        OutputDebugStringA(string{ "current texture mip level = " + to_string(defaultBuffer->GetDesc().MipLevels) + "\n" }.c_str());
        OutputDebugStringA(string{ "current texture format = " + to_string(defaultBuffer->GetDesc().Format) + "\n" }.c_str());

        // Create the GPU upload buffer.
        ThrowIfFailed(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(uploadBuffer.GetAddressOf())));

        UpdateSubresources(commandList, defaultBuffer.Get(), uploadBuffer.Get(), 0, 0, static_cast<UINT>(subresources.size()), subresources.data());
        commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

        m_textureBuffer_defaults.push_back(move(defaultBuffer));
        m_textureBuffer_uploads.push_back(move(uploadBuffer));
    }
}

void Scene::BuildTextureBufferView(ID3D12Device* device)
{
    // Describe and create a SRV for the texture.
    for (int i = 0; i < m_DDSFileName.size(); ++i)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = m_textureBuffer_defaults[i]->GetDesc().Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = m_textureBuffer_defaults[i]->GetDesc().MipLevels;

        CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(m_descriptorHeap->GetCPUDescriptorHandleForHeapStart());
        hDescriptor.Offset(1 + i, m_cbvsrvuavDescriptorSize); // 1 + i   1 ǹ̴   constant buffer view  ̴.      

        device->CreateShaderResourceView(m_textureBuffer_defaults[i].Get(), &srvDesc, hDescriptor);
    }
}

UINT Scene::CalcConstantBufferByteSize(UINT byteSize)
{
    return (byteSize + 255) & ~255;
}

void Scene::BuildProjMatrix()
{
    XMMATRIX proj = XMMatrixPerspectiveFovLH(XM_PI * 0.25f, m_viewport.Width / m_viewport.Height, 1.0f, 1000.0f);
    XMStoreFloat4x4(&m_proj, proj);
}

void Scene::LoadMeshAnimationTexture()
{
    m_resourceManager = make_unique<ResourceManager>();
    m_resourceManager->CreatePlane("Plane", 500);
    m_resourceManager->CreateTerrain("HeightMap.raw", 200, 10, 80);
    m_resourceManager->LoadFbx("1P(boy-idle).fbx", false, false);
    m_resourceManager->LoadFbx("1P(boy-jump).fbx", true, false);
    m_resourceManager->LoadFbx("boy_run_fix.fbx", true, false);
    m_resourceManager->LoadFbx("boy_walk_fix.fbx", true, false);
    m_resourceManager->LoadFbx("boy_pickup_fix.fbx", true, false);
    m_resourceManager->LoadFbx("long_tree.fbx", false, true);
    m_resourceManager->LoadFbx("202411_walk_tiger_center.fbx", false, false);

    int i = 0;
    m_DDSFileName.push_back(L"./Textures/boy.dds");
    m_subTextureData.insert({ L"boy", i++ });
    m_DDSFileName.push_back(L"./Textures/bricks3.dds");
    m_subTextureData.insert({ L"bricks3", i++ });
    m_DDSFileName.push_back(L"./Textures/checkboard.dds");
    m_subTextureData.insert({ L"checkboard", i++ });
    m_DDSFileName.push_back(L"./Textures/grass.dds");
    m_subTextureData.insert({ L"grass", i++ });
    m_DDSFileName.push_back(L"./Textures/tile.dds");
    m_subTextureData.insert({ L"tile", i++ });
    m_DDSFileName.push_back(L"./Textures/WireFence.dds");
    m_subTextureData.insert({ L"WireFence", i++ });
    m_DDSFileName.push_back(L"./Textures/god.dds");
    m_subTextureData.insert({ L"god", i++ });
    m_DDSFileName.push_back(L"./Textures/sister.dds");
    m_subTextureData.insert({ L"sister", i++ });
    m_DDSFileName.push_back(L"./Textures/water1.dds");
    m_subTextureData.insert({ L"water1", i++ });
    m_DDSFileName.push_back(L"./Textures/PP_Color_Palette.dds");
    m_subTextureData.insert({ L"PP_Color_Palette", i++ });
    m_DDSFileName.push_back(L"./Textures/tigercolor.dds");
    m_subTextureData.insert({ L"tigercolor", i++ });
    m_DDSFileName.push_back(L"./Textures/stone.dds");
    m_subTextureData.insert({ L"stone", i++ });
    m_DDSFileName.push_back(L"./Textures/normaltree_texture.dds");
    m_subTextureData.insert({ L"normalTree", i++ });
    m_DDSFileName.push_back(L"./Textures/longtree_texture.dds");
    m_subTextureData.insert({ L"longTree", i++ });
    m_DDSFileName.push_back(L"./Textures/rock(smooth).dds");
    m_subTextureData.insert({ L"rock", i++ });
}

void Scene::SetState(ID3D12GraphicsCommandList* commandList)
{
    // Set necessary state.
    commandList->SetPipelineState(m_pipelineState.Get());
    commandList->SetGraphicsRootSignature(m_rootSignature.Get());
    commandList->RSSetViewports(1, &m_viewport);
    commandList->RSSetScissorRects(1, &m_scissorRect);
}

void Scene::SetDescriptorHeaps(ID3D12GraphicsCommandList* commandList)
{
    ID3D12DescriptorHeap* ppHeaps[] = { m_descriptorHeap.Get() };
    commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
}

// Update frame-based values.
void Scene::OnUpdate(GameTimer& gTimer)
{
    // 기존 업데이트 코드
    for (auto& [key, value] : m_objects) {
        visit([&gTimer](auto& arg) {arg.OnUpdate(gTimer); }, value);
    }
    
    // 호랑이 보간 업데이트
    float deltaTime = gTimer.DeltaTime();
    for (auto& [tigerID, interpData] : m_tigerInterpolationData) {
        wstring objectName = L"NetworkTiger_" + std::to_wstring(tigerID);
        if (m_objects.find(objectName) != m_objects.end()) {
            auto& tiger = GetObj<TigerObject>(objectName);
            
            interpData.currentTime += deltaTime;
            float t = min(interpData.currentTime / interpData.interpolationTime, 1.0f);
            
            // 선형 보간 수행
            float newX = std::lerp(interpData.prevPosition.x, interpData.targetPosition.x, t);
            float newY = std::lerp(interpData.prevPosition.y, interpData.targetPosition.y, t);
            float newZ = std::lerp(interpData.prevPosition.z, interpData.targetPosition.z, t);
            float newRotY = std::lerp(interpData.prevRotY, interpData.targetRotY, t);
            
            // 실제 위치와 회전 업데이트
            tiger.GetComponent<Position>().SetXMVECTOR(XMVectorSet(newX, newY, newZ, 1.0f));
            tiger.GetComponent<Rotation>().SetXMVECTOR(XMVectorSet(0.0f, newRotY, 0.0f, 0.0f));
        }
    }
    
    memcpy(static_cast<UINT8*>(m_mappedData) + sizeof(XMMATRIX), &XMMatrixTranspose(XMLoadFloat4x4(&m_proj)), sizeof(XMMATRIX));
}

// Render the scene.
void Scene::OnRender(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    commandList->IASetIndexBuffer(&m_indexBufferView);

    CD3DX12_GPU_DESCRIPTOR_HANDLE hDescriptor(m_descriptorHeap->GetGPUDescriptorHandleForHeapStart());
    commandList->SetGraphicsRootDescriptorTable(0, hDescriptor);

    // 기존 오브젝트 렌더링
    for (auto& [key, value] : m_objects) {
        if (key.find(L"NetworkTiger_") != std::wstring::npos) {
            char buffer[256];
            sprintf_s(buffer, "Rendering tiger object: %ls\n", key.c_str());
            NetworkManager::LogToFile(buffer);
        }
        visit([device, commandList](auto& arg) {arg.OnRender(device, commandList); }, value);
    }
    
}

void Scene::OnResize(UINT width, UINT height)
{
    m_viewport = { CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f) };
    m_scissorRect = { CD3DX12_RECT(0, 0, static_cast<LONG>(width), static_cast<LONG>(height)) };
    BuildProjMatrix();
}

void Scene::OnDestroy()
{
    CD3DX12_RANGE Range(0, CalcConstantBufferByteSize(sizeof(ObjectCB)));
    m_constantBuffer->Unmap(0, &Range);
}

void Scene::OnKeyDown(UINT8 key)
{

}

void Scene::OnKeyUp(UINT8 key)
{
}

void Scene::CheckCollision()
{
    for (auto it1 = m_objects.begin(); it1 != m_objects.end(); ++it1) {
        Object* object1 = visit([](auto& arg)->Object* { return &arg; }, it1->second);
        if (object1->FindComponent<Collider>() == false) continue;
        Collider& collider1 = object1->GetComponent<Collider>();
        for (auto it2 = std::next(it1); it2 != m_objects.end(); ++it2) {
            Object* object2 = visit([](auto& arg)->Object* { return &arg; }, it2->second);
            if (object2->FindComponent<Collider>() == false) continue;
            Collider& collider2 = object2->GetComponent<Collider>();
            if (collider1.mAABB.Intersects(collider2.mAABB)) { // obj1  obj2  浹ߴٸ?
                if (collider1.FindCollisionObj(object2)) { //   Ʈ 浹  ִٸ?
                    CollisionState state = collider1.mCollisionStates.at(object2);
                    if (state == CollisionState::ENTER || state == CollisionState::STAY) {
                        collider1.mCollisionStates[object2] = CollisionState::STAY;
                        OutputDebugStringW((it1->first + L" and " + it2->first + L" collision stay\n").c_str());
                        OutputDebugStringW((L"Collision count: " + to_wstring(collider1.mCollisionStates.size()) + L"\n").c_str());
                    }
                    else {
                        collider1.mCollisionStates[object2] = CollisionState::ENTER;
                    }
                }
                else {
                    collider1.mCollisionStates[object2] = CollisionState::ENTER;
                    OutputDebugStringW((it1->first + L" and " + it2->first + L" collision enter\n").c_str());
                    OutputDebugStringW((L"Collision count: " + to_wstring(collider1.mCollisionStates.size()) + L"\n").c_str());
                }

                if (collider2.FindCollisionObj(object1)) {
                    CollisionState state = collider2.mCollisionStates.at(object1);
                    if (state == CollisionState::ENTER || state == CollisionState::STAY) {
                        collider2.mCollisionStates[object1] = CollisionState::STAY;
                        OutputDebugStringW((it2->first + L" and " + it1->first + L" collision stay\n").c_str());
                        OutputDebugStringW((L"Collision count: " + to_wstring(collider2.mCollisionStates.size()) + L"\n").c_str());
                    }
                    else {
                        collider2.mCollisionStates[object1] = CollisionState::ENTER;
                    }
                }
                else {
                    collider2.mCollisionStates[object1] = CollisionState::ENTER;
                    OutputDebugStringW((it2->first + L" and " + it1->first + L" collision enter\n").c_str());
                    OutputDebugStringW((L"Collision count: " + to_wstring(collider2.mCollisionStates.size()) + L"\n").c_str());
                }
            }
            else { // obj1  obj2 浹 ʾҴٸ
                if (collider1.FindCollisionObj(object2)) {
                    CollisionState state = collider1.mCollisionStates.at(object2);
                    if (state == CollisionState::EXIT) {
                        collider1.mCollisionStates.erase(object2);
                        OutputDebugStringW((it1->first + L" and " + it2->first + L" collision exit\n").c_str());
                        OutputDebugStringW((L"Collision count: " + to_wstring(collider1.mCollisionStates.size()) + L"\n").c_str());
                    }
                    else {
                        collider1.mCollisionStates[object2] = CollisionState::EXIT;
                        OutputDebugStringW((it1->first + L" and " + it2->first + L" collision exit\n").c_str());
                        OutputDebugStringW((L"Collision count: " + to_wstring(collider1.mCollisionStates.size()) + L"\n").c_str());
                    }
                }

                if (collider2.FindCollisionObj(object1)) {
                    CollisionState state = collider2.mCollisionStates.at(object1);
                    if (state == CollisionState::EXIT) {
                        collider2.mCollisionStates.erase(object1);
                        OutputDebugStringW((it2->first + L" and " + it1->first + L" collision exit\n").c_str());
                        OutputDebugStringW((L"Collision count: " + to_wstring(collider2.mCollisionStates.size()) + L"\n").c_str());
                    }
                    else {
                        collider2.mCollisionStates[object1] = CollisionState::EXIT;
                        OutputDebugStringW((it2->first + L" and " + it1->first + L" collision exit\n").c_str());
                        OutputDebugStringW((L"Collision count: " + to_wstring(collider2.mCollisionStates.size()) + L"\n").c_str());
                    }
                }
            }
        }
    }
}

void Scene::LateUpdate(GameTimer& gTimer)
{
    for (auto& [key, value] : m_objects)
    {
        visit([&gTimer](auto& arg) {arg.LateUpdate(gTimer); }, value);
    }
}

std::wstring Scene::GetSceneName() const
{
    return m_name;
}

ResourceManager& Scene::GetResourceManager()
{
    return *(m_resourceManager.get());
}

void* Scene::GetConstantBufferMappedData()
{
    // TODO: ⿡ return  մϴ.
    return m_mappedData;
}

ID3D12DescriptorHeap* Scene::GetDescriptorHeap()
{
    return m_descriptorHeap.Get();
}

void Scene::CreateTigerObject(int tigerID, float x, float y, float z, ID3D12Device* device) {
    NetworkManager::LogToFile("=== CreateTigerObject Start ===");
    NetworkManager::LogToFile("[Debug] Function called with parameters:");
    char buffer[256];
    sprintf_s(buffer, "tigerID: %d, position: (%.2f, %.2f, %.2f)", tigerID, x, y, z);
    NetworkManager::LogToFile(buffer);
    
    if (!device) {
        NetworkManager::LogToFile("[Error] Device is null");
        return;
    }

    wstring objectName = L"NetworkTiger_" + std::to_wstring(tigerID);
    NetworkManager::LogToFile("[Debug] Creating object with name: NetworkTiger_" + std::to_string(tigerID));
    
    // 기존 Tiger가 있다면 제거
    if (m_objects.find(objectName) != m_objects.end()) {
        NetworkManager::LogToFile("[Debug] Found existing tiger with same ID - removing it");
        m_objects.erase(objectName);
    }
    
    try {
        NetworkManager::LogToFile("[Debug] Adding new TigerObject");
        AddObj(objectName, TigerObject(this));
        
        auto& tiger = GetObj<TigerObject>(objectName);
        NetworkManager::LogToFile("[Debug] Successfully created TigerObject");
        
        // 지형 높이 계산
        float terrainHeight = CalculateTerrainHeight(x, z);
        sprintf_s(buffer, "[Debug] Calculated terrain height: %.2f", terrainHeight);
        NetworkManager::LogToFile(buffer);
        
        // 컴포넌트 추가
        NetworkManager::LogToFile("[Debug] Adding components...");
        tiger.AddComponent<Position>(Position{ x, terrainHeight, z, 1.0f, &tiger });
        tiger.AddComponent<Rotation>(Rotation{ 0.0f, 0.0f, 0.0f, 0.0f, &tiger });
        tiger.AddComponent<Scale>(Scale{ 0.2f, &tiger });
        tiger.AddComponent<Velocity>(Velocity{ 0.0f, 0.0f, 0.0f, 0.0f, &tiger });
        tiger.AddComponent<Animation>(Animation{ m_resourceManager->GetAnimationData(), &tiger });
        tiger.AddComponent<Mesh>(Mesh{ m_resourceManager->GetSubMeshData().at("202411_walk_tiger_center.fbx"), &tiger });
        tiger.AddComponent<Texture>(Texture{ m_subTextureData.at(L"tigercolor"), &tiger });
        tiger.AddComponent<Collider>(Collider{ 0.0f, 0.0f, 0.0f, 2.0f, 50.0f, 10.0f, &tiger });
        NetworkManager::LogToFile("[Debug] All components added successfully");
        
        // 상수 버퍼 생성
        NetworkManager::LogToFile("[Debug] Building constant buffer");
        tiger.BuildConstantBuffer(device);
        NetworkManager::LogToFile("[Debug] Constant buffer created successfully");
        
    } catch (const std::exception& e) {
        sprintf_s(buffer, "[Error] Exception during tiger creation: %s", e.what());
        NetworkManager::LogToFile(buffer);
    }
    
    NetworkManager::LogToFile("=== CreateTigerObject End ===");
}

void Scene::UpdateTigerObject(int tigerID, float x, float y, float z, float rotY) {
    NetworkManager::LogToFile("[Tiger] Updating tiger object with ID: " + std::to_string(tigerID));
    wstring objectName = L"NetworkTiger_" + std::to_wstring(tigerID);
    char buffer[256];
    
    if (m_objects.find(objectName) != m_objects.end()) {
        auto& tiger = GetObj<TigerObject>(objectName);
        
        // 현재 위치를 이전 위치로 저장
        XMFLOAT3 currentPos;
        XMStoreFloat3(&currentPos, tiger.GetComponent<Position>().GetXMVECTOR());
        float currentRotY = XMVectorGetY(tiger.GetComponent<Rotation>().GetXMVECTOR());
        
        // 지형 높이 계산
        float terrainHeight = CalculateTerrainHeight(x, z);
        
        // 보간 데이터 업데이트
        TigerInterpolationData& interpData = m_tigerInterpolationData[tigerID];
        interpData.prevPosition = currentPos;
        interpData.targetPosition = XMFLOAT3(x, terrainHeight, z);  // y 대신 terrainHeight 사용
        interpData.prevRotY = currentRotY;
        interpData.targetRotY = rotY;
        interpData.interpolationTime = INTERPOLATION_DURATION;
        interpData.currentTime = 0.0f;
        
        sprintf_s(buffer, "Updated Tiger %d interpolation data from (%.2f, %.2f, %.2f) to (%.2f, %.2f, %.2f)\n", 
            tigerID, currentPos.x, currentPos.y, currentPos.z, x, terrainHeight, z);
        NetworkManager::LogToFile(buffer);
    } else {
        sprintf_s(buffer, "Failed to update Tiger %d: object not found\n", tigerID);
        NetworkManager::LogToFile(buffer);
    }
}

float Scene::CalculateTerrainHeight(float x, float z) {
    ResourceManager& rm = GetResourceManager();
    int width = rm.GetTerrainData().terrainWidth;
    int height = rm.GetTerrainData().terrainHeight;
    int terrainScale = rm.GetTerrainData().terrainScale;
    
    if (x >= 0 && z >= 0 && x <= width * terrainScale && z <= height * terrainScale) {
        vector<Vertex>& vertexBuffer = rm.GetVertexBuffer();
        UINT startVertex = GetObj<TerrainObject>(L"TerrainObject").GetComponent<Mesh>().mSubMeshData.startVertexLocation;

        int indexX = (int)(x / terrainScale);
        int indexZ = (int)(z / terrainScale);

        float leftBottom = vertexBuffer[startVertex + indexZ * width + indexX].position.y;
        float rightBottom = vertexBuffer[startVertex + indexZ * width + indexX + 1].position.y;
        float leftTop = vertexBuffer[startVertex + (indexZ + 1) * width + indexX].position.y;
        float rightTop = vertexBuffer[startVertex + (indexZ + 1) * width + indexX + 1].position.y;

        float offsetX = x / terrainScale - indexX;
        float offsetZ = z / terrainScale - indexZ;

        float lerpXBottom = (1 - offsetX) * leftBottom + offsetX * rightBottom;
        float lerpXTop = (1 - offsetX) * leftTop + offsetX * rightTop;

        return (1 - offsetZ) * lerpXBottom + offsetZ * lerpXTop;
    }
    return 0.0f;
}

void Scene::Initialize() {
    // Device 초기화가 완료되었는지 확인하는 로그 추가
    if (m_device) {
        NetworkManager::LogToFile("[Scene] Device initialized successfully");
    } else {
        NetworkManager::LogToFile("[Scene] Device initialization failed");
    }
}

void Scene::ProcessTigerSpawn(const PacketTigerSpawn* packet) {
    if (!m_device) {
        NetworkManager::LogToFile("[Scene] Queueing tiger spawn - Device not ready");
        // 대기 큐에 추가
        m_pendingTigerSpawns.push_back(*packet);
        return;
    }
    
    // 타이거 생성 시도
    CreateTigerObject(packet->tigerID, packet->x, packet->y, packet->z, m_device);
}

// Device 초기화 완료 시 호출
void Scene::OnDeviceReady() {
    NetworkManager::LogToFile("[Scene] Device is ready, processing pending spawns");
    
    // 대기 중인 타이거 스폰 처리
    for (const auto& spawnPacket : m_pendingTigerSpawns) {
        CreateTigerObject(spawnPacket.tigerID, 
            spawnPacket.x, spawnPacket.y, spawnPacket.z, m_device);
    }
    m_pendingTigerSpawns.clear();
}


