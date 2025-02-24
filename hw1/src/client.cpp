#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <utils.h>

#include <cstdio>
#include <cstring>
#include <iostream>
#include <thread>

ssize_t connect(int sfd, addrinfo server) {
    std::string message = "\\connect";
    return sendto(sfd, message.c_str(), message.size(), 0, server.ai_addr, server.ai_addrlen);
}

int *sfd;

void receiver() {
    while (true) {
        constexpr size_t buf_size = 1000;
        static char buffer[buf_size];
        memset(buffer, 0, buf_size);
        sockaddr_in sin;
        memset(&sin, 0, sizeof(sockaddr_in));
        socklen_t slen = 0;
        ssize_t numBytes = recvfrom(*sfd, buffer, buf_size - 1, 0, (sockaddr *)&sin, &slen);
        if (numBytes == 0 || (numBytes == -1 && errno == 11)) {
            continue;
        }
        std::cout << buffer << std::endl;
    }
}

int main(int argc, const char **argv) {
    const char *port = "2025";

    addrinfo server;
    sfd = new int();
    *sfd = create_dgram_socket("localhost", port, &server);

    if (*sfd == -1) {
        printf("Cannot create a socket\n");
        return 1;
    }
    std::cout << "Connecting to server..." << std::endl;
    connect(*sfd, server);

    while (true) {
        constexpr size_t buf_size = 1000;
        static char buffer[buf_size];
        memset(buffer, 0, buf_size);
        sockaddr_in sin;
        memset(&sin, 0, sizeof(sockaddr_in));
        socklen_t slen = 0;
        ssize_t numBytes = recvfrom(*sfd, buffer, buf_size - 1, 0, (sockaddr *)&sin, &slen);
        if (numBytes == 0 || (numBytes == -1 && errno == 11)) {
            continue;
        }
        std::cout << buffer << std::endl;
        break;
    }

    std::thread(receiver).detach();

    while (true) {
        std::string input;
        printf(">");
        std::getline(std::cin, input);
        ssize_t res = sendto(*sfd, input.c_str(), input.size(), 0, server.ai_addr, server.ai_addrlen);
        if (res == -1)
            std::cout << strerror(errno) << std::endl;
    }
    return 0;
}