#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <mutex>
#include <thread>
#include <cstring> 
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <map>
#include <sstream>
#include <fstream>
#include "sha.h"

// command definitions
#define CREATE_ROOM_CMD "CREATE_ROOM"
#define JOIN_ROOM_CMD "JOIN_ROOM"
#define REGISTER_CMD "REGISTER"
#define LOGIN_CMD "LOGIN"
#define HISTORY_CMD "*old_chats"
#define PEERS_CMD "*active_peers"
using namespace std;

#define PORT 8080
//#define HOST "10.100.200.172"
#define HOST "192.168.0.111"
#define MAX_CLIENTS 10

// Modify the ChatRoom struct
struct ChatRoom {
    string name;
    string token;
    map<int, string> members; // socket -> username
    unique_ptr<mutex> room_mutex;

    // Add constructor
    ChatRoom() : room_mutex(make_unique<mutex>()) {}
    
    // Add move constructor
    ChatRoom(ChatRoom&& other) noexcept
        : name(move(other.name)),
          token(move(other.token)),
          members(move(other.members)),
          room_mutex(move(other.room_mutex)) {}
    
    // Delete copy constructor and assignment operator
    ChatRoom(const ChatRoom&) = delete;
    ChatRoom& operator=(const ChatRoom&) = delete;
    
    // Add move assignment operator
    ChatRoom& operator=(ChatRoom&& other) noexcept {
        if (this != &other) {
            name = move(other.name);
            token = move(other.token);
            members = move(other.members);
            room_mutex = move(other.room_mutex);
        }
        return *this;
    }
};

map<string, ChatRoom> chatrooms; // token -> room
mutex chatrooms_mutex;

vector<int> clients;
mutex clients_mutex;
map<int, string> client_usernames; // Maps socket to username

bool authenticate_user(const string& username, const string& password_hash) {
    ifstream userFile("token_passwords.txt");
    string token, stored_hash;
    
    // First get the token for the username
    ifstream tokenFile("user_tokens.txt");
    string user, user_token;
    bool found = false;
    while (tokenFile >> user >> user_token) {
        if (user == username) {
            found = true;
            break;
        }
    }
    
    if (!found) return false;
    
    // Now check the password hash
    while (userFile >> token >> stored_hash) {
        if (token == user_token && stored_hash == password_hash) {
            return true;
        }
    }
    return false;
}

bool register_user(const string& username, const string& password_hash) {
    cout << "Attempting to register user: " << username << endl;
    
    // Check if files exist, create if they don't
    ofstream userFile("user_tokens.txt", ios::app);
    ofstream passFile("token_passwords.txt", ios::app);
    userFile.close();
    passFile.close();

    // Check if username exists
    ifstream userCheck("user_tokens.txt");
    string existing_user, token;
    while (userCheck >> existing_user >> token) {
        if (existing_user == username) {
            cout << "Username already exists" << endl;
            return false;
        }
    }
    userCheck.close();

    // Generate new token with current time to ensure uniqueness
    string new_token = to_string(time(0)) + to_string(rand());

    // Store username and token
    ofstream userFileOut("user_tokens.txt", ios::app);
    if (!userFileOut.is_open()) {
        cerr << "Failed to open user_tokens.txt" << endl;
        return false;
    }
    userFileOut << username << " " << new_token << endl;
    userFileOut.close();

    // Store token and password hash
    ofstream passFileOut("token_passwords.txt", ios::app);
    if (!passFileOut.is_open()) {
        cerr << "Failed to open token_passwords.txt" << endl;
        return false;
    }
    passFileOut << new_token << " " << password_hash << endl;
    passFileOut.close();

    cout << "User registered successfully" << endl;
    return true;
}

// void broadcast(const string& message, int sender_fd) {
//     lock_guard<mutex> lock(clients_mutex);
//     string sender_name = client_usernames[sender_fd];
//     string formatted_message = sender_name + ": " + message;
    
//     // Save to chat history
//     ofstream history("ChatHistory.txt", ios::app);
//     if (history.is_open()) {
//         history << formatted_message;  // Write the message to file
//         history.close();
//     }

