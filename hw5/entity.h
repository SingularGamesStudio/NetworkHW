#pragma once
#include <cstdint>

// ms
const int fixedUpdate = 20;

constexpr uint16_t invalid_entity = -1;
struct Entity {
    uint32_t color = 0xff00ffff;
    float x = 0.f;
    float y = 0.f;
    float speed = 0.f;
    float ori = 0.f;

    float thr = 0.f;
    float steer = 0.f;

    uint16_t eid = invalid_entity;

    uint32_t physFrame;
};

struct EntityState {
    float x = 0.f;
    float y = 0.f;
    float ori = 0.f;

    uint32_t physFrame;

    // we can afford losing one of 2 states of the same frame when inserting into a set.
    bool operator<(const EntityState& other) const {
        return physFrame < other.physFrame;
    }
};

void simulate_entity(Entity& e, int frames);
