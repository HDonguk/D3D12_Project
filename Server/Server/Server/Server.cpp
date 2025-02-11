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
        int clientID = g_nextClientID++;
        g_clients[clientID] = { clientSock, 0, { 0, 0, 0, 0 } };
        CreateIoCompletionPort((HANDLE)clientSock, g_hIOCP, clientID, 0);

        std::cout << "[Connect] New client connected. ID: " << clientID << std::endl;

        PacketPlayerSpawn spawnPacket = { { sizeof(PacketPlayerSpawn), PACKET_PLAYER_SPAWN }, clientID, 0, 0, 0 };
        send(clientSock, (char*)&spawnPacket, sizeof(spawnPacket), 0);
        std::cout << "[Spawn] Sent spawn packet to client ID: " << clientID << std::endl;

        // 클라이언트의 수신 시작
        StartReceive(clientSock, clientID);
    }

    closesocket(listenSock);
    WSACleanup();
    return 0;
}