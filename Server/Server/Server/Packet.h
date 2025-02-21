#pragma once
#include <winsock2.h>

#pragma pack(push, 1)
struct PacketHeader {
    unsigned short size;
    unsigned short type;
};

enum PacketType {
    PACKET_PLAYER_UPDATE = 1,
    PACKET_PLAYER_SPAWN = 2,
    TIGER_SPAWN = 3,
    TIGER_UPDATE = 4,
    TIGER_REMOVE = 5
};

struct PacketPlayerUpdate {
    PacketHeader header;
    int clientID;
    float x, y, z;    // Position
    float rotY;       // Rotation
};

struct PacketPlayerSpawn {
    PacketHeader header;
    int playerID;     // 클라이언트 ID만 필요
};

struct TigerSpawnPacket {
     PacketHeader header;  // unsigned char 대신 PacketHeader 사용
    int tigerID;
    float x, y, z;
};

struct TigerUpdatePacket {
    PacketHeader header;  // unsigned char 대신 PacketHeader 사용
    int tigerID;
    float x, y, z;
    float rotY;
};
#pragma pack(pop)