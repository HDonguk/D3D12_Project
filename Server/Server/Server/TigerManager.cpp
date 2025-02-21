#include <iostream>
#include "TigerManager.h"
#include "Packet.h"
#include <random>
#include <algorithm>
#include "Server.h"  // BroadcastPacket 함수 선언을 위해


TigerManager* TigerManager::instance = nullptr;

TigerManager* TigerManager::GetInstance() {
    if (instance == nullptr) {
        instance = new TigerManager();
    }
    return instance;
}

void TigerManager::SpawnTiger(float x, float y, float z) {
    std::lock_guard<std::mutex> lock(m_mutex);
    int newID = nextTigerID++;
    tigers[newID] = new Tiger(newID, x, y, z);
    
    TigerSpawnPacket packet;
    packet.size = sizeof(TigerSpawnPacket);
    packet.type = TIGER_SPAWN;
    packet.tigerID = newID;
    packet.x = x;
    packet.y = y;
    packet.z = z;
    
    m_server->BroadcastPacket(&packet, sizeof(packet));
}

void TigerManager::Update(float deltaTime) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    static float moveTimer = 0.0f;
    moveTimer += deltaTime;
    
    for (auto& [id, tiger] : tigers) {
        // 3초마다 새로운 방향 선택
        if (moveTimer >= 3.0f) {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_real_distribution<float> dis(-1.0f, 1.0f);
            
            float dirX = dis(gen);
            float dirZ = dis(gen);
            
            // 이동 방향에 따른 회전 각도 계산
            tiger->SetRotationY(atan2f(dirZ, dirX));
        }
        
        // 현재 방향으로 이동
        float rotY = tiger->GetRotationY();
        float speed = tiger->GetSpeed();
        float moveX = cosf(rotY) * speed * deltaTime;
        float moveZ = sinf(rotY) * speed * deltaTime;
        
        tiger->Move(moveX, moveZ);
        
        // 경계 체크
        const float BOUNDARY = 1000.0f;
        const Vector3& pos = tiger->GetPosition();
        tiger->SetPosition(
            std::clamp(pos.x, -BOUNDARY, BOUNDARY),
            pos.y,
            std::clamp(pos.z, -BOUNDARY, BOUNDARY)
        );
        
        // 패킷 전송
        TigerUpdatePacket packet;
        packet.size = sizeof(TigerUpdatePacket);
        packet.type = TIGER_UPDATE;
        packet.tigerID = id;
        packet.x = pos.x;
        packet.y = pos.y;
        packet.z = pos.z;
        packet.rotY = rotY;
        
        m_server->BroadcastPacket(&packet, sizeof(packet));
    }
    
    if (moveTimer >= 3.0f) {
        moveTimer = 0.0f;
    }
} 