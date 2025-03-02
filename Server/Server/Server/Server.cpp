#include "Server.h"
#include <iostream>
#include <format>
#include <random>

GameServer::GameServer()
    : m_nextClientID(1)
    , m_nextTigerID(1)
    , m_isRunning(false)
    , m_hIOCP(NULL)
    , m_listenSocket(INVALID_SOCKET)
    , m_port(5000)
    , m_tigerUpdateTimer(0.0f)
    , m_randomEngine(std::random_device{}())
{
}

GameServer::~GameServer() {
    Cleanup();
}

bool GameServer::Initialize(int port) {
    m_port = port;
    std::cout << "[Server] Starting server on port " << m_port << std::endl;

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cout << "[Error] WSAStartup failed" << std::endl;
        return false;
    }

    m_listenSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (m_listenSocket == INVALID_SOCKET) {
        std::cout << "[Error] Failed to create listen socket" << std::endl;
        return false;
    }

    SOCKADDR_IN serverAddr = { 0 };
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(m_port);

    if (bind(m_listenSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cout << "[Error] Bind failed" << std::endl;
        return false;
    }

    if (listen(m_listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cout << "[Error] Listen failed" << std::endl;
        return false;
    }

    m_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (m_hIOCP == NULL) {
        std::cout << "[Error] CreateIoCompletionPort failed" << std::endl;
        return false;
    }

    InitializeTigers();
    return true;
}

void GameServer::Start() {
    m_isRunning = true;
    
    // Create worker threads
    for (int i = 0; i < 2; ++i) {
        HANDLE hThread = CreateThread(NULL, 0, WorkerThreadProc, this, 0, NULL);
        if (hThread) {
            m_workerThreads.push_back(hThread);
        }
    }

    // Main accept loop
    while (m_isRunning) {
        SOCKET clientSocket = accept(m_listenSocket, NULL, NULL);
        if (clientSocket != INVALID_SOCKET) {
            ProcessNewClient(clientSocket);
        }
    }
}

void GameServer::Stop() {
    m_isRunning = false;
    Cleanup();
}

DWORD WINAPI GameServer::WorkerThreadProc(LPVOID lpParam) {
    GameServer* server = static_cast<GameServer*>(lpParam);
    return server->WorkerThread();
}

DWORD GameServer::WorkerThread() {
    DWORD lastTime = GetTickCount();
    
    while (m_isRunning) {
        DWORD currentTime = GetTickCount();
        float deltaTime = (currentTime - lastTime) / 1000.0f;
        lastTime = currentTime;
        
        // 호랑이 업데이트
        UpdateTigers(deltaTime);
        
        // 기존 IOCP 처리 코드
        DWORD bytesTransferred;
        ULONG_PTR completionKey;
        OVERLAPPED* pOverlapped;
        
        BOOL result = GetQueuedCompletionStatus(m_hIOCP, &bytesTransferred, 
            &completionKey, &pOverlapped, 100); // 타임아웃을 100ms로 설정
        
        if (!m_isRunning) break;
        
        if (!pOverlapped) continue;
        
        IOContext* ioContext = CONTAINING_RECORD(pOverlapped, IOContext, overlapped);
        int clientID = static_cast<int>(completionKey);
        
        if (m_clients.find(clientID) == m_clients.end()) {
            delete ioContext;
            continue;
        }
        
        if (!result || bytesTransferred == 0) {
            std::cout << "\n[Disconnect] Client ID " << clientID << " disconnected" << std::endl;
            closesocket(m_clients[clientID].socket);
            m_clients.erase(clientID);
            std::cout << "  -> Active clients remaining: " << m_clients.size() << std::endl;
            delete ioContext;
            continue;
        }

        HandlePacket(ioContext, clientID, bytesTransferred);

        // 다음 수신 준비
        memset(ioContext->buffer, 0, sizeof(ioContext->buffer));
        memset(&ioContext->overlapped, 0, sizeof(OVERLAPPED));
        ioContext->wsaBuf.len = sizeof(ioContext->buffer);
        ioContext->flags = 0;

        DWORD recvBytes;
        if (WSARecv(m_clients[clientID].socket, &ioContext->wsaBuf, 1, &recvBytes,
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

void GameServer::HandlePacket(IOContext* ioContext, int clientID, DWORD bytesTransferred) {
    PacketHeader* header = (PacketHeader*)ioContext->buffer;
    if (header->size == 0 || header->size > MAX_PACKET_SIZE || header->type <= 0) {
        std::cout << "[Error] Invalid packet header - Size: " << header->size 
                  << ", Type: " << header->type << std::endl;
        return;
    }

    if (bytesTransferred < sizeof(PacketHeader)) {
        std::cout << "[Error] Invalid packet size (smaller than header)" << std::endl;
        return;
    }

    if (bytesTransferred < header->size) {
        std::cout << "[Error] Incomplete packet received" << std::endl;
        return;
    }

    std::cout << "\n[Receive] From client " << clientID << std::endl;
    std::cout << "  -> Packet type: " << header->type << std::endl;
    std::cout << "  -> Packet size: " << header->size << std::endl;

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
            PacketPlayerSpawn* pkt = (PacketPlayerSpawn*)ioContext->buffer;
            BroadcastPacket(pkt, sizeof(PacketPlayerSpawn), clientID);
            break;
        }
        case PACKET_TIGER_SPAWN: {
            if (header->size != sizeof(PacketTigerSpawn)) {
                std::cout << "[Error] Invalid TIGER_SPAWN packet size" << std::endl;
                break;
            }
            PacketTigerSpawn* pkt = (PacketTigerSpawn*)ioContext->buffer;
            BroadcastPacket(pkt, sizeof(PacketTigerSpawn));
            break;
        }
        case PACKET_TIGER_UPDATE: {
            if (header->size != sizeof(PacketTigerUpdate)) {
                std::cout << "[Error] Invalid TIGER_UPDATE packet size" << std::endl;
                break;
            }
            PacketTigerUpdate* pkt = (PacketTigerUpdate*)ioContext->buffer;
            BroadcastPacket(pkt, sizeof(PacketTigerUpdate));
            break;
        }
        default:
            std::cout << "  -> Unknown packet type" << std::endl;
            break;
    }
}

void GameServer::Cleanup() {
    m_isRunning = false;

    for (auto& [id, client] : m_clients) {
        closesocket(client.socket);
    }
    m_clients.clear();

    if (m_listenSocket != INVALID_SOCKET) {
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
    }

    for (HANDLE hThread : m_workerThreads) {
        WaitForSingleObject(hThread, INFINITE);
        CloseHandle(hThread);
    }
    m_workerThreads.clear();

    if (m_hIOCP) {
        CloseHandle(m_hIOCP);
        m_hIOCP = NULL;
    }

    WSACleanup();
}

void GameServer::BroadcastPacket(const void* packet, int size, int excludeID) {
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
    else if (header->type == PACKET_TIGER_UPDATE) {
        PacketTigerUpdate* pkt = (PacketTigerUpdate*)packet;
        std::cout << "  -> Broadcasting tiger position: (" << pkt->x << ", " << pkt->y << ", " << pkt->z
            << "), Rotation: " << pkt->rotY << std::endl;
    }
    else if (header->type == PACKET_TIGER_SPAWN) {
        PacketTigerSpawn* pkt = (PacketTigerSpawn*)packet;
        std::cout << "  -> Broadcasting tiger spawn: ID " << pkt->tigerID 
            << " at (" << pkt->x << ", " << pkt->y << ", " << pkt->z << ")" << std::endl;
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
            
            if (error != WSAEWOULDBLOCK) {
                closesocket(client.socket);
                m_clients.erase(id);
            }
        }
        else {
            std::cout << "  -> Successfully sent to client " << id << std::endl;
            sentCount++;
        }
    }
    std::cout << "  -> Total broadcasts sent: " << sentCount << "/" << targetCount << std::endl;
}

void GameServer::ProcessNewClient(SOCKET clientSocket) {
    int clientID = m_nextClientID++;
    std::cout << "[Info] ProcessNewClient" << std::endl;
    
    ClientInfo newClient;
    newClient.socket = clientSocket;
    newClient.clientID = clientID;
    newClient.lastUpdate = { 0 };
    
    // 1. IOCP 설정
    if (CreateIoCompletionPort((HANDLE)clientSocket, m_hIOCP, clientID, 0) == NULL) {
        std::cout << "[Error] Failed to associate with IOCP" << std::endl;
        closesocket(clientSocket);
        return;
    }
    
    // 2. 수신 시작 - 먼저 시작
    IOContext* ioContext = new IOContext();
    if (!StartReceive(clientSocket, clientID, ioContext)) {
        std::cout << "[Error] Failed to start receive" << std::endl;
        delete ioContext;
        closesocket(clientSocket);
        return;
    }
    
    // 3. 클라이언트 맵에 추가
    m_clients[clientID] = newClient;
    std::cout << "[ProcessNewClient] " << clientID << " added to map. Total clients: " << m_clients.size() << std::endl;
    
    // 4. 플레이어 스폰 패킷 전송
    PacketPlayerSpawn spawnPacket;
    spawnPacket.header.type = PACKET_PLAYER_SPAWN;
    spawnPacket.header.size = sizeof(PacketPlayerSpawn);
    spawnPacket.playerID = clientID;
    
    if (!SendPacket(clientSocket, &spawnPacket, sizeof(spawnPacket))) {
        std::cout << "[Error] Failed to send player spawn packet" << std::endl;
        return;
    }
    
    // 5. 타이거 정보 전송
    std::cout << "[ProcessNewClient] Sending existing tiger information..." << std::endl;
    
    // 모든 호랑이 정보를 하나의 버퍼에 모음
    std::vector<PacketTigerSpawn> tigerPackets;
    for (const auto& [tigerID, tiger] : m_tigers) {
        PacketTigerSpawn tigerPacket;
        tigerPacket.header.type = PACKET_TIGER_SPAWN;
        tigerPacket.header.size = sizeof(PacketTigerSpawn);
        tigerPacket.tigerID = tiger.tigerID;
        tigerPacket.x = tiger.x;
        tigerPacket.y = tiger.y;
        tigerPacket.z = tiger.z;
        tigerPackets.push_back(tigerPacket);
    }
    
    // 패킷 크기 정보 전송
    int numTigers = static_cast<int>(tigerPackets.size());
    if (!SendPacket(clientSocket, &numTigers, sizeof(int))) {
        std::cout << "[Error] Failed to send tiger count" << std::endl;
        closesocket(clientSocket);
        return;
    }
    
    // 모든 호랑이 패킷을 한 번에 전송
    for (const auto& packet : tigerPackets) {
        if (!SendPacket(clientSocket, &packet, sizeof(PacketTigerSpawn))) {
            std::cout << "[Error] Failed to send tiger spawn packet for ID: " << packet.tigerID << std::endl;
            closesocket(clientSocket);
            return;
        }
        std::cout << "[Success] Sent tiger spawn packet for ID: " << packet.tigerID << std::endl;
    }
    
    // 전송 완료 확인 패킷
    int confirmPacket = 1;
    if (!SendPacket(clientSocket, &confirmPacket, sizeof(int))) {
        std::cout << "[Error] Failed to send confirmation packet" << std::endl;
        closesocket(clientSocket);
        return;
    }
    
    // 6. 다른 플레이어 정보 브로드캐스트
    BroadcastNewPlayer(clientID);
}

bool GameServer::SendPacket(SOCKET socket, const void* packet, int size) {
    const char* data = static_cast<const char*>(packet);
    int totalSent = 0;
    int retryCount = 0;
    const int MAX_RETRIES = 3;
    
    while (totalSent < size && retryCount < MAX_RETRIES) {
        int result = send(socket, data + totalSent, size - totalSent, 0);
        if (result == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error != WSAEWOULDBLOCK) {
                std::cout << "[Error] Send failed with error: " << error << std::endl;
                return false;
            }
            Sleep(10);  // WSAEWOULDBLOCK 발생 시 잠시 대기
            retryCount++;
            continue;
        }
        totalSent += result;
        retryCount = 0;  // 성공하면 재시도 카운트 리셋
    }
    
    return totalSent == size;
}

bool GameServer::StartReceive(SOCKET clientSocket, int clientID, IOContext* ioContext) {
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
            return false;
        }
    }
    return true;
}

