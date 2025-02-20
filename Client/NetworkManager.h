#pragma once
#include "stdafx.h"
#include "Packet.h"
#include "Scene.h"
#include "GameTimer.h"
#include <fstream>
#include <ctime>
#include <mutex>

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
    void LogToFile(const std::string& message);
    void Update(GameTimer& gTimer, Scene* scene);

private:
    static DWORD WINAPI NetworkThread(LPVOID arg);
    void ProcessPacket(char* buffer);

    Scene* m_scene{nullptr};
    SOCKET sock;
    HANDLE m_networkThread;
    bool m_isRunning;
    char m_recvBuffer[1024];
    std::ofstream m_logFile;
    int m_myClientID{0};  // 자신의 클라이언트 ID 저장
    std::mutex m_logMutex;
    float m_updateTimer{0.0f};  // 업데이트 간격 타이머
};