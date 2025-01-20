#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <thread>
#include <vector>
#include <mutex>
#include <netinet/in.h>
#include <unistd.h>

map<string, pair<string, string>> chatrooms; // RoomID -> (Password, LogFile)
mutex fileMutex;
using namespace std;

string xorEncryptDecrypt(const string &data, char key = 'K') {
    string result = data;
    for (char &c : result) {
        c ^= key;
    }
    return result;
}

void handleClient(int clientSocket) {
    char buffer[1024];
    string roomId, password;
    send(clientSocket, "Enter Room ID: ", 15, 0);
    recv(clientSocket, buffer, 1024, 0);
    roomId = buffer;

    send(clientSocket, "Enter Room Password: ", 21, 0);
    recv(clientSocket, buffer, 1024, 0);
    password = buffer;

    if (chatrooms.find(roomId) == chatrooms.end() || chatrooms[roomId].first != password) {
        send(clientSocket, "Invalid Room ID or Password\n", 30, 0);
        close(clientSocket);
        return;
    }

    send(clientSocket, "Connected to the chatroom!\n", 27, 0);
    string logFile = chatrooms[roomId].second;

    while (true) {
        int bytesReceived = recv(clientSocket, buffer, 1024, 0);
        if (bytesReceived <= 0) break;

        string message(buffer, bytesReceived);
        {
            lock_guard<mutex> lock(fileMutex);
            ofstream outFile(logFile, ios::app);
            outFile << xorEncryptDecrypt(message) << "\n";
        }

        send(clientSocket, message.c_str(), message.size(), 0);
    }

    close(clientSocket);
}

void createChatroom() {
    string roomId, password, logFile;
    cout << "Enter Room ID: ";
    cin >> roomId;
    cout << "Enter Room Password: ";
    cin >> password;

    logFile = roomId + "_log.txt";
    chatrooms[roomId] = {password, logFile};

    ofstream outFile(logFile, ios::trunc);
    outFile << xorEncryptDecrypt("Chatroom created\n");
    outFile.close();

    cout << "Chatroom " << roomId << " created successfully.\n";
}

int main() {
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serverAddr{}, clientAddr{};
    socklen_t addrLen = sizeof(clientAddr);

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8080);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSocket, (sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        cerr << "Binding failed.\n";
        return -1;
    }

    listen(serverSocket, 5);
    cout << "Server started. Listening for connections...\n";
    thread(createChatroom).detach();

    while (true) {
        int clientSocket = accept(serverSocket, (sockaddr *)&clientAddr, &addrLen);
        if (clientSocket >= 0) {
            thread(handleClient, clientSocket).detach();
        }
    }

    close(serverSocket);
    return 0;
}