void GameServer::BroadcastNewPlayer(int newClientID) {
    std::cout << "\n[BroadcastNewPlayer] New client ID: " << newClientID << std::endl;
    
    for (const auto& [id, client] : m_clients) {
        if (id != newClientID) {
            PacketPlayerSpawn existingClientPacket;
            existingClientPacket.header.type = PACKET_PLAYER_SPAWN;
            existingClientPacket.header.size = sizeof(PacketPlayerSpawn);
            existingClientPacket.playerID = id;
            
            send(m_clients[newClientID].socket, (char*)&existingClientPacket, sizeof(existingClientPacket), 0);
        }
    }
    
    PacketPlayerSpawn newClientPacket;
    newClientPacket.header.type = PACKET_PLAYER_SPAWN;
    newClientPacket.header.size = sizeof(PacketPlayerSpawn);
    newClientPacket.playerID = newClientID;
    
    BroadcastPacket(&newClientPacket, sizeof(newClientPacket), newClientID);
}

void GameServer::InitializeTigers() {
    std::cout << "\n[InitializeTigers] Starting tiger initialization..." << std::endl;
    
    // 초기 호랑이 생성
    for (int i = 0; i < MAX_TIGERS; ++i) {
        TigerInfo tiger;
        tiger.tigerID = m_nextTigerID++;
        tiger.x = GetRandomFloat(0.0f, 1000.0f);
        tiger.y = 0.0f;
        tiger.z = GetRandomFloat(0.0f, 1000.0f);
        tiger.rotY = GetRandomFloat(0.0f, 360.0f);
        tiger.moveTimer = GetRandomFloat(0.0f, 5.0f);
        tiger.isChasing = false;
        
        m_tigers[tiger.tigerID] = tiger;

        std::cout << "[Tiger] Created tiger ID: " << tiger.tigerID 
                  << " at position (" << tiger.x << ", " << tiger.y << ", " << tiger.z << ")" << std::endl;

        // 호랑이 스폰 패킷 전송
        PacketTigerSpawn spawnPacket;
        spawnPacket.header.type = PACKET_TIGER_SPAWN;
        spawnPacket.header.size = sizeof(PacketTigerSpawn);
        spawnPacket.tigerID = tiger.tigerID;
        spawnPacket.x = tiger.x;
        spawnPacket.y = tiger.y;
        spawnPacket.z = tiger.z;
        
        // 현재 연결된 클라이언트 수 확인
        std::cout << "[Tiger] Attempting to broadcast spawn packet for tiger ID: " << tiger.tigerID 
                  << " to " << m_clients.size() << " clients" << std::endl;
        
        BroadcastPacket(&spawnPacket, sizeof(spawnPacket));
    }
    
    std::cout << "[InitializeTigers] Completed. Total tigers created: " << m_tigers.size() << std::endl;
}

