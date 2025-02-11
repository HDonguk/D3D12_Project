#pragma once
#include <unordered_map>
#include "Object.h"
#include "Scene.h"
#include "ResourceManager.h"

class OtherPlayerManager {
private:
    static inline OtherPlayerManager* instance = nullptr;
    std::unordered_map<int, PlayerObject*> otherPlayers;
    PlayerObject* playerPrefab;
    Scene* m_currentScene{nullptr};

    OtherPlayerManager() {}

public:
    static OtherPlayerManager* GetInstance() {
        if (instance == nullptr) {
            instance = new OtherPlayerManager();
        }
        return instance;
    }

    void SetScene(Scene* scene) { m_currentScene = scene; }

    void Initialize(Scene* scene) {
        m_currentScene = scene;
        
        // 프리팹 초기화
        playerPrefab = new PlayerObject(m_currentScene);
        auto& rm = m_currentScene->GetResourceManager();
        
        playerPrefab->AddComponent(Position{ 0.f, 0.f, 0.f, 1.f, playerPrefab });
        playerPrefab->AddComponent(Rotation{ 0.0f, 180.0f, 0.0f, 0.0f, playerPrefab });
        playerPrefab->AddComponent(Scale{ 0.1f, playerPrefab });
        playerPrefab->AddComponent(Mesh{ rm.GetSubMeshData().at("1P(boy-idle).fbx"), playerPrefab });
    }

    void SpawnOtherPlayer(int clientID, float x, float y, float z) {
        if (otherPlayers.find(clientID) != otherPlayers.end()) {
            return;
        }

        PlayerObject* newPlayer = new PlayerObject(*playerPrefab);  // 복사 생성
        newPlayer->GetComponent<Position>().mFloat4 = XMFLOAT4(x, y, z, 1.0f);
        otherPlayers[clientID] = newPlayer;
    }

    void UpdateOtherPlayer(int clientID, float x, float y, float z, float rotY) {
        if (otherPlayers.find(clientID) == otherPlayers.end()) {
            SpawnOtherPlayer(clientID, x, y, z);
            return;
        }

        PlayerObject* player = otherPlayers[clientID];
        player->GetComponent<Position>().mFloat4 = XMFLOAT4(x, y, z, 1.0f);
        player->GetComponent<Rotation>().mFloat4.y = rotY;
    }

    void RemoveOtherPlayer(int clientID) {
        if (otherPlayers.find(clientID) != otherPlayers.end()) {
            delete otherPlayers[clientID];
            otherPlayers.erase(clientID);
        }
    }

    std::unordered_map<int, PlayerObject*>& GetPlayers() {
        return otherPlayers;
    }

    void OnRender(ID3D12Device* device, ID3D12GraphicsCommandList* commandList) {
        for (auto& [id, player] : otherPlayers) {
            if (player) {
                player->OnRender(device, commandList);
            }
        }
    }
}; 