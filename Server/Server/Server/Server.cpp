#include "Packet.h"
#include <windows.h>
#include <mswsock.h>
#include <iostream>
#include <unordered_map>
#include <format>  // C++20 format 사용
#include "Packet.h"

#pragma comment(lib, "ws2_32.lib")

#define MAX_CLIENTS 2
#define SERVER_PORT 5000

HANDLE g_hIOCP;  // IOCP 핸들 전역 변수 추가
int g_nextClientID = 1;  // 클라이언트 ID 카운터도 필요

struct ClientInfo {
    SOCKET socket;
    int clientID;
    PacketPlayerUpdate lastUpdate;  // 마지막으로 업데이트된 위치 정보를 패킷 형태로 저장
};

// 명시적으로 타입을 지정
std::unordered_map<int, struct ClientInfo> g_clients;  // 클라이언트 ID를 키로 사용하는 맵

// IOCP 작업을 위한 구조체 추가
struct IOContext {
    OVERLAPPED overlapped;
    WSABUF wsaBuf;
    char buffer[1024];
    DWORD flags;
};

void BroadcastPacket(const void* packet, int size, int excludeID = -1) {
    PacketHeader* header = (PacketHeader*)packet;
    std::cout << "\n[Broadcast] Type: " << header->type << ", Size: " << size << std::endl;
    std::cout << "  -> Excluding client ID: " << excludeID << std::endl;

    if (header->type == PACKET_PLAYER_UPDATE) {
        PacketPlayerUpdate* pkt = (PacketPlayerUpdate*)packet;
        std::cout << "  -> Broadcasting position: (" << pkt->x << ", " << pkt->y << ", " << pkt->z
            << "), Rotation: " << pkt->rotY << std::endl;
    }

    int sentCount = 0;
    for (auto& [id, client] : g_clients) {
        if (id == excludeID) continue;
        if (send(client.socket, (char*)packet, size, 0) == SOCKET_ERROR) {
            std::cout << "  -> Failed to send to client " << id << " (Error: " << WSAGetLastError() << ")" << std::endl;
        }
        else {
            std::cout << "  -> Successfully sent to client " << id << std::endl;
            sentCount++;
        }
    }
    std::cout << "  -> Total broadcasts sent: " << sentCount << "/" << g_clients.size() - 1 << std::endl;
}

DWORD WINAPI WorkerThread(LPVOID lpParam) {
    DWORD bytesTransferred;
    ULONG_PTR completionKey;
    OVERLAPPED* pOverlapped;

    std::cout << "[Thread] Worker thread started" << std::endl;

    while (true) {
        BOOL result = GetQueuedCompletionStatus(g_hIOCP, &bytesTransferred, &completionKey, &pOverlapped, INFINITE);
        int clientID = (int)completionKey;
        IOContext* ioContext = CONTAINING_RECORD(pOverlapped, IOContext, overlapped);

        if (!result || bytesTransferred == 0) {
            std::cout << "\n[Disconnect] Client ID " << clientID << " disconnected" << std::endl;
            std::cout << "  -> Active clients remaining: " << g_clients.size() - 1 << std::endl;
            closesocket(g_clients[clientID].socket);
            g_clients[clientID].socket = INVALID_SOCKET;
            g_clients.erase(clientID);
            delete ioContext;
            continue;
        }

        // 패킷 처리
        PacketHeader* header = (PacketHeader*)ioContext->buffer;
        std::cout << "\n[Receive] From client " << clientID << std::endl;
        std::cout << "  -> Packet type: " << header->type << std::endl;
        std::cout << "  -> Packet size: " << header->size << std::endl;

        if (header->type == PACKET_PLAYER_UPDATE) {
            PacketPlayerUpdate* pkt = (PacketPlayerUpdate*)ioContext->buffer;
            pkt->clientID = clientID;  // 클라이언트 ID 설정
            
            // 이전 위치와 비교
            float oldX = g_clients[clientID].lastUpdate.x;
            float oldY = g_clients[clientID].lastUpdate.y;
            float oldZ = g_clients[clientID].lastUpdate.z;
            float oldRotY = g_clients[clientID].lastUpdate.rotY;

            // 새 위치 저장
            g_clients[clientID].lastUpdate = *pkt;

            std::cout << "  -> Client " << clientID << " position update:" << std::endl;
            std::cout << "     Previous: (" << oldX << ", " << oldY << ", " << oldZ << ") rot: " << oldRotY << std::endl;
            std::cout << "     Current:  (" << pkt->x << ", " << pkt->y << ", " << pkt->z << ") rot: " << pkt->rotY << std::endl;

            BroadcastPacket(pkt, sizeof(PacketPlayerUpdate), clientID);
        }

        // 다음 수신 준비
        memset(&ioContext->overlapped, 0, sizeof(OVERLAPPED));
        ioContext->wsaBuf.len = sizeof(ioContext->buffer);
        ioContext->flags = 0;

        DWORD recvBytes;
        if (WSARecv(g_clients[clientID].socket, &ioContext->wsaBuf, 1, &recvBytes,
            &ioContext->flags, &ioContext->overlapped, NULL) == SOCKET_ERROR) {
            if (WSAGetLastError() != ERROR_IO_PENDING) {
                std::cout << "[Error] WSARecv failed: " << WSAGetLastError() << std::endl;
                closesocket(g_clients[clientID].socket);
                g_clients[clientID].socket = INVALID_SOCKET;
                g_clients.erase(clientID);
                delete ioContext;
            }
        }
    }
    return 0;
}

