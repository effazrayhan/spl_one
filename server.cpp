#include <iostream>
#include <thread>
#include <vector>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include <cstdlib>

#define PORT 8080
#define MAX_CLIENTS 5
using namespace std;
void handleClient(int clientSocket) {
    char buffer[1024];
    int readSize;
    while ((readSize = recv(clientSocket, buffer, sizeof(buffer), 0)) > 0) {
        buffer[readSize] = '\0';
        cout << "Received from client: " << buffer << endl;

        const char *message = "Message received";
        send(clientSocket, message, strlen(message), 0);
    }

    if (readSize == 0) {
        cout << "Client disconnected" << endl;
    } else if (readSize == -1) {
        perror("recv failed");
    }
    close(clientSocket);
}

int main() {
    int serverSocket, clientSocket;
    sockaddr_in serverAddr, clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);

    // Create the server socket
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);
    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Bind failed");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }
    if (listen(serverSocket, MAX_CLIENTS) < 0) {
        perror("Listen failed");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    cout << "Server listening on port " << PORT << endl;
    while (true) {
        clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
        if (clientSocket < 0) {
            perror("Client accept failed");
            continue;
        }

        cout << "Client connected: " << inet_ntoa(clientAddr.sin_addr) << endl;
        thread clientThread(handleClient, clientSocket);
        clientThread.detach(); 
    }
    close(serverSocket);
    return 0;
}
