#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <signal.h>
#include "sha.h"
#include "endec.h"

#define PORT 8080
#define HOST "192.168.0.111"

using namespace std;

// Global variables
int client_socket = -1;
atomic<bool> is_running(false);
thread receive_thread;

// Network functions
void disconnect() {
    if (is_running) {
        is_running = false;
        if (receive_thread.joinable()) {
            receive_thread.join();
        }
    }
    if (client_socket != -1) {
        close(client_socket);
        client_socket = -1;
    }
}

void receive_messages() {
    char buffer[1024];
    while (is_running) {
        memset(buffer, 0, sizeof(buffer));
        int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
        if (bytes_received > 0) {
            string message(buffer);
            if (!message.empty()) {
                cout << message;
                if (message.back() != '\n') {
                    cout << endl;
                }
            }
        }
    }
}

bool connect_to_server() {
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        cerr << "Socket creation failed\n";
        return false;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    
    if (inet_pton(AF_INET, HOST, &server_addr.sin_addr) <= 0) {
        cerr << "Invalid address\n";
        return false;
    }

    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        cerr << "Connection failed\n";
        return false;
    }

    return true;
}

bool register_user(const string& username, const string& password_hash) {
    if (!connect_to_server()) {
        cerr << "Failed to connect to server\n";
        return false;
    }

    string register_msg = "REGISTER " + username + " " + password_hash + "\n";
    cout << "Sending registration request...\n";
    
    if (send(client_socket, register_msg.c_str(), register_msg.length(), 0) <= 0) {
        cerr << "Failed to send registration request\n";
        disconnect();
        return false;
    }

    char response[1024] = {0};
    cout << "Waiting for server response...\n";
    
    int recv_size = recv(client_socket, response, sizeof(response), 0);
    if (recv_size <= 0) {
        cerr << "Failed to receive server response\n";
        disconnect();
        return false;
    }
    
    string resp_str(response);
    cout << "Server response: " << resp_str << endl;
    
    disconnect();
    return (resp_str.find("successful") != string::npos);
}

bool authenticate(const string& username, const string& password_hash) {
    string auth_msg = "LOGIN " + username + " " + password_hash + "\n";
    cout << "Sending login request...\n";
    
    if (send(client_socket, auth_msg.c_str(), auth_msg.length(), 0) <= 0) {
        cerr << "Failed to send login request\n";
        return false;
    }

    char response[1024] = {0};
    
    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
    
    int recv_size = recv(client_socket, response, sizeof(response), 0);
    if (recv_size <= 0) {
        cerr << "Failed to receive server response\n";
        return false;
    }
    
    string resp_str(response);
    cout << "Server response: '" << resp_str << "'" << endl;
    
    if (resp_str.find("Welcome") != string::npos) {
        is_running = true;
        receive_thread = thread(receive_messages);
        return true;
    }
    
    cerr << "Authentication failed with response: " << resp_str << endl;
    return false;
}

void send_chat_message(const string& message) {
    if (is_running) {
        send(client_socket, message.c_str(), message.length(), 0);
    }
}

void handle_room_chat() {
    string choice;
    cout << "Waiting for server options...\n";
    
    char buffer[1024];
    memset(buffer, 0, 1024);
    recv(client_socket, buffer, 1024, 0);
    cout << buffer;
    
    cin >> choice;
    send_chat_message(choice);
    
    if (choice == "1") {
        // Create room
        memset(buffer, 0, 1024);
        recv(client_socket, buffer, 1024, 0);
        cout << buffer;
        
        string room_name;
        cin >> room_name;
        send_chat_message(room_name);
        
        memset(buffer, 0, 1024);
        recv(client_socket, buffer, 1024, 0);
        cout << buffer;
        
        string pwd;
        cin >> pwd;
        string hashed_password = hashdata(pwd);  // Add newline
        cout << pwd << endl;
        cout << pwd.length() << endl;
        cout << hashed_password << endl;
        send_chat_message(hashed_password + "\n");
        
        // Get room token
        memset(buffer, 0, 1024);
        recv(client_socket, buffer, 1024, 0);
        cout << buffer;
    }
    else if (choice == "2") {
        // Join room
        memset(buffer, 0, 1024);
        recv(client_socket, buffer, 1024, 0);
        cout << buffer;
        cin.ignore();
        string token;
        getline(cin, token);
        send_chat_message(token + "\n");
    
        memset(buffer, 0, 1024);
        recv(client_socket, buffer, 1024, 0);
        cout << buffer;
        //cin.ignore();
        string pwd;
        getline(cin, pwd);
        cout << pwd << endl;
        cout << pwd.length() << endl;
        string hashed_password = hashdata(pwd); // << NO newline in hashdata input
        cout << hashed_password << endl;
        send_chat_message(hashed_password + "\n");
    
        memset(buffer, 0, 1024);
        recv(client_socket, buffer, 1024, 0);
        cout << buffer;
    }
    
    
    // Start chat loop
    string message;
    cout << "\nYou're in the chat room. Type 'quit' to exit.\n";
    cout << "Special commands: *old_chats, *active_peers\n\n";
    //cin.ignore(); // Clear any leftover newlines
    while (true) {
        getline(cin, message);
        if (message == "quit") {
            message = encrypt(message + "a");
            message = stringToHex(message);
            send_chat_message( message + " " );
            break;
        }
        if (!message.empty()) {
            message = encrypt(message + "a");
            message = stringToHex(message);
            send_chat_message(message);
            // Add small delay to prevent message flooding
            //usleep(10000);
        }
    }
}

void clientSignup() {
    string user, pass;
    cout << "Enter Username: ";
    cin >> user;
    cout << "Enter Password: ";
    cin >> pass;

    string password_hash = hashdata(pass);
    
    if (register_user(user, password_hash)) {
        cout << "Signup successful. You can now login." << endl;
    } else {
        cout << "Signup failed. Username may already exist." << endl;
    }
}

void clientLogin() {
    string user, pass;
    cout << "Enter Username: ";
    cin >> user;
    cout << "Enter Password: ";
    cin >> pass;

    string password_hash = hashdata(pass);
    
    if (connect_to_server()) {
        if (authenticate(user, password_hash)) {
            cout << "Connected to server!\n";
            handle_room_chat();
            disconnect();
        } else {
            cout << "Authentication failed!\n";
            disconnect();
        }
    } else {
        cout << "Failed to connect to server!\n";
    }
}

void Connecto() {
    int choice;
    cout << "Enter Your Choice" << endl;
    cout << "1. Login\n2. Signup\n";
    cin >> choice;
    
    switch (choice) {
        case 1:
            clientLogin();
            break;
        case 2:
            clientSignup();
            break;
        default:
            cout << "Invalid input. Try again!" << endl;
            Connecto();
            break;
    }
}

int main() {
    Connecto();
    return 0;
}
