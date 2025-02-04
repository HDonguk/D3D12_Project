#pragma once
#include <unordered_map>
#include "Object.h"

class OtherPlayerManager {
private:
    static OtherPlayerManager* instance;
    std::unordered_map<int, PlayerObject*> otherPlayers;
    PlayerObject* playerPrefab;

    OtherPlayerManager() {}

public:
    static OtherPlayerManager* GetInstance() {
        if (instance == nullptr) {
            instance = new OtherPlayerManager();
        }
        return instance;
    }

    void Initialize(PlayerObject* prefab) {
        playerPrefab = prefab;
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

    const std::unordered_map<int, PlayerObject*>& GetPlayers() const {
        return otherPlayers;
    }
};

OtherPlayerManager* OtherPlayerManager::instance = nullptr; 