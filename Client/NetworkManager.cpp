#include "stdafx.h"
#include "NetworkManager.h"
#include "OtherPlayerManager.h"
#include <iostream>

NetworkManager::NetworkManager() : sock(INVALID_SOCKET), m_networkThread(NULL), m_isRunning(false) {
}

NetworkManager::~NetworkManager() {
    Shutdown();
}

bool NetworkManager::Initialize(const char* serverIP, int port) {
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
    return true;
}

DWORD WINAPI NetworkManager::NetworkThread(LPVOID arg) {
    NetworkManager* network = (NetworkManager*)arg;
    while (network->m_isRunning) {
        int recvBytes = recv(network->sock, network->m_recvBuffer, sizeof(network->m_recvBuffer), 0);
        if (recvBytes <= 0) {
            if (recvBytes == 0) {
                std::cout << "[Disconnect] Server closed connection" << std::endl;
            } else {
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
    }
    return 0;
}

void NetworkManager::SendPlayerUpdate(float x, float y, float z, float rotY) {
    if (!m_isRunning) return;

    PacketPlayerUpdate pkt = { {sizeof(PacketPlayerUpdate), PACKET_PLAYER_UPDATE}, x, y, z, rotY };
    std::cout << "[Send] Player position update: (" << x << ", " << y << ", " << z << ") rot: " << rotY << std::endl;
    send(sock, (char*)&pkt, sizeof(pkt), 0);
}

void NetworkManager::ProcessPacket(char* buffer) {
    PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);
    
    switch (header->type) {
        case PACKET_PLAYER_SPAWN: {
            PacketPlayerSpawn* pkt = reinterpret_cast<PacketPlayerSpawn*>(buffer);
            OtherPlayerManager::GetInstance()->SpawnOtherPlayer(
                pkt->playerID, pkt->x, pkt->y, pkt->z);
            break;
        }
        
        case PACKET_PLAYER_UPDATE: {
            PacketPlayerUpdate* pkt = reinterpret_cast<PacketPlayerUpdate*>(buffer);
            OtherPlayerManager::GetInstance()->UpdateOtherPlayer(
                pkt->clientID, pkt->x, pkt->y, pkt->z, pkt->rotY);
            break;
        }
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