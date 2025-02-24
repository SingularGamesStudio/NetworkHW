#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <utils.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <random>
#include <tuple>
#include <vector>

// split splits a message formatted as "\command data"
std::pair<std::string, std::string> split(std::string message) {
    auto iter = std::find(message.begin(), message.end(), ' ');
    return {message.substr(1, iter - message.begin() - 1), message.substr(iter - message.begin())};
}

int sfd;

std::mt19937 rnd(time(NULL));

void send(sockaddr_in client, std::string msg) {
    ssize_t res = sendto(sfd, msg.c_str(), msg.size(), 0, reinterpret_cast<sockaddr *>(&client), sizeof(sockaddr_in));
    if (res == -1)
        std::cout << strerror(errno) << std::endl;
}

int main(int argc, const char **argv) {
    const char *port = "2025";
    std::vector<sockaddr_in> clients(0);

    sfd = create_dgram_socket(nullptr, port, nullptr);
    int duelState = -1;
    std::unordered_map<int, int> in_duel_with;
    std::unordered_map<int, int> duel_task;

    if (sfd == -1) {
        printf("cannot create socket\n");
        return 1;
    }
    printf("listening!\n");

    while (true) {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(sfd, &readSet);

        timeval timeout = {0, 100000};  // 100 ms
        select(sfd + 1, &readSet, NULL, NULL, &timeout);

        if (FD_ISSET(sfd, &readSet)) {
            constexpr size_t buf_size = 1000;
            static char buffer[buf_size];
            memset(buffer, 0, buf_size);

            sockaddr_in sin;
            socklen_t slen = sizeof(sockaddr_in);
            ssize_t numBytes = recvfrom(sfd, buffer, buf_size - 1, 0, (sockaddr *)&sin, &slen);
            if (numBytes == 0) {
                continue;
            }
            auto [command, data] = split(buffer);

            if (command == "connect") {
                clients.push_back(sin);
                send(sin, "Connected to server. You are User-" + std::to_string(clients.size() - 1) + ". Write \\c to send a message, and \\mathduel to start one");
                continue;
            }

            int sender = 0;
            for (int i = 0; i < clients.size(); i++) {
                if (clients[i].sin_port == sin.sin_port && clients[i].sin_addr.s_addr == sin.sin_addr.s_addr) {
                    sender = i;
                    break;
                }
            }

            if (command == "c") {
                data = "User-" + std::to_string(sender) + ": " + data;
                for (int i = 0; i < clients.size(); i++) {
                    if (i == sender) {
                        continue;
                    }
                    send(clients[i], data);
                }
            } else if (command == "mathduel" && duelState == -1) {
                if (in_duel_with[sender] != 0) {
                    send(sin, "You cannot start a duel while participating in one");
                } else {
                    duelState = sender;
                    std::string announcement = "User-" + std::to_string(sender) + " wants to start a mathduel. Write \\mathduel to accept.";
                    for (int i = 0; i < clients.size(); i++) {
                        if (i == sender) {
                            continue;
                        }
                        send(clients[i], announcement);
                    }
                }
            } else if (command == "mathduel" && duelState != -1) {
                if (in_duel_with[sender] != 0) {
                    send(sin, "You cannot start a duel while participating in one");
                } else {
                    in_duel_with[duelState] = sender + 1;
                    in_duel_with[sender] = duelState + 1;
                    int t1 = rnd() % 100;
                    int t2 = rnd() % 100;
                    duel_task[sender] = t1 + t2;
                    duel_task[duelState] = t1 + t2;
                    std::string msg = "You are in a duel! Your task: " + std::to_string(t1) + " + " + std::to_string(t2) + ". To answer, use command \\ans";
                    send(clients[duelState], msg);
                    send(clients[sender], msg);
                    duelState = -1;
                }
            } else if (command == "ans") {
                if (in_duel_with[sender] == 0) {
                    send(sin, "You are not in a duel!");
                } else {
                    int opponent = in_duel_with[sender] - 1;
                    if (std::atoi(data.c_str()) != duel_task[sender]) {
                        std::swap(sender, opponent);
                    }
                    send(clients[opponent], "You lost the duel!");
                    send(clients[sender], "You won the duel!");
                    in_duel_with[sender] = 0;
                    in_duel_with[opponent] = 0;
                }
            }

            printf("(%s:%d) (%s, %s)\n", inet_ntoa(sin.sin_addr), sin.sin_port, command.c_str(), data.c_str());
        }
    }
    return 0;
}