void GameServer::UpdateTigers(float deltaTime) {
    m_tigerUpdateTimer += deltaTime;
    if (m_tigerUpdateTimer < 0.1f) return; // 100ms마다 업데이트

    for (auto& [id, tiger] : m_tigers) {
        UpdateTigerBehavior(tiger, m_tigerUpdateTimer);
    }

    BroadcastTigerUpdates();
    m_tigerUpdateTimer = 0.0f;
}

void GameServer::UpdateTigerBehavior(TigerInfo& tiger, float deltaTime) {
    const float CHASE_RADIUS = 200.0f;
    const float MOVE_SPEED = 15.0f;
    
    tiger.moveTimer -= deltaTime;
    
    if (IsPlayerNearby(tiger, CHASE_RADIUS)) {
        tiger.isChasing = true;
        float targetX, targetZ;
        GetNearestPlayerPosition(tiger, targetX, targetZ);
        
        // 플레이어 방향으로 이동
        float dx = targetX - tiger.x;
        float dz = targetZ - tiger.z;
        float dist = sqrt(dx * dx + dz * dz);
        if (dist > 0.1f) {
            tiger.x += (dx / dist) * MOVE_SPEED * deltaTime;
            tiger.z += (dz / dist) * MOVE_SPEED * deltaTime;
            tiger.rotY = atan2(dx, dz) * (180.0f / 3.141592f);
        }
    }
    else {
        tiger.isChasing = false;
        if (tiger.moveTimer <= 0.0f) {
            // 새로운 랜덤 이동
            tiger.moveTimer = GetRandomFloat(3.0f, 7.0f);
            float angle = GetRandomFloat(0.0f, 360.0f) * (3.141592f / 180.0f);
            tiger.targetX = tiger.x + cos(angle) * 100.0f;
            tiger.targetZ = tiger.z + sin(angle) * 100.0f;
        }
        
        // 목표 지점으로 이동
        float dx = tiger.targetX - tiger.x;
        float dz = tiger.targetZ - tiger.z;
        float dist = sqrt(dx * dx + dz * dz);
        if (dist > 0.1f) {
            tiger.x += (dx / dist) * MOVE_SPEED * 0.5f * deltaTime;
            tiger.z += (dz / dist) * MOVE_SPEED * 0.5f * deltaTime;
            tiger.rotY = atan2(dx, dz) * (180.0f / 3.141592f);
        }
    }
}

