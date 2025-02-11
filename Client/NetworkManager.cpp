#include "stdafx.h"
#include "NetworkManager.h"
#include "OtherPlayerManager.h"
#include <iostream>
#include "ResourceManager.h"

NetworkManager::NetworkManager() : sock(INVALID_SOCKET), m_networkThread(NULL), m_isRunning(false) {
}

NetworkManager::~NetworkManager() {
    Shutdown();
}

bool NetworkManager::Initialize(const char* serverIP, int port, Scene* scene) {
    m_scene = scene;  // Scene 설정
    
    std::cout << "[Client] Connecting to server " << serverIP << ":" << port << std::endl;
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cout << "[Error] WSAStartup failed" << std::endl;
        return false;
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        std::cout << "[Error] Socket creation failed" << std::endl;
        return false;
    }

    SOCKADDR_IN serverAddr = { 0 };
    serverAddr.sin_family = AF_INET;
    inet_pton(AF_INET, serverIP, &serverAddr.sin_addr);
    serverAddr.sin_port = htons(port);

    if (connect(sock, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cout << "[Error] Connection failed" << std::endl;
        return false;
    }

    m_isRunning = true;
    m_networkThread = CreateThread(NULL, 0, NetworkThread, this, 0, NULL);
    if (m_networkThread == NULL) {
        std::cout << "[Error] Failed to create network thread" << std::endl;
        return false;
    }

    std::cout << "[Client] Successfully connected to server" << std::endl;

    // Scene의 PlayerObject 설정과 동일하게 프리팹 생성
    PlayerObject* playerPrefab = new PlayerObject(nullptr);
    
    playerPrefab->AddComponent(Position{ 0.f, 0.f, 0.f, 1.f, playerPrefab });
    playerPrefab->AddComponent(Rotation{ 0.0f, 180.0f, 0.0f, 0.0f, playerPrefab });
    playerPrefab->AddComponent(Scale{ 0.1f, playerPrefab });
    
    if (m_scene) {
        auto& rm = m_scene->GetResourceManager();
        playerPrefab->AddComponent(Mesh{ rm.GetSubMeshData().at("1P(boy-idle).fbx"), playerPrefab });
    }
    
    OtherPlayerManager::GetInstance()->Initialize(m_scene);  // Scene 포인터 전달
    return true;
}

DWORD WINAPI NetworkManager::NetworkThread(LPVOID arg) {
    NetworkManager* network = (NetworkManager*)arg;
    while (network->m_isRunning) {
        int recvBytes = recv(network->sock, network->m_recvBuffer, sizeof(network->m_recvBuffer), 0);
        if (recvBytes <= 0) {
            if (recvBytes == 0) {
                std::cout << "[Disconnect] Server closed connection" << std::endl;
            }
            else {
                std::cout << "[Error] recv failed: " << WSAGetLastError() << std::endl;
            }
            network->m_isRunning = false;
            break;
        }

        PacketHeader* header = (PacketHeader*)network->m_recvBuffer;
        std::cout << "[Receive] Packet type: " << header->type << ", size: " << header->size << std::endl;

        if (header->type == PACKET_PLAYER_UPDATE) {
            PacketPlayerUpdate* pkt = (PacketPlayerUpdate*)network->m_recvBuffer;
            std::cout << "[Update] Other player position: (" << pkt->x << ", " << pkt->y << ", " << pkt->z
                << ") rot: " << pkt->rotY << std::endl;
        }

        network->ProcessPacket(network->m_recvBuffer);
    }
    return 0;
}

void NetworkManager::SendPlayerUpdate(float x, float y, float z, float rotY) {
    if (!m_isRunning) return;

    PacketPlayerUpdate pkt;
    pkt.header.size = sizeof(PacketPlayerUpdate);
    pkt.header.type = PACKET_PLAYER_UPDATE;
    pkt.clientID = 0;  // 서버가 알아서 설정할 것이므로 0으로 초기화
    pkt.x = x;
    pkt.y = y;
    pkt.z = z;
    pkt.rotY = rotY;

    send(sock, (char*)&pkt, sizeof(pkt), 0);
}

void NetworkManager::ProcessPacket(char* buffer) {
    PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);
    
    try {
        switch (header->type) {
            case PACKET_PLAYER_SPAWN: {
                PacketPlayerSpawn* pkt = reinterpret_cast<PacketPlayerSpawn*>(buffer);
                if (OtherPlayerManager::GetInstance()) {
                    OtherPlayerManager::GetInstance()->SpawnOtherPlayer(
                        pkt->playerID, pkt->x, pkt->y, pkt->z);
                }
                break;
            }
            case PACKET_PLAYER_UPDATE: {
                PacketPlayerUpdate* pkt = reinterpret_cast<PacketPlayerUpdate*>(buffer);
                if (OtherPlayerManager::GetInstance()) {
                    OtherPlayerManager::GetInstance()->UpdateOtherPlayer(
                        pkt->clientID, pkt->x, pkt->y, pkt->z, pkt->rotY);
                }
                break;
            }
        }
    }
    catch (const std::exception& e) {
        std::cout << "[Error] ProcessPacket failed: " << e.what() << std::endl;
    }
}

void NetworkManager::Shutdown() {
    m_isRunning = false;
    if (m_networkThread) {
        WaitForSingleObject(m_networkThread, INFINITE);
        CloseHandle(m_networkThread);
        m_networkThread = NULL;
    }
    if (sock != INVALID_SOCKET) {
        closesocket(sock);
        sock = INVALID_SOCKET;
    }
    WSACleanup();
}