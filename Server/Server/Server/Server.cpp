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

    int targetCount = 0;  // 실제 전송 대상 클라이언트 수
    for (auto& [id, client] : g_clients) {
        if (id != excludeID) targetCount++;
    }

    int sentCount = 0;
    for (auto& [id, client] : g_clients) {
        if (id == excludeID) continue;
        
        int result = send(client.socket, (char*)packet, size, 0);
        if (result == SOCKET_ERROR) {
            int error = WSAGetLastError();
            std::cout << "  -> Failed to send to client " << id << " (Error: " << error << ")" << std::endl;
            
            if (error != WSAEWOULDBLOCK) {
                closesocket(client.socket);
                g_clients.erase(id);
            }
        }
        else {
            std::cout << "  -> Successfully sent to client " << id << std::endl;
            sentCount++;
        }
    }
    std::cout << "  -> Total broadcasts sent: " << sentCount << "/" << targetCount << std::endl;
}

DWORD WINAPI WorkerThread(LPVOID lpParam) {
    while (true) {
        DWORD bytesTransferred;
        ULONG_PTR completionKey;
        OVERLAPPED* pOverlapped;
        
        BOOL result = GetQueuedCompletionStatus(g_hIOCP, &bytesTransferred, &completionKey, &pOverlapped, INFINITE);
        int clientID = (int)completionKey;
        
        if (!pOverlapped) {
            continue;
        }
        
        IOContext* ioContext = CONTAINING_RECORD(pOverlapped, IOContext, overlapped);
        
        if (g_clients.find(clientID) == g_clients.end()) {
            if (ioContext) delete ioContext;
            continue;
        }
        
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
            int error = WSAGetLastError();
            if (error != ERROR_IO_PENDING && error != WSAEWOULDBLOCK) {
                std::cout << "[Error] WSARecv failed with error: " << error << std::endl;
                delete ioContext;
                return 1;  // 에러 발생 시 1 반환
            }
        }
    }
    return 0;  // 정상 종료 시 0 반환
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
        int error = WSAGetLastError();
        if (error != ERROR_IO_PENDING && error != WSAEWOULDBLOCK) {
            std::cout << "[Error] Initial WSARecv failed with error: " << error << std::endl;
            delete ioContext;
            return;
        }
    }
}

void BroadcastNewPlayer(int newClientID, const PacketPlayerSpawn& spawnPacket) {
    std::cout << "[Broadcast] New player ID: " << newClientID << std::endl;
    
    // 새 클라이언트에게 기존 클라이언트 정보 먼저 전송
    for (const auto& [id, client] : g_clients) {
        if (id != newClientID) {
            PacketPlayerSpawn existingClientPacket;
            existingClientPacket.header.type = PACKET_PLAYER_SPAWN;
            existingClientPacket.header.size = sizeof(PacketPlayerSpawn);
            existingClientPacket.playerID = id;
            existingClientPacket.x = client.lastUpdate.x;
            existingClientPacket.y = client.lastUpdate.y;
            existingClientPacket.z = client.lastUpdate.z;
            
            if (send(g_clients[newClientID].socket, (char*)&existingClientPacket, sizeof(existingClientPacket), 0) == SOCKET_ERROR) {
                std::cout << "[오류] 기존 클라이언트 정보 전송 실패. Error: " << WSAGetLastError() << std::endl;
                continue;
            }
            Sleep(10); // 약간의 딜레이 추가
        }
    }
    
    // 그 다음 기존 클라이언트들에게 새 클라이언트 정보 전송
    for (const auto& [id, client] : g_clients) {
        if (id != newClientID) {
            if (send(client.socket, (char*)&spawnPacket, sizeof(spawnPacket), 0) == SOCKET_ERROR) {
                std::cout << "[오류] 새 클라이언트 정보 전송 실패. Error: " << WSAGetLastError() << std::endl;
            }
            Sleep(10); // 약간의 딜레이 추가
        }
    }
}

void ProcessNewClient(SOCKET clientSock) {
    int clientID = g_nextClientID++;
    
    ClientInfo newClient;
    newClient.socket = clientSock;
    newClient.clientID = clientID;
    newClient.lastUpdate = { 0 };
    
    g_clients[clientID] = newClient;
    
    CreateIoCompletionPort((HANDLE)clientSock, g_hIOCP, clientID, 0);
    
    // StartReceive를 먼저 호출
    StartReceive(clientSock, clientID);
    
    // 그 다음 패킷 전송
    PacketPlayerSpawn spawnPacket;
    spawnPacket.header.type = PACKET_PLAYER_SPAWN;
    spawnPacket.header.size = sizeof(PacketPlayerSpawn);
    spawnPacket.playerID = clientID;
    spawnPacket.x = 600.0f + (clientID * 2.0f);
    spawnPacket.y = 0.0f;
    spawnPacket.z = 600.0f;
    
    if (send(clientSock, (char*)&spawnPacket, sizeof(spawnPacket), 0) == SOCKET_ERROR) {
        std::cout << "[오류] 스폰 패킷 전송 실패" << std::endl;
        return;
    }
    
    BroadcastNewPlayer(clientID, spawnPacket);
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