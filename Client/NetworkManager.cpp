#include "stdafx.h"
#include "NetworkManager.h"
#include "OtherPlayerManager.h"
#include <iostream>
#include "ResourceManager.h"
#include <ctime>
#include <cstdio>
#include <mutex>

NetworkManager::NetworkManager() : sock(INVALID_SOCKET), m_networkThread(NULL), m_isRunning(false), m_myClientID(0) {
    // 로그 파일 생성 (현재 시간을 파일명에 포함)
    time_t now = time(0);
    tm ltm;
    localtime_s(&ltm, &now);
    char filename[100];
    sprintf_s(filename, "network_log_%d%02d%02d_%02d%02d%02d.txt",
        ltm.tm_year + 1900, ltm.tm_mon + 1, ltm.tm_mday,
        ltm.tm_hour, ltm.tm_min, ltm.tm_sec);
    
    m_logFile.open(filename);
    if (m_logFile.is_open()) {
        LogToFile("NetworkManager initialized");
    }
}

NetworkManager::~NetworkManager() {
    Shutdown();
}

bool NetworkManager::Initialize(const char* serverIP, int port, Scene* scene) {
    if (!scene) {
        LogToFile("[Error] Scene is null");
        return false;
    }
    m_scene = scene;
    
    try {
        auto& player = m_scene->GetObj<PlayerObject>(L"PlayerObject");
        LogToFile("[Info] Found PlayerObject in scene");
    }
    catch (const std::exception& e) {
        LogToFile("[Error] Failed to find PlayerObject: " + std::string(e.what()));
        return false;
    }
    
    std::string logMsg = "[Client] Connecting to server " + std::string(serverIP) + ":" + std::to_string(port);
    LogToFile(logMsg);
    
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        LogToFile("[Error] WSAStartup failed");
        return false;
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        LogToFile("[Error] Socket creation failed");
        return false;
    }

    SOCKADDR_IN serverAddr = { 0 };
    serverAddr.sin_family = AF_INET;
    inet_pton(AF_INET, serverIP, &serverAddr.sin_addr);
    serverAddr.sin_port = htons(port);

    if (connect(sock, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        LogToFile("[Error] Connection failed");
        return false;
    }

    // recv 버퍼 초기화
    memset(m_recvBuffer, 0, sizeof(m_recvBuffer));

    m_isRunning = true;
    m_networkThread = CreateThread(NULL, 0, NetworkThread, this, 0, NULL);
    if (m_networkThread == NULL) {
        std::cout << "[Error] Failed to create network thread" << std::endl;
        return false;
    }

    LogToFile("[Client] Successfully connected to server");

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
    delete playerPrefab;  // 메모리 해제 추가
    return true;
}

DWORD WINAPI NetworkManager::NetworkThread(LPVOID arg) {
    NetworkManager* network = (NetworkManager*)arg;
    network->LogToFile("[Thread] Network thread started");
    
    while (network->m_isRunning) {
        int recvBytes = recv(network->sock, network->m_recvBuffer, sizeof(network->m_recvBuffer), 0);
        network->LogToFile("[Receive] Received " + std::to_string(recvBytes) + " bytes");
        
        if (recvBytes <= 0) {
            if (recvBytes == 0) {
                std::cout << "[Disconnect] Server closed connection" << std::endl;
                network->LogToFile("[Disconnect] Server closed connection");
            }
            else {
                std::string errorMsg = "[Error] recv failed: " + std::to_string(WSAGetLastError());
                std::cout << errorMsg << std::endl;
                network->LogToFile(errorMsg);
            }
            network->m_isRunning = false;
            break;
        }

        PacketHeader* header = (PacketHeader*)network->m_recvBuffer;
        std::string logMsg = "[Receive] Packet type: " + std::to_string(header->type) + ", size: " + std::to_string(header->size);
        network->LogToFile(logMsg);

        if (header->type == PACKET_PLAYER_UPDATE) {
            PacketPlayerUpdate* pkt = (PacketPlayerUpdate*)network->m_recvBuffer;
            std::cout << "[Update] Other player position: (" << pkt->x << ", " << pkt->y << ", " << pkt->z
                << ") rot: " << pkt->rotY << std::endl;
        }

        network->ProcessPacket(network->m_recvBuffer);
    }
    
    network->LogToFile("[Thread] Network thread ended");
    return 0;
}

