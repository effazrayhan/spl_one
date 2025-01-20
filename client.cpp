#include <iostream>
#include <cstring>
#include <thread>
#include <arpa/inet.h>
#include <unistd.h>

void receiveMessages(int socket) {
    char buffer[1024];
    while (true) {
        int bytesReceived = recv(socket, buffer, 1024, 0);
        if (bytesReceived <= 0) break;
        std::cout << "Server: " << std::string(buffer, bytesReceived) << std::endl;
    }
}

int main() {
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serverAddr{};

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8080); // Same port as server
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr); // Server IP (Localhost for LAN)

    if (connect(clientSocket, (sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Connection failed.\n";
        return -1;
    }

    std::thread(receiveMessages, clientSocket).detach();

    char buffer[1024];
    while (true) {
        std::cin.getline(buffer, 1024);
        if (std::string(buffer) == "exit") break;

        send(clientSocket, buffer, strlen(buffer), 0);
    }

    close(clientSocket);
    return 0;
}
