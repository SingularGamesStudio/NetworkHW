#include <enet/enet.h>

#include <cstring>
#include <iostream>
#include <sstream>
#include <unordered_map>

#include "raylib.h"

using std::string;

struct Network {
    ENetHost *client = nullptr;
    ENetAddress lobbyAddress;
    ENetPeer *lobbyPeer = nullptr;
    ENetAddress gameAddress;
    ENetPeer *gamePeer = nullptr;
    uint32_t timeStart = 0;
    uint32_t lastPing = 0;
    uint32_t lastBigMessage = 0;
};

struct Player {
    string name = "";
    float posx = 0;
    float posy = 0;
    int ping = 0;
};

void send_reliable_packet(ENetPeer *peer, std::string message) {
    ENetPacket *packet = enet_packet_create(message.c_str(), message.size(), ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(peer, 0, packet);
}

void send_micro_packet(ENetPeer *peer, char id, float posx, float posy) {
    string msg = string(1, id) + " " + std::to_string(posx) + " " + std::to_string(posy);
    ENetPacket *packet = enet_packet_create(msg.c_str(), msg.size() + 1, ENET_PACKET_FLAG_UNSEQUENCED);
    enet_peer_send(peer, 1, packet);
}

void initGraphics() {
    int width = 800;
    int height = 600;
    InitWindow(width, height, "hw2 MIPT networked");

    const int scrWidth = GetMonitorWidth(0);
    const int scrHeight = GetMonitorHeight(0);
    if (scrWidth < width || scrHeight < height) {
        width = std::min(scrWidth, width);
        height = std::min(scrHeight - 150, height);
        SetWindowSize(width, height);
    }

    SetTargetFPS(60);  // Set our game to run at 60 frames-per-second
}

int initNetwork(Network &net) {
    if (enet_initialize() != 0) {
        printf("Cannot init ENet");
        return 1;
    }
    net.client = enet_host_create(nullptr, 2, 2, 0, 0);
    if (!net.client) {
        printf("Cannot create ENet client\n");
        return 1;
    }
    enet_address_set_host(&net.lobbyAddress, "localhost");
    net.lobbyAddress.port = 10887;
    net.lobbyPeer = enet_host_connect(net.client, &net.lobbyAddress, 2, 0);
    if (!net.lobbyPeer) {
        printf("Cannot connect to lobby");
        return 1;
    }
    net.timeStart = enet_time_get();
    return 0;
}

void parsePing(string data, std::unordered_map<char, Player> &players) {
    std::cout << "received player ping" << std::endl;
    std::istringstream stream(data);
    while (!stream.eof()) {
        string playerData;
        getline(stream, playerData);
        if (playerData.size() < 2) {
            continue;
        }
        string pingStr = playerData.substr(2);
        players[playerData[0]].ping = std::atoi(pingStr.c_str());
    }
}

void parsePlayers(string data, std::unordered_map<char, Player> &players) {
    std::cout << "received player names" << std::endl;
    std::istringstream stream(data);
    while (!stream.eof()) {
        string playerData;
        getline(stream, playerData);
        if (playerData.size() < 2) {
            continue;
        }
        players[playerData[0]].name = playerData.substr(2);
    }
}

void parsePositions(string data, std::unordered_map<char, Player> &players) {
    std::cout << "received player positions" << std::endl;
    std::istringstream stream(data);
    while (!stream.eof()) {
        string playerData;
        getline(stream, playerData);
        if (playerData.size() < 2) {
            continue;
        }
        int delim = playerData.substr(2).find(" ");
        string stry = playerData.substr(delim + 3);
        string strx = playerData.substr(2, delim + 2);
        players[playerData[0]].posx = std::strtof(strx.c_str(), nullptr);
        players[playerData[0]].posy = std::strtof(stry.c_str(), nullptr);
    }
}

int main(int argc, const char **argv) {
    initGraphics();
    Network net = Network();
    if (initNetwork(net) != 0) {
        return 1;
    }
    std::unordered_map<char, Player> players;
    char myID = 255;
    int gameState = 0;
    float posx = GetRandomValue(100, 1000);
    float posy = GetRandomValue(100, 500);
    float velx = 0.f;
    float vely = 0.f;
    while (!WindowShouldClose()) {
        const float dt = GetFrameTime();
        ENetEvent event;
        while (enet_host_service(net.client, &event, 10) > 0) {
            string data;
            switch (event.type) {
                case ENET_EVENT_TYPE_CONNECT:
                    if (gameState == 0) {
                        printf("Connection with lobby (%x:%u) established\n", event.peer->address.host, event.peer->address.port);
                        gameState = 1;
                    } else {
                        printf("Connection with game (%x:%u) established\n", event.peer->address.host, event.peer->address.port);
                        gameState = 2;
                    }
                    break;
                case ENET_EVENT_TYPE_RECEIVE:
                    data = string((char *)event.packet->data);
                    printf("packet received '%s'\n", data.c_str());
                    if (data.starts_with("game")) {
                        // connect to game server
                        string host = data.substr(5, data.substr(5).find(":"));
                        string port = data.substr(data.find(":") + 1);
                        enet_address_set_host(&net.gameAddress, host.c_str());
                        net.gameAddress.port = std::atoi(port.c_str());
                        net.gamePeer = enet_host_connect(net.client, &net.gameAddress, 2, 0);
                        if (!net.gamePeer) {
                            std::cout << "failed to connect to game" << std::endl;
                            return 1;
                        }
                    } else if (data.starts_with("ping")) {
                        // save ping for output
                        parsePing(data.substr(5), players);
                    } else if (data.starts_with("me")) {
                        // save your id
                        myID = data[3];
                        printf("Your player id is %c\n", myID);
                    } else if (data.starts_with("players")) {
                        // add new players
                        parsePlayers(data.substr(8), players);
                    } else if (data.starts_with("pos")) {
                        // move players
                        parsePositions(data.substr(4), players);
                    } else {
                        printf("Unknown packet received '%s'\n", data.c_str());
                    }
                    enet_packet_destroy(event.packet);
                    break;
                default:
                    break;
            };
        }
        uint32_t curTime = enet_time_get();
        if (gameState == 1) {
            if (IsKeyDown(KEY_ENTER) && curTime > net.lastBigMessage + 300) {
                net.lastBigMessage = curTime;
                send_reliable_packet(net.lobbyPeer, "start");
            }
        } else if (gameState == 2) {
            if (myID != 0 && curTime - net.lastPing > 100) {
                net.lastPing = curTime;
                send_micro_packet(net.gamePeer, myID, posx, posy);
            }
        }
        bool left = IsKeyDown(KEY_LEFT);
        bool right = IsKeyDown(KEY_RIGHT);
        bool up = IsKeyDown(KEY_UP);
        bool down = IsKeyDown(KEY_DOWN);
        constexpr float accel = 30.f;
        velx += ((left ? -1.f : 0.f) + (right ? 1.f : 0.f)) * dt * accel;
        vely += ((up ? -1.f : 0.f) + (down ? 1.f : 0.f)) * dt * accel;
        posx += velx * dt;
        posy += vely * dt;
        velx *= 0.99f;
        vely *= 0.99f;

        BeginDrawing();
        if (gameState == 1) {
            DrawText("Press enter to start", 20, 60, 20, WHITE);
        } else {
            ClearBackground(BLACK);
            DrawText(TextFormat("My position: (%d, %d)", (int)posx, (int)posy), 20, 40, 20, WHITE);
            DrawText("List of players:", 20, 60, 20, WHITE);
            int height = 80;
            for (auto &p : players) {
                char id = p.first;
                Player player = p.second;
                string line = "Player " + player.name + ((id == myID) ? "(You)" : "") + ": ping " + std::to_string(player.ping);
                DrawText(line.c_str(), 20, height, 20, WHITE);
                height += 20;
                if (id != myID)
                    DrawCircleV(Vector2{player.posx, player.posy}, 10.f, WHITE);
            }
            DrawCircleV(Vector2{posx, posy}, 10.f, WHITE);
        }
        EndDrawing();
    }
    return 0;
}
