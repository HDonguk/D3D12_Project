#include "OtherPlayerManager.h"
#include "GameTimer.h"

OtherPlayerManager* OtherPlayerManager::instance = nullptr;

void OtherPlayerManager::SpawnOtherPlayer(int clientID, float x, float y, float z) {
    std::lock_guard<std::mutex> lock(m_mutex);  // 함수 진입 시 락 획득
    
    if (otherPlayers.find(clientID) != otherPlayers.end()) {
        return;
    }

    PlayerObject* newPlayer = new PlayerObject(m_currentScene);
    auto& rm = m_currentScene->GetResourceManager();
    newPlayer->AddComponent(Position{ x, 0.f, z, 1.f, newPlayer });
    newPlayer->AddComponent(Velocity{ 0.f, 0.f, 0.f, 0.f, newPlayer });
    newPlayer->AddComponent(Rotation{ 0.0f, 180.0f, 0.0f, 0.0f, newPlayer });
    newPlayer->AddComponent(Rotate{ 0.0f, 0.0f, 0.0f, 0.0f, newPlayer });
    newPlayer->AddComponent(Scale{ 0.1f, newPlayer });
    newPlayer->AddComponent(Mesh{ rm.GetSubMeshData().at("1P(boy-idle).fbx"), newPlayer });
    newPlayer->AddComponent(Texture{ m_currentScene->GetSubTextureData().at(L"boy"), newPlayer });
    newPlayer->AddComponent(Animation{ rm.GetAnimationData(), newPlayer });
    newPlayer->AddComponent(Gravity{ 2.f, newPlayer });
    newPlayer->AddComponent(Collider{ 0.f, 0.f, 0.f, 4.f, 50.f, 4.f, newPlayer });

    otherPlayers[clientID] = newPlayer;

    if (m_currentScene) {
        std::wstring objectName = L"OtherPlayer" + std::to_wstring(clientID);
        try {
            m_currentScene->AddObj(objectName, *newPlayer);
            if (m_networkManager) {
                m_networkManager->LogToFile("[Info] Added other player " + std::to_string(clientID) + " to scene");
            }
        }
        catch (const std::exception& e) {
            if (m_networkManager) {
                m_networkManager->LogToFile("[Error] Failed to add other player to scene: " + std::string(e.what()));
            }
            delete newPlayer;  // 실패 시 메모리 해제
            otherPlayers.erase(clientID);
            throw;
        }
    }
}

void OtherPlayerManager::UpdateOtherPlayer(int clientID, float x, float y, float z, float rotY) {
    std::lock_guard<std::mutex> lock(m_mutex);  // 함수 진입 시 락 획득
    
    if (otherPlayers.find(clientID) == otherPlayers.end()) {
        SpawnOtherPlayer(clientID, x, y, z);
        return;
    }

    auto& player = otherPlayers[clientID];
    player->GetComponent<Position>().mFloat4 = XMFLOAT4(x, y, z, 1.0f);
    player->GetComponent<Rotation>().mFloat4 = XMFLOAT4(0.0f, rotY, 0.0f, 0.0f);
}