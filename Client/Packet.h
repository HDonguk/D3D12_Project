#pragma once
#include "stdafx.h"
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
    float scale;
    std::string currentAnim;
};
#pragma pack(pop)