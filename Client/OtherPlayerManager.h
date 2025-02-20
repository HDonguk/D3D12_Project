#pragma once
#include <unordered_map>
#include "Object.h"
#include "Scene.h"
#include "ResourceManager.h"
#include "GameTimer.h"
#include "NetworkManager.h"
#include <mutex>

class OtherPlayerManager {
private:
    static OtherPlayerManager* instance;
    Scene* m_currentScene{nullptr};
    NetworkManager* m_networkManager{nullptr};
    std::unordered_map<int, PlayerObject*> otherPlayers;
    PlayerObject* playerPrefab;
    std::mutex m_mutex;  // 스레드 안전성을 위한 뮤텍스 추가

    struct PendingSpawn {
        int clientID;
        PlayerObject* player;
    };
    std::vector<PendingSpawn> m_pendingSpawns;

    OtherPlayerManager() {}

public:
    static OtherPlayerManager* GetInstance() {
        if (instance == nullptr) {
            instance = new OtherPlayerManager();
        }
        return instance;
    }

    void SetScene(Scene* scene) { m_currentScene = scene; }

    void SetNetworkManager(NetworkManager* networkManager) { m_networkManager = networkManager; }

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

    void SpawnOtherPlayer(int clientID, float x, float y, float z);

    void UpdateOtherPlayer(int clientID, float x, float y, float z, float rotY);

    void RemoveOtherPlayer(int clientID) {
        if (otherPlayers.find(clientID) != otherPlayers.end()) {
            delete otherPlayers[clientID];
            otherPlayers.erase(clientID);
        }
    }

    std::unordered_map<int, PlayerObject*>& GetPlayers() {
        return otherPlayers;
    }

    void Update(GameTimer& timer) {
        // 대기 중인 스폰 처리만 남김
        for (const auto& spawn : m_pendingSpawns) {
            std::wstring objectName = L"OtherPlayer" + std::to_wstring(spawn.clientID);
            m_currentScene->AddObj(objectName, *spawn.player);
        }
        m_pendingSpawns.clear();
    }

    void Render(ID3D12Device* device, ID3D12GraphicsCommandList* commandList) {
        for (auto& [id, player] : otherPlayers) {
            if (player) {
                player->OnRender(device, commandList);
            }
        }
    }
}; 