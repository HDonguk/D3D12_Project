#pragma once
#include "Scene.h"
#include "OtherPlayerManager.h"

class OtherPlayersScene : public Scene {
public:
    OtherPlayersScene(UINT width, UINT height, wstring name) 
        : Scene(width, height, name) {}

    void OnInit(ID3D12Device* device, ID3D12GraphicsCommandList* commandList) override {
        Scene::OnInit(device, commandList);
        
        // 다른 플레이어들을 위한 리소스 초기화
        auto& rm = GetResourceManager();
        auto& subMeshData = rm.GetSubMeshData();
        
        PlayerObject* playerPrefab = new PlayerObject(nullptr);
        playerPrefab->AddComponent(Position{ 0.f, 0.f, 0.f, 1.f, playerPrefab });
        playerPrefab->AddComponent(Rotation{ 0.0f, 180.0f, 0.0f, 0.0f, playerPrefab });
        playerPrefab->AddComponent(Scale{ 0.1f, playerPrefab });
        playerPrefab->AddComponent(Mesh{ subMeshData.at("1P(boy-idle).fbx"), playerPrefab });
        
        delete playerPrefab;  
        
        OtherPlayerManager::GetInstance()->Initialize(this);
    }

    void OnUpdate(GameTimer& gTimer) override {
        Scene::OnUpdate(gTimer);
    }

    void OnRender(ID3D12Device* device, ID3D12GraphicsCommandList* commandList) override {
        Scene::OnRender(device, commandList);
        
        auto& otherPlayers = OtherPlayerManager::GetInstance()->GetPlayers();
        for (auto& [id, player] : otherPlayers) {
            if (player) {
                player->OnRender(device, commandList);
            }
        }
    }
}; 