void NetworkManager::SendPlayerUpdate(float x, float y, float z, float rotY) {
    if (!m_isRunning) return;

    try {
        PacketPlayerUpdate pkt;
        pkt.header.size = sizeof(PacketPlayerUpdate);
        pkt.header.type = PACKET_PLAYER_UPDATE;
        pkt.clientID = m_myClientID;
        pkt.x = x;
        pkt.y = y;
        pkt.z = z;
        pkt.rotY = rotY;

        // 로그 기록 실패가 전체 동작에 영향을 주지 않도록 try-catch로 분리
        try {
            LogToFile("[Send] Player Update: (" + std::to_string(x) + ", " + 
                      std::to_string(y) + ", " + std::to_string(z) + 
                      ") rot: " + std::to_string(rotY));
        } catch (...) {}
        
        int sendResult = send(sock, (char*)&pkt, sizeof(pkt), 0);
        if (sendResult == SOCKET_ERROR) {
            int error = WSAGetLastError();
            try {
                LogToFile("[Error] Failed to send update: " + std::to_string(error));
            } catch (...) {}
            
            if (error != WSAEWOULDBLOCK) {  // 일시적인 에러가 아닐 경우에만 종료
                m_isRunning = false;
            }
        }
    }
    catch (const std::exception& e) {
        try {
            LogToFile("[Error] SendPlayerUpdate failed: " + std::string(e.what()));
        } catch (...) {}
        m_isRunning = false;
    }
}

void NetworkManager::ProcessPacket(char* buffer) {
    PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);
    
    try {
        // 패킷 크기 검증 수정
        if (header->size > sizeof(m_recvBuffer)) {
            LogToFile("[Error] Packet size too large: " + std::to_string(header->size));
            return;
        }

        if (!OtherPlayerManager::GetInstance()) {
            LogToFile("[Error] OtherPlayerManager instance is null");
            return;
        }
        
        if (!m_scene) {
            LogToFile("[Error] Scene is null");
            return;
        }

        std::string logMsg = "[Client] Processing packet type: " + std::to_string(header->type) + 
                            ", size: " + std::to_string(header->size) + 
                            ", expected size: " + std::to_string(sizeof(PacketHeader));
        LogToFile(logMsg);
        
        switch (header->type) {
            case PACKET_PLAYER_SPAWN: {
                PacketPlayerSpawn* pkt = reinterpret_cast<PacketPlayerSpawn*>(buffer);
                LogToFile("[Spawn] Processing spawn packet for ID: " + std::to_string(pkt->playerID) + 
                         " at position: (" + std::to_string(pkt->x) + ", " + 
                         std::to_string(pkt->y) + ", " + std::to_string(pkt->z) + ")");
                
                if (m_myClientID == 0) {
                    m_myClientID = pkt->playerID;
                    LogToFile("[Info] Set my client ID to: " + std::to_string(m_myClientID));
                    break;
                }
                
                if (pkt->playerID != m_myClientID) {
                    OtherPlayerManager::GetInstance()->SpawnOtherPlayer(
                        pkt->playerID, pkt->x, pkt->y, pkt->z);
                }
                break;
            }
            case PACKET_PLAYER_UPDATE: {
                PacketPlayerUpdate* pkt = reinterpret_cast<PacketPlayerUpdate*>(buffer);
                if (pkt->clientID != m_myClientID) {
                    try {
                        OtherPlayerManager::GetInstance()->UpdateOtherPlayer(
                            pkt->clientID, pkt->x, pkt->y, pkt->z, pkt->rotY);
                    } catch (const std::exception& e) {
                        LogToFile("[Error] Failed to update other player: " + std::string(e.what()));
                    }
                }
                break;
            }
            default:
                std::cout << "[Client] Unknown packet type: " << header->type << std::endl;
                break;
        }
    }
    catch (const std::exception& e) {
        LogToFile("[Error] ProcessPacket failed: " + std::string(e.what()));
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

void NetworkManager::LogToFile(const std::string& message) {
    std::lock_guard<std::mutex> lock(m_logMutex);
    try {
        if (!m_logFile.is_open()) return;
        
        time_t now = time(0);
        tm ltm;
        localtime_s(&ltm, &now);
        char timestamp[50];
        sprintf_s(timestamp, "[%02d:%02d:%02d] ", ltm.tm_hour, ltm.tm_min, ltm.tm_sec);
        
        m_logFile << timestamp << message << std::endl;
    }
    catch (...) {
        // 로그 실패는 무시
    }
}