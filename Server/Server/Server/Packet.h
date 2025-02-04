#pragma once
#include <winsock2.h>
#include <vector>

#pragma pack(push, 1)
struct PacketHeader {
    unsigned short size;
    unsigned short type;
};

enum PacketType {
    PACKET_PLAYER_UPDATE = 1,
    PACKET_PLAYER_SPAWN = 2
};

struct PacketPlayerUpdate {
    PacketHeader header;
    float x, y, z;
    float rotY;
};

struct PacketPlayerSpawn {
    PacketHeader header;
    int playerID;
    float x, y, z;
};
#pragma pack(pop)