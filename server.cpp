#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <mutex>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <algorithm>
#define PORT 8080
#define HOST "192.168.1.1"
#define MAX_CLIENTS 10

vector<int> clients;
mutex clients_mutex;
using namespace std;

void broadcast(const string& message, int sender_fd) {
    lock_guard<mutex> lock(clients_mutex);
    for (int client : clients) {
        if (client != sender_fd) {
            send(client, message.c_str(), message.length(), 0);
        }
    }
}

void handle_client(int client_socket) {
    char buffer[1024];
    while (true) {
        memset(buffer, 0, 1024);
        int bytes_received = recv(client_socket, buffer, 1024, 0);
        if (bytes_received <= 0) {
            lock_guard<mutex> lock(clients_mutex);
            close(client_socket);
            break;
        }
        string message = "Client " + to_string(client_socket) + ": " + buffer;
        cout << message;
        broadcast(message, client_socket);
    }
}

int main() {
    int server_socket, client_socket;
    sockaddr_in server_addr, client_addr;
    socklen_t client_size = sizeof(client_addr);
    //Creating a socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    inet_pton(AF_INET, HOST, &server_addr.sin_addr);
    server_addr.sin_port = htons(PORT);

    bind(server_socket, (sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_socket, MAX_CLIENTS);

    cout << "Server started on port " << PORT << endl;

    while (true) {
        client_socket = accept(server_socket, (sockaddr*)&client_addr, &client_size);
        lock_guard<mutex> lock(clients_mutex);
        clients.push_back(client_socket);
        thread(handle_client, client_socket).detach();
        cout << "New client connected: " << client_socket << endl;
    }

    return 0;
}
