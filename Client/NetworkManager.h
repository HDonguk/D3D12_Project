#pragma once
#include "stdafx.h"
#include "Packet.h"
#include "Scene.h"  // Scene 헤더 추가

class OtherPlayerManager;  // 전방 선언 추가
class Scene;  // 전방 선언 추가

class NetworkManager {
public:
    NetworkManager();
    ~NetworkManager();
    bool Initialize(const char* serverIP, int port, Scene* scene);  // Scene 매개변수 추가
    void SetScene(Scene* scene) { m_scene = scene; }  // Scene 설정 함수 추가
    void SendPlayerUpdate(float x, float y, float z, float rotY);
    void Shutdown();
    bool IsRunning() const { return m_isRunning; }

private:
    static DWORD WINAPI NetworkThread(LPVOID arg);
    void ProcessPacket(char* buffer);

    Scene* m_scene{nullptr};  // Scene 포인터 멤버 추가
    SOCKET sock;
    HANDLE m_networkThread;
    bool m_isRunning;
    char m_recvBuffer[1024];
};