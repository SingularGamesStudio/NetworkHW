#include <enet/enet.h>

#include <iostream>
#include <vector>

void send_reliable_packet(ENetPeer *peer, std::string message) {
    ENetPacket *packet = enet_packet_create(message.c_str(), message.size(), ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(peer, 0, packet);
}

int main(int argc, const char **argv) {
    if (enet_initialize() != 0) {
        printf("Cannot init ENet");
        return 1;
    }
    int gameState = 0;

    ENetAddress myAddress;

    myAddress.host = ENET_HOST_ANY;
    myAddress.port = 10887;

    ENetHost *server = enet_host_create(&myAddress, 32, 2, 0, 0);
    std::vector<ENetPeer *> players;

    if (!server) {
        printf("Cannot create ENet server\n");
        return 1;
    }

    while (true) {
        ENetEvent event;
        while (enet_host_service(server, &event, 10) > 0) {
            switch (event.type) {
                case ENET_EVENT_TYPE_CONNECT:
                    printf("Connection with %x:%u established\n", event.peer->address.host, event.peer->address.port);
                    players.push_back(event.peer);
                    if (gameState > 0) {
                        send_reliable_packet(event.peer, "game localhost:10886");
                    }
                    break;
                case ENET_EVENT_TYPE_RECEIVE:
                    printf("Packet received '%s'\n", event.packet->data);
                    if (gameState == 0 && std::string((char *)event.packet->data) == "start") {
                        gameState = 1;
                        for (ENetPeer *player : players) {
                            send_reliable_packet(player, "game localhost:10886");
                        }
                    }
                    enet_packet_destroy(event.packet);
                    break;
                default:
                    break;
            };
        }
    }

    enet_host_destroy(server);

    atexit(enet_deinitialize);
    return 0;
}
