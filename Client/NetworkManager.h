#pragma once
#include "stdafx.h"
#include "Packet.h"

class OtherPlayerManager;  // 전방 선언 추가

class NetworkManager {
public:
    NetworkManager();
    ~NetworkManager();
    bool Initialize(const char* serverIP, int port);
    void SendPlayerUpdate(float x, float y, float z, float rotY);
    void Shutdown();
    bool IsRunning() const { return m_isRunning; }

private:
    static DWORD WINAPI NetworkThread(LPVOID arg);
    void ProcessPacket(char* buffer);

    SOCKET sock;
    HANDLE m_networkThread;
    bool m_isRunning;
    char m_recvBuffer[1024];
};