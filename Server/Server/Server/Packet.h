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
    PACKET_TIGER_SPAWN = 3,    // 호랑이 스폰 패킷
    PACKET_TIGER_UPDATE = 4    // 호랑이 업데이트 패킷
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

struct PacketTigerSpawn {
    PacketHeader header;
    int tigerID;
    float x, y, z;
};

struct PacketTigerUpdate {
    PacketHeader header;
    int tigerID;
    float x, y, z;
    float rotY;
};
#pragma pack(pop)