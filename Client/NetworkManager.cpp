#include "stdafx.h"
#include "NetworkManager.h"
#include "OtherPlayerManager.h"
#include <iostream>
#include "ResourceManager.h"
#include <ctime>
#include <cstdio>
#include <mutex>

NetworkManager::NetworkManager() : sock(INVALID_SOCKET), m_networkThread(NULL), m_isRunning(false), m_myClientID(0) {

    
    try {
        CreateDirectory(L"logs", NULL);
        
        time_t now = time(0);
        tm ltm;
        localtime_s(&ltm, &now);
        char filename[100];
        sprintf_s(filename, "logs/network_log_%d%02d%02d_%02d%02d%02d.txt",
            ltm.tm_year + 1900, ltm.tm_mon + 1, ltm.tm_mday,
            ltm.tm_hour, ltm.tm_min, ltm.tm_sec);
        
        m_logFile.open(filename);
        if (m_logFile.is_open()) {
            LogToFile("NetworkManager initialized");
        }
        else {
            std::cerr << "Failed to open log file: " << filename << std::endl;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to initialize NetworkManager: " << e.what() << std::endl;
    }
    catch (...) {
        std::cerr << "Unknown error in NetworkManager initialization" << std::endl;
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

    return true;
}

DWORD WINAPI NetworkManager::NetworkThread(LPVOID arg) {
    NetworkManager* network = (NetworkManager*)arg;
    network->LogToFile("[Thread] Network thread started");
    
    int errorCount = 0;
    const int MAX_ERRORS = 3;  // 최대 연속 에러 허용 횟수
    const int ERROR_RESET_TIME = 5000;  // 에러 카운트 리셋 시간 (ms)
    DWORD lastErrorTime = GetTickCount();
    
    while (network->m_isRunning) {
        try {
            fd_set readSet;
            FD_ZERO(&readSet);
            FD_SET(network->sock, &readSet);
            
            timeval timeout;
            timeout.tv_sec = 0;
            timeout.tv_usec = 100000;  // 100ms
            
            int selectResult = select(0, &readSet, nullptr, nullptr, &timeout);
            if (selectResult == SOCKET_ERROR) {
                int error = WSAGetLastError();
                network->LogToFile("[Error] Select failed with error: " + std::to_string(error));
                if (error != WSAEWOULDBLOCK) {
                    if (++errorCount >= MAX_ERRORS) {
                        network->LogToFile("[Critical] Too many select errors, closing connection");
                        break;
                    }
                    Sleep(1000);  // 에러 발생 시 잠시 대기
                }
                continue;
            }
            
            if (selectResult == 0) continue;  // 타임아웃

            int recvBytes = recv(network->sock, network->m_recvBuffer, sizeof(network->m_recvBuffer), 0);
            if (recvBytes <= 0) {
                int error = WSAGetLastError();
                if (error != WSAEWOULDBLOCK) {
                    network->LogToFile("[Error] Receive failed: " + std::to_string(error));
                    if (++errorCount >= MAX_ERRORS) {
                        network->LogToFile("[Critical] Too many receive errors, closing connection");
                        break;
                    }
                    Sleep(1000);
                }
                continue;
            }

            if (recvBytes > sizeof(network->m_recvBuffer)) {
                network->LogToFile("[Error] Receive buffer overflow: " + std::to_string(recvBytes) + " bytes");
                return 1;
            }

            // 패킷 헤더 검증
            PacketHeader* header = (PacketHeader*)network->m_recvBuffer;
            if (recvBytes < sizeof(PacketHeader) || recvBytes != header->size) {
                network->LogToFile("[Error] Invalid packet size or header");
                if (++errorCount >= MAX_ERRORS) {
                    network->LogToFile("[Critical] Too many invalid packets, closing connection");
                    break;
                }
                continue;
            }

            // 성공적인 패킷 수신 시 에러 카운트 관리
            DWORD currentTime = GetTickCount();
            if (currentTime - lastErrorTime > ERROR_RESET_TIME) {
                errorCount = 0;  // 일정 시간 동안 에러가 없었다면 카운트 리셋
                lastErrorTime = currentTime;
            }

            network->ProcessPacket(network->m_recvBuffer);
        }
        catch (const std::exception& e) {
            network->LogToFile("[Error] Exception in network thread: " + std::string(e.what()));
            if (++errorCount >= MAX_ERRORS) {
                network->LogToFile("[Critical] Too many exceptions, closing connection");
                break;
            }
            Sleep(1000);
        }
    }
    
    network->LogToFile("[Thread] Network thread ended normally");
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

        int sendResult = send(sock, (char*)&pkt, sizeof(pkt), 0);
        if (sendResult == SOCKET_ERROR) {
            LogToFile("[Error] Failed to send update packet");
        }
    }
    catch (const std::exception& e) {
        LogToFile("[Error] Exception in SendPlayerUpdate: " + std::string(e.what()));
    }
}

void NetworkManager::ProcessPacket(char* buffer) {
    PacketHeader* header = (PacketHeader*)buffer;
    LogToFile("[ProcessPacket] Starting to process packet type: " + std::to_string(header->type));

    try {
        switch (header->type) {
            case PACKET_PLAYER_SPAWN: {
                PacketPlayerSpawn* pkt = (PacketPlayerSpawn*)buffer;
                LogToFile("[Spawn] Processing spawn packet for ID: " + std::to_string(pkt->playerID));

                if (m_myClientID == 0) {
                    m_myClientID = pkt->playerID;
                    LogToFile("[Spawn] Set my client ID to: " + std::to_string(m_myClientID));
                }
                else if (pkt->playerID != m_myClientID) {
                    try {
                        OtherPlayerManager::GetInstance()->SpawnOtherPlayer(pkt->playerID);
                        LogToFile("[Spawn] Successfully spawned other player: " + std::to_string(pkt->playerID));
                    }
                    catch (const std::exception& e) {
                        LogToFile("[Error] Failed to spawn other player: " + std::string(e.what()));
                    }
                }
                break;
            }

            case PACKET_PLAYER_UPDATE: {
                PacketPlayerUpdate* pkt = (PacketPlayerUpdate*)buffer;
                if (pkt->clientID == m_myClientID) {
                    LogToFile("[Update] Ignoring own update packet");
                    return;
                }

                OtherPlayerManager::GetInstance()->UpdateOtherPlayer(
                    pkt->clientID, pkt->x, pkt->y, pkt->z, pkt->rotY);
                LogToFile("[Update] Successfully updated player: " + std::to_string(pkt->clientID));
                break;
            }

            default:
                LogToFile("[Warning] Unknown packet type: " + std::to_string(header->type));
                break;
        }
    }
    catch (const std::exception& e) {
        LogToFile("[Error] Failed to process packet: " + std::string(e.what()));
        // 예외를 상위로 전파하지 않고 여기서 처리
    }

    LogToFile("[ProcessPacket] Finished processing packet type: " + std::to_string(header->type));
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
    // 로그 기록 비활성화
    
    static bool logFailureReported = false;
    
    std::lock_guard<std::mutex> lock(m_logMutex);
    try {
        if (!m_logFile.is_open()) {
            if (!logFailureReported) {
                std::cerr << "Log file is not open" << std::endl;
                logFailureReported = true;
            }
            return;
        }
        
        time_t now = time(0);
        tm ltm;
        localtime_s(&ltm, &now);
        char timestamp[50];
        sprintf_s(timestamp, "[%02d:%02d:%02d] ", ltm.tm_hour, ltm.tm_min, ltm.tm_sec);
        
        m_logFile << timestamp << message << std::endl;
        m_logFile.flush();
    }
    catch (const std::exception& e) {
        if (!logFailureReported) {
            std::cerr << "Failed to write to log file: " << e.what() << std::endl;
            logFailureReported = true;
        }
    }
    
}

void NetworkManager::Update(GameTimer& gTimer, Scene* scene) {
    if (!m_scene || !m_isRunning) return;

    auto& player = scene->GetObj<PlayerObject>(L"PlayerObject");
    auto& position = player.GetComponent<Position>();
    auto& rotation = player.GetComponent<Rotation>();
    
    m_updateTimer += gTimer.DeltaTime();
    const float UPDATE_INTERVAL = 0.1f;  // 100ms마다 업데이트

    if (m_updateTimer >= UPDATE_INTERVAL) {
        SendPlayerUpdate(
            position.mFloat4.x, 
            position.mFloat4.y, 
            position.mFloat4.z,
            rotation.mFloat4.y
        );
        m_updateTimer = 0.0f;
    }
}

