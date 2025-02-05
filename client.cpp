#include <iostream>
#include <thread>
#include <string>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>

#define PORT 8080
#define SERVER_IP "127.0.0.1"
using namespace std;
void receive_messages(int socket) {
    char buffer[1024];
    while (true) {
        memset(buffer, 0, 1024);
        int bytes_received = recv(socket, buffer, 1024, 0);
        if (bytes_received <= 0) {
            cout << "Disconnected from server." << endl;
            break;
        }
        cout << buffer << endl;
    }
}

int main() {
    int client_socket;
    sockaddr_in server_addr;

    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    if (connect(client_socket, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        cerr << "Connection to server failed!" << endl;
        return 1;
    }

    cout << "Connected to server. Start chatting!" << endl;
    thread receive_thread(receive_messages, client_socket);
    
    string message;
    while (true) {
        getline(cin, message);
        if (message == "exit") break;
        send(client_socket, message.c_str(), message.length(), 0);
    }

    close(client_socket);
    receive_thread.join();
    return 0;
}
