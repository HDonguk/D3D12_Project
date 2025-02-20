#pragma once
#include <winsock2.h>
#include <string>
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
    int clientID;
    float x, y, z;          // Position
    float rotY;             // Rotation
    float velX, velY, velZ; // Velocity
    int animationState;     // Current animation state
    bool isGrounded;        // Gravity/ground check
    bool isColliding;       // Collision state
};

struct PacketPlayerSpawn {
    PacketHeader header;
    int packetID;
    int playerID;
    float x, y, z;          // Position
    float rotY;             // Rotation
    float velX, velY, velZ; // Velocity
    int animationState;     // Current animation state
    bool isGrounded;        // Gravity/ground check
    bool isColliding;       // Collision state
    float scale;            // Player scale (needed for spawn)
};
#pragma pack(pop)