//     // Send to all clients except sender
//     for (int client : clients) {
//         if (client != sender_fd) {
//             send(client, formatted_message.c_str(), formatted_message.length(), 0);
//         }
//     }
// }

void remove_client(int client_socket) {
    lock_guard<mutex> lock(clients_mutex);
    clients.erase(remove(clients.begin(), clients.end(), client_socket), clients.end());
    client_usernames.erase(client_socket);
    close(client_socket);
}

void send_chat_history(int client_socket) {
    ifstream history("ChatHistory.txt");
    if (!history.is_open()) {
        string msg = "No chat history available.\n";
        send(client_socket, msg.c_str(), msg.length(), 0);
        return;
    }

    string line;
    string history_block = "\n=== Chat History ===\n";
    send(client_socket, history_block.c_str(), history_block.length(), 0);
    usleep(100000); // Small delay to ensure order

    while (getline(history, line)) {
        if (!line.empty()) {
            send(client_socket, line.c_str(), line.length(), 0);
            usleep(10000); // Small delay between messages
        }
    }
    
    history_block = "=== End of History ===\n\n";
    send(client_socket, history_block.c_str(), history_block.length(), 0);
    history.close();
}

void send_active_peers(int client_socket) {
    lock_guard<mutex> lock(clients_mutex);
    string peers_msg = "\n=== Active Users ===\n";
    
    for (const auto& pair : client_usernames) {
        peers_msg += "- " + pair.second + "\n";
    }
    
    peers_msg += "=== Total Users: " + to_string(client_usernames.size()) + " ===\n\n";
    send(client_socket, peers_msg.c_str(), peers_msg.length(), 0);
}

// Add new functions
string generate_room_token() {
    srand(time(0));
    int token = rand() % 9000 + 1000; // Generates number between 1000-9999
    return to_string(token);
}

// Modify create_chatroom function
bool create_chatroom(const string& room_name, const string& password_hash, string& room_token) {
    lock_guard<mutex> lock(chatrooms_mutex);
    
    // Generate room token
    room_token = generate_room_token();
    
    // Save room info
    ofstream roomFile("room_tokens.txt", ios::app);
    if (!roomFile.is_open()) return false;
    roomFile << room_name << " " << room_token << endl;
    roomFile.close();
    
    // Save room password
    ofstream passFile("room_passwords.txt", ios::app);
    if (!passFile.is_open()) return false;
    passFile << room_token << " " << password_hash << endl;
    passFile.close();
    
    // Create room in memory
    ChatRoom room;
    room.name = room_name;
    room.token = room_token;
    chatrooms.emplace(room_token, move(room));
    
    return true;
}

bool authenticate_room(const string& room_token, const string& password_hash) {
    ifstream passFile("room_passwords.txt");
    string token, stored_hash;
    
    while (passFile >> token >> stored_hash) {
        if (token == room_token && stored_hash == password_hash) {
            return true;
        }
    }
    return false;
}

// Fix the broadcast_to_room function
void broadcast_to_room(const string& message, int sender_fd, const string& room_token) {
    lock_guard<mutex> lock(chatrooms_mutex);
    if (chatrooms.find(room_token) == chatrooms.end()) return;
    
    ChatRoom& room = chatrooms[room_token];
    lock_guard<mutex> room_lock(*room.room_mutex);
    
    string sender_name = room.members[sender_fd];
    string formatted_message = "[Room " + room_token + "] " + sender_name + ": " + message;
    
    // Write to room history file first
    string history_file = "chatroom_" + room_token + ".txt";
    ofstream history(history_file, ios::app);
    if (history.is_open()) {
        history << formatted_message;
        history.close();
    }
    
    // Then broadcast to all members (including sender for confirmation)
    for (const auto& member : room.members) {
        send(member.first, formatted_message.c_str(), formatted_message.length(), 0);
    }
}

