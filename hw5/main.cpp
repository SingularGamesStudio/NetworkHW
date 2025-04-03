// initial skeleton is a clone from https://github.com/jpcy/bgfx-minimal-example
//
#include <enet/enet.h>
#include <math.h>

#include <cstdio>
#include <deque>
#include <functional>
#include <iostream>
#include <set>
#include <unordered_map>

#include "entity.h"
#include "protocol.h"
#include "raylib.h"

static std::unordered_map<int, std::set<EntityState>> receivedStates;
static std::unordered_map<int, Entity> entities;

static std::deque<EntityState> history;
int historyStart = 0;
static EntityState correction = {0, 0, 0};

static uint16_t my_entity = invalid_entity;

const float pi = acos(-1);

static uint32_t frame = 0;

void on_new_entity_packet(ENetPacket *packet) {
    Entity newEntity;

    deserialize_new_entity(packet, newEntity);
    entities[newEntity.eid] = newEntity;
}

void on_set_controlled_entity(ENetPacket *packet) {
    uint32_t server_time;
    deserialize_set_controlled_entity(packet, my_entity, server_time);
    frame = server_time / fixedUpdate;
    historyStart = frame;
}

void on_snapshot(ENetPacket *packet) {
    uint16_t eid = invalid_entity;
    float x = 0.f;
    float y = 0.f;
    float ori = 0.f;
    uint32_t t = 0.f;
    deserialize_snapshot(packet, eid, x, y, ori, t);
    if (frame < t + 10) {  // we are in sync with the server (10 frames are 200 ms)
        receivedStates[eid].insert({x, y, ori, t + (uint32_t)10});
    } else {  // correcting the state
        if (t >= historyStart && eid == my_entity) {
            correction.x += history[t - historyStart].x - x;
            correction.y += history[t - historyStart].y - y;
            correction.ori += history[t - historyStart].ori - ori;
        }
    }
}

int main(int argc, const char **argv) {
    if (enet_initialize() != 0) {
        printf("Cannot init ENet");
        return 1;
    }

    ENetHost *client = enet_host_create(nullptr, 1, 2, 0, 0);
    if (!client) {
        printf("Cannot create ENet client\n");
        return 1;
    }

    ENetAddress address;
    enet_address_set_host(&address, "localhost");
    address.port = 10131;

    ENetPeer *serverPeer = enet_host_connect(client, &address, 2, 0);
    if (!serverPeer) {
        printf("Cannot connect to server");
        return 1;
    }

    int width = 600;
    int height = 600;

    InitWindow(width, height, "w5 networked MIPT");

    const int scrWidth = GetMonitorWidth(0);
    const int scrHeight = GetMonitorHeight(0);
    if (scrWidth < width || scrHeight < height) {
        width = std::min(scrWidth, width);
        height = std::min(scrHeight - 150, height);
        SetWindowSize(width, height);
    }

    Camera2D camera = {{0, 0}, {0, 0}, 0.f, 1.f};
    camera.target = Vector2{0.f, 0.f};
    camera.offset = Vector2{width * 0.5f, height * 0.5f};
    camera.rotation = 0.f;
    camera.zoom = 10.f;

    SetTargetFPS(60);  // Set our game to run at 60 frames-per-second

    bool connected = false;

    uint32_t lastTime = enet_time_get();
    while (!WindowShouldClose()) {
        uint32_t curTime = enet_time_get();
        ENetEvent event;
        while (enet_host_service(client, &event, 0) > 0) {
            switch (event.type) {
                case ENET_EVENT_TYPE_CONNECT:
                    printf("Connection with %x:%u established\n", event.peer->address.host, event.peer->address.port);
                    send_join(serverPeer);
                    connected = true;
                    break;
                case ENET_EVENT_TYPE_RECEIVE:
                    switch (get_packet_type(event.packet)) {
                        case E_SERVER_TO_CLIENT_NEW_ENTITY:
                            on_new_entity_packet(event.packet);
                            break;
                        case E_SERVER_TO_CLIENT_SET_CONTROLLED_ENTITY:
                            on_set_controlled_entity(event.packet);
                            break;
                        case E_SERVER_TO_CLIENT_SNAPSHOT:
                            on_snapshot(event.packet);
                            break;
                    };
                    break;
                default:
                    break;
            };
        }

        int dt = (int)(curTime / fixedUpdate) - (int)(lastTime / fixedUpdate);
        frame += dt;

        if (my_entity != invalid_entity) {
            bool left = IsKeyDown(KEY_LEFT);
            bool right = IsKeyDown(KEY_RIGHT);
            bool up = IsKeyDown(KEY_UP);
            bool down = IsKeyDown(KEY_DOWN);
            float thr = (up ? 1.f : 0.f) + (down ? -1.f : 0.f);
            float steer = (left ? -1.f : 0.f) + (right ? 1.f : 0.f);
            entities[my_entity].thr = thr;
            entities[my_entity].steer = steer;

            // Send
            send_entity_input(serverPeer, my_entity, thr, steer);
        }

        BeginDrawing();
        ClearBackground(GRAY);
        BeginMode2D(camera);
        for (auto &p : entities) {
            int eid = p.first;
            Entity &cur = p.second;
            std::set<EntityState> &states = receivedStates[eid];
            // delete old snapshots

            if (eid == my_entity && historyStart > 0 && dt > 0) {
                for (int i = 0; i < dt - 1; i++) {
                    history.push_back({cur.x, cur.y, cur.ori});
                    if (history.size() > 200) {
                        history.pop_front();
                        historyStart++;
                    }
                }
            }

            while (!states.empty() && (*states.begin()).physFrame <= frame) {
                EntityState next = *states.begin();
                cur.x = next.x;
                cur.y = next.y;
                cur.ori = next.ori;
                cur.physFrame = next.physFrame;
                correction = {0, 0, 0};
                states.erase(states.begin());
            }

            float interX = 0, interY = 0, interOri = 0;
            // we have a later snapshot,  linear interpolation
            if (!states.empty()) {
                EntityState next = *states.begin();
                int dtFull = next.physFrame - entities[eid].physFrame;
                int dt1 = next.physFrame - frame, dt0 = frame - cur.physFrame;
                interX = ((cur.x + correction.x) * dt1 + next.x * dt0) / dtFull;
                interY = ((cur.y + correction.y) * dt1 + next.y * dt0) / dtFull;
                interOri = ((cur.ori + correction.ori) * dt1 + next.ori * dt0) / dtFull;
            } else {  // desync, simulating local entity
                if (eid == my_entity) {
                    simulate_entity(cur, dt);
                }
                interX = cur.x + correction.x;
                interY = cur.y + correction.y;
                interOri = cur.ori + correction.ori;
            }
            if (eid == my_entity && historyStart > 0 && dt > 0) {
                history.push_back({interX, interY, interOri});
            }

            const Rectangle rect = {interX, interY, 3.f, 1.f};
            DrawRectanglePro(rect, {0.f, 0.5f}, interOri * 180.f / PI, GetColor(cur.color));
        }
        lastTime = curTime;
        EndMode2D();
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