void GameServer::BroadcastTigerUpdates() {
    for (const auto& [id, tiger] : m_tigers) {
        PacketTigerUpdate updatePacket;
        updatePacket.header.type = PACKET_TIGER_UPDATE;
        updatePacket.header.size = sizeof(PacketTigerUpdate);
        updatePacket.tigerID = tiger.tigerID;
        updatePacket.x = tiger.x;
        updatePacket.y = tiger.y;
        updatePacket.z = tiger.z;
        updatePacket.rotY = tiger.rotY;
        
        BroadcastPacket(&updatePacket, sizeof(updatePacket));
    }
}

float GameServer::GetRandomFloat(float min, float max) {
    std::uniform_real_distribution<float> dist(min, max);
    return dist(m_randomEngine);
}

bool GameServer::IsPlayerNearby(const TigerInfo& tiger, float radius) {
    for (const auto& [id, client] : m_clients) {
        float dx = client.lastUpdate.x - tiger.x;
        float dz = client.lastUpdate.z - tiger.z;
        float distSq = dx * dx + dz * dz;
        if (distSq < radius * radius) {
            return true;
        }
    }
    return false;
}

void GameServer::GetNearestPlayerPosition(const TigerInfo& tiger, float& outX, float& outZ) {
    float minDistSq = FLT_MAX;
    outX = tiger.x;
    outZ = tiger.z;
    
    for (const auto& [id, client] : m_clients) {
        float dx = client.lastUpdate.x - tiger.x;
        float dz = client.lastUpdate.z - tiger.z;
        float distSq = dx * dx + dz * dz;
        if (distSq < minDistSq) {
            minDistSq = distSq;
            outX = client.lastUpdate.x;
            outZ = client.lastUpdate.z;
        }
    }
}

int main() {
    GameServer server;
    
    if (!server.Initialize(5000)) {  // 포트 번호 지정 가능
        std::cout << "[Error] Server initialization failed" << std::endl;
        return 1;
    }

    server.Start();
    return 0;
}