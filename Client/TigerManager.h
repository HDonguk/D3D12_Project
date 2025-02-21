#pragma once
#include <unordered_map>
#include "Object.h"
#include "Scene.h"
#include "ResourceManager.h"
#include "GameTimer.h"
#include "NetworkManager.h"
#include <mutex>

class TigerManager {
private:
    static TigerManager* instance;
    Scene* m_currentScene{nullptr};
    NetworkManager* m_networkManager{nullptr};
    std::unordered_map<int, TigerObject*> tigers;
    std::mutex m_mutex;

    TigerManager() {}

public:
    static TigerManager* GetInstance();
    void SetScene(Scene* scene) { m_currentScene = scene; }
    void SetNetworkManager(NetworkManager* networkManager) { m_networkManager = networkManager; }
    
    void SpawnTiger(int tigerID, float x, float y, float z);
    void UpdateTiger(int tigerID, float x, float y, float z, float rotY);
    void RemoveTiger(int tigerID);
    
    std::unordered_map<int, TigerObject*>& GetTigers() { return tigers; }
};