#include "Packet.h"
#include <windows.h>
#include <mswsock.h>
#include <iostream>
#include <unordered_map>
#include <format>  // C++20 format 사용
#include "TigerManager.h"
#include "Server.h"

#pragma comment(lib, "ws2_32.lib")

#define MAX_CLIENTS 2
#define SERVER_PORT 5000
#define MAX_PACKET_SIZE 1024  // IOContext의 buffer 크기와 동일하게 설정

Server::Server()
    : m_hIOCP(NULL)
    , m_nextClientID(1)
    , m_tigerManager(nullptr)
    , m_isRunning(false)
    , m_listenSocket(INVALID_SOCKET)
{
}

Server::~Server() {
    Shutdown();
}

bool Server::Initialize(int port) {
    // WSA 초기화
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cout << "[Error] WSAStartup failed" << std::endl;
        return false;
    }

    // IOCP 생성
    m_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (m_hIOCP == NULL) {
        std::cout << "[Error] CreateIoCompletionPort failed" << std::endl;
        return false;
    }

    // 워커 스레드 생성
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    for (DWORD i = 0; i < sysInfo.dwNumberOfProcessors * 2; i++) {
        CreateThread(NULL, 0, WorkerThread, this, 0, NULL);
    }

    // 리스닝 소켓 생성 및 바인딩
    m_listenSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (m_listenSocket == INVALID_SOCKET) {
        std::cout << "[Error] Failed to create listen socket" << std::endl;
        return false;
    }

    SOCKADDR_IN serverAddr = { 0 };
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(port);

    if (bind(m_listenSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cout << "[Error] Failed to bind socket" << std::endl;
        return false;
    }

    if (listen(m_listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cout << "[Error] Failed to listen" << std::endl;
        return false;
    }

    m_isRunning = true;
    CreateThread(NULL, 0, AcceptThread, this, 0, NULL);
    
    return true;
}

DWORD WINAPI Server::WorkerThread(LPVOID arg) {
    Server* server = (Server*)arg;
    while (server->m_isRunning) {
        DWORD bytesTransferred;
        ULONG_PTR completionKey;
        OVERLAPPED* overlapped;
        
        BOOL result = GetQueuedCompletionStatus(
            server->m_hIOCP,
            &bytesTransferred,
            &completionKey,
            &overlapped,
            INFINITE
        );

        if (!overlapped) {
            continue;
        }
        
        IOContext* ioContext = CONTAINING_RECORD(overlapped, IOContext, overlapped);
        
        if (server->m_clients.find(completionKey) == server->m_clients.end()) {
            if (ioContext) delete ioContext;
            continue;
        }
        
        if (!result || bytesTransferred == 0) {
            std::cout << "\n[Disconnect] Client ID " << completionKey << " disconnected" << std::endl;
            closesocket(server->m_clients[completionKey].socket);
            server->m_clients.erase(completionKey);
            std::cout << "  -> Active clients remaining: " << server->m_clients.size() << std::endl;
            delete ioContext;
            continue;
        }

        if (result) {
            std::cout << "[WorkerThread] Received data - Bytes: " << bytesTransferred 
                      << ", ClientID: " << completionKey << std::endl;
        }

        // 패킷 처리
        PacketHeader* header = (PacketHeader*)ioContext->buffer;
        if (header->size == 0 || header->size > MAX_PACKET_SIZE || header->type <= 0) {
            std::cout << "[Error] Invalid packet header - Size: " << header->size 
                      << ", Type: " << header->type << std::endl;
            continue;
        }

        if (bytesTransferred < sizeof(PacketHeader)) {
            std::cout << "[Error] Invalid packet size (smaller than header)" << std::endl;
            continue;
        }

        // 완전한 패킷을 수신했는지 확인
        if (bytesTransferred < header->size) {
            std::cout << "[Error] Incomplete packet received" << std::endl;
            continue;
        }

        std::cout << "\n[Receive] From client " << completionKey << std::endl;
        std::cout << "  -> Packet type: " << header->type << std::endl;
        std::cout << "  -> Packet size: " << header->size << std::endl;

        server->ProcessPacket(ioContext, completionKey);

        // 다음 수신 준비
        memset(ioContext->buffer, 0, sizeof(ioContext->buffer));  // 버퍼 초기화 추가
        memset(&ioContext->overlapped, 0, sizeof(OVERLAPPED));
        ioContext->wsaBuf.len = sizeof(ioContext->buffer);
        ioContext->flags = 0;

        DWORD recvBytes;
        if (WSARecv(server->m_clients[completionKey].socket, &ioContext->wsaBuf, 1, &recvBytes,
            &ioContext->flags, &ioContext->overlapped, NULL) == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error != ERROR_IO_PENDING && error != WSAEWOULDBLOCK) {
                std::cout << "[Error] WSARecv failed with error: " << error << std::endl;
                delete ioContext;
                continue;
            }
        }
    }
    return 0;
}

