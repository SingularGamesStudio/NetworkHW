#include <enet/enet.h>

#include <iostream>
#include <unordered_map>
#include <vector>

using std::string;

void send_reliable_packet(ENetPeer *peer, std::string message) {
    ENetPacket *packet = enet_packet_create(message.c_str(), message.size(), ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(peer, 0, packet);
}

struct Player {
    string name = "";
    float posx = 0;
    float posy = 0;
    int ping = 0;
    uint32_t lastPing = 0;
    ENetPeer *conn = nullptr;
};

int main(int argc, const char **argv) {
    if (enet_initialize() != 0) {
        printf("Cannot init ENet");
        return 1;
    }
    char nextID = 'A';
    std::unordered_map<char, Player> players;

    ENetAddress myAddress;

    myAddress.host = ENET_HOST_ANY;
    myAddress.port = 10886;
    uint32_t lastPing = 0;

    ENetHost *server = enet_host_create(&myAddress, 32, 2, 0, 0);

    if (!server) {
        printf("Cannot create ENet server\n");
        return 1;
    }

    while (true) {
        ENetEvent event;
        while (enet_host_service(server, &event, 10) > 0) {
            Player player = Player();
            string toSend, toSendAll, data, strx, stry;
            uint32_t time;
            char id;
            int delim;
            switch (event.type) {
                case ENET_EVENT_TYPE_CONNECT:
                    printf("Connection with %x:%u established\n", event.peer->address.host, event.peer->address.port);
                    player.conn = event.peer;
                    player.name = string(3, nextID);
                    send_reliable_packet(player.conn, "me " + string(1, nextID));
                    toSend = "players " + string(1, nextID) + " " + player.name;
                    toSendAll = toSend + "\n";
                    for (auto &p : players) {
                        // sending new player to others
                        char id = p.first;
                        Player other = p.second;
                        send_reliable_packet(other.conn, toSend);
                        toSendAll += string(1, id) + " " + other.name + "\n";
                    }
                    // sending other players to the new one
                    send_reliable_packet(player.conn, toSendAll);
                    players[nextID++] = player;
                    break;
                case ENET_EVENT_TYPE_RECEIVE:
                    data = ((char *)event.packet->data) + 2;
                    id = ((char *)event.packet->data)[0];
                    delim = data.find(" ");
                    stry = data.substr(delim + 1);
                    strx = data.substr(0, delim);
                    players[id].posx = std::strtof(strx.c_str(), nullptr);
                    players[id].posy = std::strtof(stry.c_str(), nullptr);

                    time = enet_time_get();

                    players[id].ping = time - players[id].lastPing;
                    players[id].lastPing = time;

                    std::cout << "player " << id << " sent position " << players[id].posx << " " << players[id].posx << " with ping " << players[id].ping << std::endl;
                    enet_packet_destroy(event.packet);
                    break;
                default:
                    break;
            };

            uint32_t curTime = enet_time_get();

            if (curTime > lastPing + 100) {
                lastPing = curTime;
                string msgPos = "pos ";
                string msgPing = "ping ";
                for (auto &p : players) {
                    char id = p.first;
                    Player plr = p.second;
                    msgPing += string(1, id) + " " + std::to_string(plr.ping) + "\n";
                    msgPos += string(1, id) + " " + std::to_string(plr.posx) + " " + std::to_string(plr.posy) + "\n";
                }
                for (auto &p : players) {
                    // std::cout << msgPos << "\n\n\n" << msgPing << std::endl;
                    ENetPacket *packet1 = enet_packet_create(msgPos.c_str(), msgPos.size() + 1, ENET_PACKET_FLAG_UNSEQUENCED);
                    enet_peer_send(p.second.conn, 1, packet1);
                    ENetPacket *packet2 = enet_packet_create(msgPing.c_str(), msgPing.size() + 1, ENET_PACKET_FLAG_UNSEQUENCED);
                    enet_peer_send(p.second.conn, 1, packet2);
                }
            }
        }
    }

    enet_host_destroy(server);

    atexit(enet_deinitialize);
    return 0;
}