// 클라이언트 연결 시 초기 수신 설정
void StartReceive(SOCKET clientSocket, int clientID) {
    IOContext* ioContext = new IOContext();
    memset(&ioContext->overlapped, 0, sizeof(OVERLAPPED));
    ioContext->wsaBuf.buf = ioContext->buffer;
    ioContext->wsaBuf.len = sizeof(ioContext->buffer);
    ioContext->flags = 0;

    DWORD recvBytes;
    if (WSARecv(clientSocket, &ioContext->wsaBuf, 1, &recvBytes,
        &ioContext->flags, &ioContext->overlapped, NULL) == SOCKET_ERROR) {
        if (WSAGetLastError() != ERROR_IO_PENDING) {
            std::cout << "[Error] Initial WSARecv failed: " << WSAGetLastError() << std::endl;
            delete ioContext;
            return;
        }
    }
}

void ProcessNewClient(SOCKET clientSock) {
    static int nextClientID = 1;  // 정적 변수로 변경
    int clientID = nextClientID++;
    
    ClientInfo newClient;
    newClient.socket = clientSock;
    newClient.clientID = clientID;
    newClient.lastUpdate = { 0 };  // 초기화 추가
    
    g_clients[clientID] = newClient;
    
    CreateIoCompletionPort((HANDLE)clientSock, g_hIOCP, clientID, 0);
    std::cout << "[연결] 새로운 클라이언트 연결됨. ID: " << clientID << std::endl;
    
    // 스폰 패킷에 packetID 추가
    PacketPlayerSpawn spawnPacket;
    spawnPacket.header.type = PACKET_PLAYER_SPAWN;
    spawnPacket.header.size = sizeof(PacketPlayerSpawn);
    spawnPacket.playerID = clientID;
    spawnPacket.packetID = nextClientID;  // packetID 설정
    spawnPacket.x = 600.0f;  // 클라이언트별 위치 조정
    spawnPacket.y = 0.0f;
    spawnPacket.z = 600.0f;
    
    if (send(clientSock, (char*)&spawnPacket, sizeof(spawnPacket), 0) == SOCKET_ERROR) {
        std::cout << "[오류] 스폰 패킷 전송 실패. 클라이언트 ID: " << clientID << std::endl;
        return;
    }
    std::cout << "[스폰] 클라이언트 " << clientID << "에게 스폰 패킷 전송됨" << std::endl;

    Sleep(100);  // 패킷 처리 시간 확보

    // 기존 클라이언트들의 정보를 새 클라이언트에게 전송
    for (const auto& [id, client] : g_clients) {
        if (id != clientID) {
            PacketPlayerSpawn otherSpawn = { 
                { sizeof(PacketPlayerSpawn), PACKET_PLAYER_SPAWN }, 
                id, 
                client.lastUpdate.x,
                client.lastUpdate.y,
                client.lastUpdate.z 
            };
            if (send(clientSock, (char*)&otherSpawn, sizeof(otherSpawn), 0) == SOCKET_ERROR) {
                std::cout << "[오류] 기존 클라이언트 정보 전송 실패. ID: " << id << std::endl;
                continue;
            }
            std::cout << "[스폰] 기존 클라이언트 " << id << " 정보를 새 클라이언트 " 
                      << clientID << "에게 전송" << std::endl;
        }
    }
    
    // 새 클라이언트 정보를 기존 클라이언트들에게 전송
    for (const auto& [id, client] : g_clients) {
        if (id != clientID) {
            if (send(client.socket, (char*)&spawnPacket, sizeof(spawnPacket), 0) == SOCKET_ERROR) {
                std::cout << "[오류] 새 클라이언트 정보 전송 실패. 대상 ID: " << id << std::endl;
                continue;
            }
            std::cout << "[스폰] 새 클라이언트 " << clientID << " 정보를 클라이언트 " 
                      << id << "에게 전송" << std::endl;
        }
    }

    StartReceive(clientSock, clientID);
}

int main() {
    std::cout << "[Server] Starting server on port " << SERVER_PORT << std::endl;

    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    SOCKET listenSock = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
    SOCKADDR_IN serverAddr = { 0 };
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(SERVER_PORT);

    bind(listenSock, (SOCKADDR*)&serverAddr, sizeof(serverAddr));
    listen(listenSock, SOMAXCONN);

    g_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

    for (int i = 0; i < 2; ++i) {
        CreateThread(NULL, 0, WorkerThread, NULL, 0, NULL);
    }

    while (true) {
        SOCKET clientSock = accept(listenSock, NULL, NULL);
        ProcessNewClient(clientSock);
    }

    closesocket(listenSock);
    WSACleanup();
    return 0;
}