DWORD WINAPI Server::AcceptThread(LPVOID arg) {
    Server* server = (Server*)arg;
    while (server->m_isRunning) {
        SOCKET clientSock = accept(server->m_listenSocket, NULL, NULL);
        if (clientSock != INVALID_SOCKET) {
            server->ProcessNewClient(clientSock);
        }
    }
    return 0;
}

void Server::ProcessPacket(IOContext* ioContext, int clientID) {
    PacketHeader* header = (PacketHeader*)ioContext->buffer;
    
    switch (header->type) {
        case PACKET_PLAYER_UPDATE: {
            if (header->size != sizeof(PacketPlayerUpdate)) {
                std::cout << "[Error] Invalid PLAYER_UPDATE packet size" << std::endl;
                break;
            }
            PacketPlayerUpdate* pkt = (PacketPlayerUpdate*)ioContext->buffer;
            pkt->clientID = clientID;
            m_clients[clientID].lastUpdate = *pkt;
            BroadcastPacket(pkt, sizeof(PacketPlayerUpdate), clientID);
            break;
        }
        case PACKET_PLAYER_SPAWN: {
            if (header->size != sizeof(PacketPlayerSpawn)) {
                std::cout << "[Error] Invalid PLAYER_SPAWN packet size" << std::endl;
                break;
            }
            // 스폰 패킷 처리 로직 추가
            PacketPlayerSpawn* pkt = (PacketPlayerSpawn*)ioContext->buffer;
            BroadcastPacket(pkt, sizeof(PacketPlayerSpawn), clientID);
            break;
        }
        case TIGER_SPAWN: {
            if (header->size != sizeof(TigerSpawnPacket)) {
                std::cout << "[Error] Invalid TIGER_SPAWN packet size" << std::endl;
                break;
            }
            TigerSpawnPacket* pkt = (TigerSpawnPacket*)ioContext->buffer;
            m_tigerManager->SpawnTiger(pkt->x, pkt->y, pkt->z);  // TigerManager에 등록
            BroadcastPacket(pkt, sizeof(TigerSpawnPacket));
            break;
        }
        case TIGER_UPDATE: {
            if (header->size != sizeof(TigerUpdatePacket)) {
                std::cout << "[Error] Invalid TIGER_UPDATE packet size" << std::endl;
                break;
            }
            TigerUpdatePacket* pkt = (TigerUpdatePacket*)ioContext->buffer;
            BroadcastPacket(pkt, sizeof(TigerUpdatePacket));
            break;
        }
        case TIGER_REMOVE: {
            if (header->size != sizeof(PacketHeader) + sizeof(int)) {
                std::cout << "[Error] Invalid TIGER_REMOVE packet size" << std::endl;
                break;
            }
            BroadcastPacket(ioContext->buffer, header->size);
            break;
        }
        default:
            std::cout << "  -> Unknown packet type" << std::endl;
            break;
    }
}

void Server::BroadcastPacket(const void* packet, int size, int excludeID) {
    PacketHeader* header = (PacketHeader*)packet;
    if (size < sizeof(PacketHeader) || size != header->size) {
        std::cout << "[Error] Invalid packet size in broadcast" << std::endl;
        return;
    }

    std::cout << "\n[Broadcast] Type: " << header->type << ", Size: " << size << std::endl;
    std::cout << "  -> Excluding client ID: " << excludeID << std::endl;

    if (header->type == PACKET_PLAYER_UPDATE) {
        PacketPlayerUpdate* pkt = (PacketPlayerUpdate*)packet;
        std::cout << "  -> Broadcasting position: (" << pkt->x << ", " << pkt->y << ", " << pkt->z
            << "), Rotation: " << pkt->rotY << std::endl;
    }

    int targetCount = 0;
    for (auto& [id, client] : m_clients) {
        if (id != excludeID) targetCount++;
    }

    int sentCount = 0;
    for (auto& [id, client] : m_clients) {
        if (id == excludeID) continue;
        
        int result = send(client.socket, (char*)packet, size, 0);
        if (result == SOCKET_ERROR) {
            int error = WSAGetLastError();
            std::cout << "  -> Failed to send to client " << id << " (Error: " << error << ")" << std::endl;
        } else {
            std::cout << "  -> Successfully sent to client " << id << std::endl;
            sentCount++;
        }
    }
    std::cout << "  -> Total broadcasts sent: " << sentCount << "/" << targetCount << std::endl;
}