// Add function to send room history
void send_room_history(int client_socket, const string& room_token) {
    string history_file = "chatroom_" + room_token + ".txt";
    ifstream history(history_file);
    if (!history.is_open()) {
        string msg = "No chat history available for this room.\n";
        send(client_socket, msg.c_str(), msg.length(), 0);
        return;
    }

    string line;
    string history_block = "\n=== Room " + room_token + " History ===\n";
    send(client_socket, history_block.c_str(), history_block.length(), 0);

    while (getline(history, line)) {
        if (!line.empty()) {
            send(client_socket, line.c_str(), line.length(), 0);
            send(client_socket, "\n", 1, 0);
        }
    }
    
    history_block = "=== End of History ===\n\n";
    send(client_socket, history_block.c_str(), history_block.length(), 0);
    history.close();
}

// Add this new function to get active room members
void send_active_room_members(int client_socket, const string& room_token) {
    lock_guard<mutex> lock(chatrooms_mutex);
    if (chatrooms.find(room_token) == chatrooms.end()) return;
    
    ChatRoom& room = chatrooms[room_token];
    string members_msg = "\n=== Active Room Members ===\n";
    
    for (const auto& member : room.members) {
        members_msg += "- " + member.second + "\n";
    }
    
    members_msg += "=== Total Members: " + to_string(room.members.size()) + " ===\n\n";
    send(client_socket, members_msg.c_str(), members_msg.length(), 0);
}

