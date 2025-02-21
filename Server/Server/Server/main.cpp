#include "Server.h"
#include "TigerManager.h"

#define SERVER_PORT 5000

int main() {
    Server server;
    TigerManager* tigerManager = TigerManager::GetInstance();
    
    if (!server.Initialize(SERVER_PORT)) {
        return 1;
    }
    
    server.SetTigerManager(tigerManager);
    
    LARGE_INTEGER frequency, lastTime, currentTime;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&lastTime);
    
    while (true) {
        server.Update();
        
        // Tiger 업데이트
        QueryPerformanceCounter(&currentTime);
        float deltaTime = (float)(currentTime.QuadPart - lastTime.QuadPart) / frequency.QuadPart;
        if (deltaTime >= 0.033f) {  // 약 30FPS
            tigerManager->Update(deltaTime);
            lastTime = currentTime;
        }
    }
    
    return 0;
} 