void Server::Shutdown() {
    std::cout << "[Server] Shutting down..." << std::endl;
    m_isRunning = false;
    
    if (m_listenSocket != INVALID_SOCKET) {
        std::cout << "  -> Closing listen socket" << std::endl;
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
    }
    
    if (m_hIOCP) {
        std::cout << "  -> Closing IOCP handle" << std::endl;
        CloseHandle(m_hIOCP);
        m_hIOCP = NULL;
    }
    
    std::cout << "  -> Closing all client connections" << std::endl;
    for (auto& [id, client] : m_clients) {
        closesocket(client.socket);
    }
    m_clients.clear();
    
    WSACleanup();
    std::cout << "[Server] Shutdown complete" << std::endl;
}

void Server::Update() {
    // 현재는 비어있지만 필요한 경우 서버 상태 업데이트 로직 추가
}

void Server::ProcessNewClient(SOCKET clientSock) {
    int clientID = m_nextClientID++;
    std::cout << "\n[ProcessNewClient] New connection accepted" << std::endl;
    std::cout << "  -> Assigning client ID: " << clientID << std::endl;
    
    ClientInfo newClient;
    newClient.socket = clientSock;
    newClient.clientID = clientID;
    newClient.lastUpdate = { 0 };
    
    m_clients[clientID] = newClient;
    CreateIoCompletionPort((HANDLE)clientSock, m_hIOCP, clientID, 0);
    
    // 새 클라이언트에게 자신의 ID 전송
    PacketPlayerSpawn spawnPacket;
    spawnPacket.header.type = PACKET_PLAYER_SPAWN;
    spawnPacket.header.size = sizeof(PacketPlayerSpawn);
    spawnPacket.playerID = clientID;
    
    if (send(clientSock, (char*)&spawnPacket, sizeof(spawnPacket), 0) == SOCKET_ERROR) {
        std::cout << "[Error] Failed to send spawn packet to new client" << std::endl;
        return;
    }
    std::cout << "  -> Sent initial spawn packet to client " << clientID << std::endl;
    
    BroadcastNewPlayer(clientID);
    StartReceive(clientSock, clientID);
}

void Server::BroadcastNewPlayer(int newClientID) {
    std::cout << "\n[BroadcastNewPlayer] New client ID: " << newClientID << std::endl;
    
    // 새 클라이언트에게 기존 클라이언트들의 정보 전송
    for (const auto& [id, client] : m_clients) {
        if (id != newClientID) {
            PacketPlayerSpawn existingClientPacket;
            existingClientPacket.header.type = PACKET_PLAYER_SPAWN;
            existingClientPacket.header.size = sizeof(PacketPlayerSpawn);
            existingClientPacket.playerID = id;
            
            // 새 클라이언트에게 기존 클라이언트 정보 전송
            int result = send(m_clients[newClientID].socket, (char*)&existingClientPacket, sizeof(existingClientPacket), 0);
            if (result == SOCKET_ERROR) {
                std::cout << "[Error] Failed to send existing client info to new client" << std::endl;
            } else {
                std::cout << "  -> Sent existing client " << id << " info to new client " << newClientID << std::endl;
            }
        }
    }
    
    // 기존 클라이언트들에게 새 클라이언트 정보 전송
    PacketPlayerSpawn newClientPacket;
    newClientPacket.header.type = PACKET_PLAYER_SPAWN;
    newClientPacket.header.size = sizeof(PacketPlayerSpawn);
    newClientPacket.playerID = newClientID;
    
    BroadcastPacket(&newClientPacket, sizeof(newClientPacket), newClientID);
}

void Server::StartReceive(SOCKET clientSocket, int clientID) {
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