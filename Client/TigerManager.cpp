#include <mutex>
#include "TigerManager.h"
#include <string>
#include "stdafx.h"
#include "Object.h"
#include "NetworkManager.h"
#include <DirectXMath.h>

using namespace DirectX;

TigerManager* TigerManager::instance = nullptr;

TigerManager* TigerManager::GetInstance() {
    if (instance == nullptr) {
        instance = new TigerManager();
    }
    return instance;
}

void TigerManager::SpawnTiger(int tigerID, float x, float y, float z) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    try {
        wstring objectName = L"TigerObject_" + to_wstring(tigerID);
        m_currentScene->AddObj(objectName, TigerObject{ m_currentScene });
        auto& newTiger = m_currentScene->GetObj<TigerObject>(objectName);
        newTiger.SetActive(true);
        newTiger.SetTigerID(tigerID);
        newTiger.GetComponent<Position>().SetXMVECTOR(XMVectorSet(x, y, z, 1.0f));
        tigers[tigerID] = &newTiger;
    }
    catch (const std::exception& e) {
        if (m_networkManager) {
            m_networkManager->LogToFile("[TigerManager] Failed to spawn tiger: " + std::string(e.what()));
        }
    }
}

void TigerManager::UpdateTiger(int tigerID, float x, float y, float z, float rotY) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = tigers.find(tigerID);
    if (it == tigers.end()) {
        SpawnTiger(tigerID, x, y, z);
        return;
    }
    
    it->second->UpdateFromNetwork(x, y, z, rotY);
}

void TigerManager::RemoveTiger(int tigerID) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = tigers.find(tigerID);
    if (it != tigers.end()) {
        it->second->SetActive(false);
        tigers.erase(it);
    }
} 