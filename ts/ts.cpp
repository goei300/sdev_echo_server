#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vector>
#include <mutex>
#include <thread>
#include <algorithm>

#ifdef __linux__
#include <arpa/inet.h>
#include <sys/socket.h>
#include <iostream>
#endif // __linux__

#ifdef WIN32
#include <winsock2.h>
#include "../mingw_net.h"
void myerror(const char* msg) { fprintf(stderr, "%s %ld\n", msg, GetLastError()); }
#else
void myerror(const char* msg) { fprintf(stderr, "%s %s %d\n", msg, strerror(errno), errno); }
#endif

std::vector<int> client_sockets;
std::mutex mtx; 

void usage() {
    printf("tcp server\n");
    printf("\n");
    printf("syntax: ts <port> [-e] [-si <src ip>]\n");
    printf("  -e : echo\n");
    printf("sample: ts 1234\n");
}

struct Param {
    bool echo = false;
    bool broad = false;
    uint16_t port = 0;
    uint32_t srcIp = INADDR_ANY;
    
    bool parse(int argc, char* argv[]) {
        for (int i = 1; i < argc; ) {
            if (strcmp(argv[i], "-e") == 0) {
                echo = true;
                i++;
                continue;
            }
            if (strcmp(argv[i], "-b") == 0) {
                broad = true;
                i++;
                continue;
            }
            if (strcmp(argv[i], "-si") == 0) {
                if (++i < argc && inet_pton(AF_INET, argv[i], &srcIp) != 1) {
                    fprintf(stderr, "Invalid network address\n");
                    return false;
                }
                i++;
                continue;
            }
            port = atoi(argv[i++]);
        }
        return port != 0;
    }
} param;

void recvThread(int sd) {
    printf("Client connected: %d\n", sd);
    static const int BUFSIZE = 65536;
    char buf[BUFSIZE];
    
    while (true) {
        ssize_t res = recv(sd, buf, BUFSIZE - 1, 0);
        if (res <= 0) {
            fprintf(stderr, "recv returned %ld\n", res);
            myerror("recv error");
            break;
        }
        buf[res] = '\0';
        printf("Received from %d: %s\n", sd, buf);
        if (param.echo) {
            std::lock_guard<std::mutex> lock(mtx);
            if (param.broad) {
				printf("hihi\n");
                for (int client_sd : client_sockets) {
                    res = ::send(client_sd, buf, res, 0);
					if (res == 0 || res == -1) {
						fprintf(stderr, "send return %ld", res);
						myerror(" ");
						break;
					}
                }
            } else {
                res = ::send(sd, buf, res, 0);
				if (res == 0 || res == -1) {
					fprintf(stderr, "send return %ld", res);
					myerror(" ");
					break;
				}
            }
        }
    }
    printf("Client disconnected: %d\n", sd);
    close(sd);
}

int main(int argc, char* argv[]) {
    if (!param.parse(argc, argv)) {
        usage();
        return -1;
    }

#ifdef WIN32
    WSAData wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fprintf(stderr, "WSAStartup failed.\n");
        return -1;
    }
#endif

    int sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd == -1) {
        myerror("socket creation failed");
        return -1;
    }

#ifdef __linux__
    int optval = 1;
    if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
        myerror("setsockopt failed");
        return -1;
    }
#endif

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = param.srcIp;
    addr.sin_port = htons(param.port);

    if (bind(sd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        myerror("bind failed");
        return -1;
    }

    if (listen(sd, 5) == -1) {
        myerror("listen failed");
        return -1;
    }

    while (true) {
        socklen_t len = sizeof(addr);
        int newsd = accept(sd, (struct sockaddr *)&addr, &len);
        if (newsd == -1) {
            myerror("accept failed");
            continue;  // Don't break the loop, continue accepting new clients
        }

        std::thread([newsd]() {
            std::lock_guard<std::mutex> lock(mtx);
            client_sockets.push_back(newsd);
            recvThread(newsd);
            std::lock_guard<std::mutex> lock2(mtx);
            client_sockets.erase(std::remove(client_sockets.begin(), client_sockets.end(), newsd), client_sockets.end());
        }).detach();
    }

    close(sd);
#ifdef WIN32
    WSACleanup();
#endif
    return 0;
}
