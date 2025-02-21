#pragma once
#include <unordered_map>
#include <mutex>
#include "Vector3.h"

class Server;  // 전방 선언

class Tiger {
private:
    int m_id;
    Vector3 m_position;
    float m_rotationY;
    float m_speed;

public:
    Tiger(int id, float x, float y, float z)
        : m_id(id)
        , m_position(x, y, z)
        , m_rotationY(0.0f)
        , m_speed(50.0f)
    {}
    
    void SetPosition(float x, float y, float z) { m_position = Vector3(x, y, z); }
    void SetRotationY(float rotY) { m_rotationY = rotY; }
    void Move(float moveX, float moveZ) {
        m_position.x += moveX;
        m_position.z += moveZ;
    }
    
    int GetID() const { return m_id; }
    const Vector3& GetPosition() const { return m_position; }
    float GetRotationY() const { return m_rotationY; }
    float GetSpeed() const { return m_speed; }
};

class TigerManager {
private:
    static TigerManager* instance;
    std::unordered_map<int, Tiger*> tigers;
    std::mutex m_mutex;
    int nextTigerID = 1;
    Server* m_server;

public:
    static TigerManager* GetInstance();
    void SetServer(Server* server) { m_server = server; }
    int SpawnTiger(float x, float y, float z);
    void UpdateTiger(int tigerID, float x, float y, float z, float rotY);
    void RemoveTiger(int tigerID);
    void Update(float deltaTime);
}; 