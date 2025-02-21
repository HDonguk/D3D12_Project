#pragma once
#include "Packet.h"
#include "TigerManager.h"
#include <windows.h>
#include <mswsock.h>
#include <unordered_map>

struct ClientInfo {
    SOCKET socket;
    int clientID;
    PacketPlayerUpdate lastUpdate;
};

struct IOContext {
    OVERLAPPED overlapped;
    WSABUF wsaBuf;
    char buffer[1024];
    DWORD flags;
};

class Server {
private:
    HANDLE m_hIOCP;
    std::unordered_map<int, ClientInfo> m_clients;
    int m_nextClientID;
    TigerManager* m_tigerManager;
    bool m_isRunning;
    SOCKET m_listenSocket;
    
    static DWORD WINAPI WorkerThread(LPVOID arg);
    static DWORD WINAPI AcceptThread(LPVOID arg);
    void ProcessPacket(IOContext* ioContext, int clientID);
    void HandleClientDisconnect(int clientID);
    void ProcessNewClient(SOCKET clientSock);
    void StartReceive(SOCKET clientSocket, int clientID);
    void BroadcastNewPlayer(int newClientID);

public:
    Server();
    ~Server();
    bool Initialize(int port);
    void BroadcastPacket(const void* packet, int size, int excludeID = -1);
    void Update();
    void Shutdown();
    
    // TigerManager 연결
    void SetTigerManager(TigerManager* manager) { 
        m_tigerManager = manager;
        m_tigerManager->SetServer(this);
    }
}; 