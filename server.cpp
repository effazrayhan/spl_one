#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <thread>
#include <vector>
#include <mutex>
#include <netinet/in.h>
#include <unistd.h>

std::map<std::string, std::pair<std::string, std::string>> chatrooms; // RoomID -> (Password, LogFile)
std::mutex fileMutex;

// XOR encryption for demonstration
std::string xorEncryptDecrypt(const std::string &data, char key = 'K') {
    std::string result = data;
    for (char &c : result) {
        c ^= key;
    }
    return result;
}

void handleClient(int clientSocket) {
    char buffer[1024];
    std::string roomId, password;

    // Ask for Room ID
    send(clientSocket, "Enter Room ID: ", 15, 0);
    recv(clientSocket, buffer, 1024, 0);
    roomId = buffer;

    // Ask for Password
    send(clientSocket, "Enter Room Password: ", 21, 0);
    recv(clientSocket, buffer, 1024, 0);
    password = buffer;

    // Verify Room
    if (chatrooms.find(roomId) == chatrooms.end() || chatrooms[roomId].first != password) {
        send(clientSocket, "Invalid Room ID or Password\n", 30, 0);
        close(clientSocket);
        return;
    }

    // Notify client
    send(clientSocket, "Connected to the chatroom!\n", 27, 0);
    std::string logFile = chatrooms[roomId].second;

    // Handle chat
    while (true) {
        int bytesReceived = recv(clientSocket, buffer, 1024, 0);
        if (bytesReceived <= 0) break;

        std::string message(buffer, bytesReceived);

        // Save message to file (encrypted)
        {
            std::lock_guard<std::mutex> lock(fileMutex);
            std::ofstream outFile(logFile, std::ios::app);
            outFile << xorEncryptDecrypt(message) << "\n";
        }

        // Broadcast to the client (Echo back)
        send(clientSocket, message.c_str(), message.size(), 0);
    }

    close(clientSocket);
}

void createChatroom() {
    std::string roomId, password, logFile;
    std::cout << "Enter Room ID: ";
    std::cin >> roomId;
    std::cout << "Enter Room Password: ";
    std::cin >> password;

    logFile = roomId + "_log.txt";
    chatrooms[roomId] = {password, logFile};

    std::ofstream outFile(logFile, std::ios::trunc);
    outFile << xorEncryptDecrypt("Chatroom created\n");
    outFile.close();

    std::cout << "Chatroom " << roomId << " created successfully.\n";
}

int main() {
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serverAddr{}, clientAddr{};
    socklen_t addrLen = sizeof(clientAddr);

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8080); // Server Port
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSocket, (sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Binding failed.\n";
        return -1;
    }

    listen(serverSocket, 5);
    std::cout << "Server started. Listening for connections...\n";

    // Create a chatroom
    std::thread(createChatroom).detach();

    while (true) {
        int clientSocket = accept(serverSocket, (sockaddr *)&clientAddr, &addrLen);
        if (clientSocket >= 0) {
            std::thread(handleClient, clientSocket).detach();
        }
    }

    close(serverSocket);
    return 0;
}
