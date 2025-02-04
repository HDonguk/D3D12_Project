#pragma once
#include "Scene.h"
#include "OtherPlayerManager.h"

class OtherPlayersScene : public Scene {
public:
    OtherPlayersScene(UINT width, UINT height, wstring name) 
        : Scene(width, height, name) {}

    void OnInit(ID3D12Device* device, ID3D12GraphicsCommandList* commandList) override {
        Scene::OnInit(device, commandList);
        
        // 메인 플레이어를 기반으로 OtherPlayerManager 초기화
        auto& player = GetObj<PlayerObject>(L"PlayerObject");
        OtherPlayerManager::GetInstance()->Initialize(&player);
    }

    void OnUpdate(GameTimer& gTimer) override {
        Scene::OnUpdate(gTimer);
        
        // 다른 플레이어들 업데이트
        auto& otherPlayers = OtherPlayerManager::GetInstance()->GetPlayers();
        for (auto& [id, player] : otherPlayers) {
            player->OnUpdate(gTimer);
        }
    }

    void OnRender(ID3D12Device* device, ID3D12GraphicsCommandList* commandList) override {
        Scene::OnRender(device, commandList);
        
        // 다른 플레이어들 렌더링
        auto& otherPlayers = OtherPlayerManager::GetInstance()->GetPlayers();
        for (auto& [id, player] : otherPlayers) {
            player->OnRender(device, commandList);
        }
    }
}; 