void handle_client(int client_socket) {
    char buffer[1024];
    memset(buffer, 0, 1024);
    int recv_bytes = recv(client_socket, buffer, 1024, 0);
    
    if (recv_bytes <= 0) {
        close(client_socket);
        return;
    }
    
    string request(buffer);
    cout << "Received request: " << request << endl;
    
    stringstream ss(request);
    string command, username, password_hash;
    ss >> command >> username >> password_hash;

    cout << "Processing command: " << command << endl;

    if (command == REGISTER_CMD) {
        bool success = register_user(username, password_hash);
        string response = success ? "Registration successful" : "Username already exists";
        cout << "Registration result: " << response << endl;
        
        // Ensure response is sent with proper length
        send(client_socket, response.c_str(), response.length(), 0);
        
        // Wait a moment before closing connection
        usleep(100000); // 100ms delay
        close(client_socket);
        return;
    }
    else if (command == LOGIN_CMD) {
        if (!authenticate_user(username, password_hash)) {
            string error_msg = "Authentication failed!";
            send(client_socket, error_msg.c_str(), error_msg.length(), 0);
            usleep(100000); // Wait for message to be sent
            close(client_socket);
            return;
        }

        // Add client to active clients list
        {
            lock_guard<mutex> lock(clients_mutex);
            clients.push_back(client_socket);
            client_usernames[client_socket] = username;
        }
        
        // Send welcome message and wait for it to be sent
        string welcome_msg = "Welcome " + username + "!\n";
        send(client_socket, welcome_msg.c_str(), welcome_msg.length(), 0);
        // usleep(100000); // Wait for message to be sent
        
        cout << username << " logged in successfully" << endl;
        usleep(100000);
        
        // Send room options
        string options_msg = "\n1. Create Chatroom\n2. Join Chatroom\nChoice: ";
        send(client_socket, options_msg.c_str(), options_msg.length(), 0);
        
        // Get client's choice
        memset(buffer, 0, 1024);
        recv(client_socket, buffer, 1024, 0);
        string choice = buffer;
        
        string current_room_token = "";  // Add this variable to track current room

        if (choice[0] == '1') {
            // Create room
            send(client_socket, "Enter room name: ", 16, 0);
            memset(buffer, 0, 1024);
            recv(client_socket, buffer, 1024, 0);
            string room_name = buffer;
            
            send(client_socket, "Enter room password: ", 20, 0);
            memset(buffer, 0, 1024);
            recv(client_socket, buffer, 1024, 0);
            string room_password = buffer;
            
            string room_token;
            if (create_chatroom(room_name, room_password, room_token)) {
                string success_msg = "Room created! Token: " + room_token + "\n";
                send(client_socket, success_msg.c_str(), success_msg.length(), 0);
                
                chatrooms[room_token].members[client_socket] = username;
                current_room_token = room_token;  // Set current room
                
                // Handle room messages
                while (true) {
                    memset(buffer, 0, 1024);
                    int bytes_received = recv(client_socket, buffer, 1024, 0);
                    if (bytes_received <= 0) {
                        cout << "Client disconnected from room" << endl;
                        break;
                    }
                    
                    string message = buffer;
                    if (message == "quit\n") {
                        string exit_msg = username + " has left the room.\n";
                        broadcast_to_room(exit_msg, client_socket, current_room_token);
                        break;
                    }
                    
                    // Handle special commands
                    if (message.find(HISTORY_CMD) != string::npos) {
                        send_room_history(client_socket, current_room_token);
                        continue;
                    }
                    else if (message.find(PEERS_CMD) != string::npos) {
                        send_active_room_members(client_socket, current_room_token);
                        continue;
                    }
                    else {
                        broadcast_to_room(message, client_socket, current_room_token);
                        // Add small delay to prevent message flooding
                        usleep(10000);
                    }
                }
            }
        }
        else if (choice[0] == '2') {
            // Join room
            send(client_socket, "Enter Room ID (4 digits): ", 25, 0);
            memset(buffer, 0, 1024);
            recv(client_socket, buffer, 1024, 0);
            string room_token = string(buffer);
            
            send(client_socket, "Enter room password: ", 20, 0);
            memset(buffer, 0, 1024);
            recv(client_socket, buffer, 1024, 0);
            string room_password = buffer;
            
            if (authenticate_room(room_token, room_password)) {
                if (chatrooms.find(room_token) != chatrooms.end()) {
                    chatrooms[room_token].members[client_socket] = username;
                    current_room_token = room_token;  // Set current room
                    
                    string join_msg = "Joined room " + room_token + "!\n";
                    send(client_socket, join_msg.c_str(), join_msg.length(), 0);
                    
                    send_room_history(client_socket, room_token);
                    
                    // Handle room messages
                    while (true) {
                        memset(buffer, 0, 1024);
                        int bytes_received = recv(client_socket, buffer, 1024, 0);
                        if (bytes_received <= 0) {
                            cout << "Client disconnected from room" << endl;
                            break;
                        }
                        
                        string message = buffer;
                        if (message == "quit\n") {
                            string exit_msg = username + " has left the room.\n";
                            broadcast_to_room(exit_msg, client_socket, current_room_token);
                            break;
                        }
                        
                        // Handle special commands
                        if (message.find(HISTORY_CMD) != string::npos) {
                            send_room_history(client_socket, current_room_token);
                            continue;
                        }
                        else if (message.find(PEERS_CMD) != string::npos) {
                            send_active_room_members(client_socket, current_room_token);
                            continue;
                        }
                        else {
                            broadcast_to_room(message, client_socket, current_room_token);
                            // Add small delay to prevent message flooding
                            usleep(10000);
                        }
                    }
                }
            }
        }
        
        remove_client(client_socket);
        return;
    }
}

int main() {
    // Add seed for random number generator
    srand(time(0));
    
    int server_socket;
    struct sockaddr_in server_addr;
    
    // Create socket
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        cerr << "Failed to create socket\n";
        return 1;
    }
    
    // Set socket options
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        cerr << "Failed to set socket options\n";
        return 1;
    }
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    
    // Bind
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        cerr << "Bind failed\n";
        return 1;
    }
    
    // Listen
    if (listen(server_socket, MAX_CLIENTS) == -1) {
        cerr << "Listen failed\n";
        return 1;
    }
    
    cout << "Server started on port " << PORT << endl;
    
    while (true) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
        
        if (client_socket == -1) {
            cerr << "Accept failed\n";
            continue;
        }
        
        thread(handle_client, client_socket).detach();
        cout << "New client connected: " << client_socket << endl;
    }
    
    close(server_socket);
    return 0;
}
