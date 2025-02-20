#include "OtherPlayerManager.h"
#include "GameTimer.h"

OtherPlayerManager* OtherPlayerManager::instance = nullptr;

void OtherPlayerManager::SpawnOtherPlayer(int clientID) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (otherPlayers.find(clientID) != otherPlayers.end()) {
        m_networkManager->LogToFile("[OtherPlayerManager] Player already exists: " + std::to_string(clientID));
        return;
    }

    try {
        auto& player = m_currentScene->GetObj<PlayerObject>(L"OtherPlayer");
        player.SetActive(true);
        otherPlayers[clientID] = &player;
        m_networkManager->LogToFile("[OtherPlayerManager] Spawned player " + std::to_string(clientID));
    }
    catch (const std::exception& e) {
        m_networkManager->LogToFile("[OtherPlayerManager] Failed to spawn player: " + std::string(e.what()));
    }
}

void OtherPlayerManager::UpdateOtherPlayer(int clientID, float x, float y, float z, float rotY) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = otherPlayers.find(clientID);
    if (it == otherPlayers.end()) {
        // 뮤텍스를 해제한 후 SpawnOtherPlayer 호출
        lock.~lock_guard();  // 현재 락 해제
        SpawnOtherPlayer(clientID);
        return;
    }

    auto* player = it->second;
    auto& position = player->GetComponent<Position>();
    position.mFloat4 = XMFLOAT4(x, y, z, 1.0f);
    auto& rotation = player->GetComponent<Rotation>();
    rotation.mFloat4 = XMFLOAT4(0.0f, rotY, 0.0f, 0.0f);
}

// Scene의 Update에서 처리