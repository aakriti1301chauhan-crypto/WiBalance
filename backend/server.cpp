#include "WiBalanceAPI.h"
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

#include <iostream>
#include <string>

#pragma comment(lib, "Ws2_32.lib")

int main() {
    WSADATA wsaData;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "Winsock initialization failed.\n";
        return 1;
    }

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "Could not create server socket.\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    serverAddress.sin_port = htons(8080);

    if (bind(serverSocket,
             reinterpret_cast<sockaddr*>(&serverAddress),
             sizeof(serverAddress)) == SOCKET_ERROR) {
        std::cerr << "Could not use port 8080. Is it already occupied?\n";
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Could not start listening.\n";
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "WiBalance backend is running!\n";
    std::cout << "Open http://127.0.0.1:8080/api/status\n";
    std::cout << "Press Ctrl+C to stop the server.\n";

    while (true) {
        SOCKET clientSocket = accept(serverSocket, nullptr, nullptr);

        if (clientSocket == INVALID_SOCKET) {
            continue;
        }

        char buffer[4096]{};
        int received = recv(
            clientSocket, buffer, sizeof(buffer) - 1, 0
        );

        std::string request;
        if (received > 0) {
            request.assign(buffer, received);
        }

        std::string body;

        if (request.rfind("GET /api/status ", 0) == 0) {
            body =
                "{\"service\":\"WiBalance\","
                "\"status\":\"online\","
                "\"tick\":0,"
                "\"totalTicks\":50,"
                "\"simulationConnected\":false}";
        } else {
            body = "{\"error\":\"Endpoint not found\"}";
        }

        std::string response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json; charset=utf-8\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Connection: close\r\n"
            "Content-Length: " + std::to_string(body.size()) +
            "\r\n\r\n" + body;

        send(clientSocket, response.c_str(),
             static_cast<int>(response.size()), 0);

        closesocket(clientSocket);
    }

    closesocket(serverSocket);
    WSACleanup();
    return 0;
}
