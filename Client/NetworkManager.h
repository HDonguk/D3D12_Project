#pragma once
#include "stdafx.h"
#include "Packet.h"
#include "Scene.h"
#include "GameTimer.h"
#include <fstream>
#include <ctime>
#include <mutex>
#include <unordered_map>

class OtherPlayerManager;
class Scene;

class NetworkManager {
public:
    NetworkManager();
    ~NetworkManager();
    bool Initialize(const char* serverIP, int port, Scene* scene);
    void SetScene(Scene* scene) { m_scene = scene; }
    void SendPlayerUpdate(float x, float y, float z, float rotY);
    void Shutdown();
    bool IsRunning() const { return m_isRunning; }
    static void LogToFile(const std::string& message);
    void Update(GameTimer& gTimer, Scene* scene);

private:
    static DWORD WINAPI NetworkThread(LPVOID arg);
    void ProcessPacket(char* buffer);

    struct TigerInfo {
        int tigerID;
        float x, y, z;
        float rotY;
    };
    std::unordered_map<int, TigerInfo> m_tigers;  // 타이거 정보 저장

    Scene* m_scene{nullptr};
    SOCKET sock;
    HANDLE m_networkThread;
    bool m_isRunning;
    char m_recvBuffer[1024];
    static std::ofstream m_logFile;
    static std::mutex m_logMutex;
    int m_myClientID{0};  // 자신의 클라이언트 ID 저장
    float m_updateTimer{0.0f};  // 업데이트 간격 타이머
};