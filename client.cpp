#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>

#define SERVER_IP "127.0.0.1"
#define PORT 8080
using namespace std;
int main() {
    int sock;
    struct sockaddr_in serverAddr;
    char buffer[1024];
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return -1;
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = inet_addr(SERVER_IP);

    if (connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Connection failed");
        return -1;
    }

    cout << "Connected to the server" << endl;

    while (true) {
        cout << "Enter message: ";
        cin.getline(buffer, sizeof(buffer));

        send(sock, buffer, strlen(buffer), 0);

        int len = recv(sock, buffer, sizeof(buffer), 0);
        if (len > 0) {
            buffer[len] = '\0';
            cout << "Server: " << buffer << endl;
        } else {
            cout << "Server disconnected." << endl;
            break;
        }
    }
    close(sock);
    return 0;
}
