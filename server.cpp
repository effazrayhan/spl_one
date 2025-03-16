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
#include "endec.h"

#define CREATE_ROOM_CMD "CREATE_ROOM"
#define JOIN_ROOM_CMD "JOIN_ROOM"
#define REGISTER_CMD "REGISTER"
#define LOGIN_CMD "LOGIN"
#define HISTORY_CMD "*old_chats"
#define PEERS_CMD "*active_peers"
using namespace std;

#define PORT 8080
#define HOST "192.168.0.111"
#define MAX_CLIENTS 10

void log_message(const string& message) {
    cout << "[SERVER " << time(nullptr) << "]: " << message << endl;
}

struct ChatRoom {
    string name;
    string token;
    map<int, string> members;
    unique_ptr<mutex> room_mutex;

    ChatRoom() : room_mutex(make_unique<mutex>()) {}
    ChatRoom(ChatRoom&& other) noexcept
        : name(move(other.name)),
          token(move(other.token)),
          members(move(other.members)),
          room_mutex(move(other.room_mutex)) {}
    ChatRoom(const ChatRoom&) = delete;
    ChatRoom& operator=(const ChatRoom&) = delete;
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

map<string, ChatRoom> chatrooms;
mutex chatrooms_mutex;

vector<int> clients;
mutex clients_mutex;
map<int, string> client_usernames;

bool authenticate_user(const string& username, const string& password_hash) {
    ifstream tokenFile("user_tokens.txt");
    if (!tokenFile.is_open()) {
        log_message("Failed to open user_tokens.txt");
        return false;
    }

    string user, user_token;
    bool found = false;
    while (tokenFile >> user >> user_token) {
        if (user == username) {
            found = true;
            break;
        }
    }
    tokenFile.close();
    
    if (!found) {
        log_message("Username not found: " + username);
        return false;
    }
    
    ifstream passFile("token_passwords.txt");
    if (!passFile.is_open()) {
        log_message("Failed to open token_passwords.txt");
        return false;
    }

    string token, stored_hash;
    while (passFile >> token >> stored_hash) {
        if (token == user_token) {
            passFile.close();
            bool auth_result = (stored_hash == password_hash);
            log_message("User auth for " + username + ": " + 
                       (auth_result ? "SUCCESS" : "FAILED") + 
                       " (stored: " + stored_hash + ", received: " + password_hash + ")");
            return auth_result;
        }
    }
    passFile.close();
    log_message("Token not found in password file for: " + username);
    return false;
}

bool register_user(const string& username, const string& password_hash) {
    log_message("Registering user: " + username);
    
    ifstream userCheck("user_tokens.txt");
    if (!userCheck.is_open()) {
        log_message("Creating new user_tokens.txt");
    } else {
        string existing_user, token;
        while (userCheck >> existing_user >> token) {
            if (existing_user == username) {
                userCheck.close();
                log_message("Username already exists: " + username);
                return false;
            }
        }
        userCheck.close();
    }

    string new_token = to_string(time(0)) + to_string(rand());
    
    ofstream userFileOut("user_tokens.txt", ios::app);
    if (!userFileOut.is_open()) {
        log_message("Failed to open user_tokens.txt for writing");
        return false;
    }
    userFileOut << username << " " << new_token << endl;
    userFileOut.close();

    ofstream passFileOut("token_passwords.txt", ios::app);
    if (!passFileOut.is_open()) {
        log_message("Failed to open token_passwords.txt for writing");
        return false;
    }
    passFileOut << new_token << " " << password_hash << endl;
    passFileOut.close();

    log_message("User registered successfully: " + username);
    return true;
}

void remove_client(int client_socket, const string& room_token = "") {
    lock_guard<mutex> lock(clients_mutex);
    clients.erase(remove(clients.begin(), clients.end(), client_socket), clients.end());
    client_usernames.erase(client_socket);
    
    if (!room_token.empty()) {
        lock_guard<mutex> room_lock(chatrooms_mutex);
        if (chatrooms.find(room_token) != chatrooms.end()) {
            chatrooms[room_token].members.erase(client_socket);
            if (chatrooms[room_token].members.empty()) {
                chatrooms.erase(room_token);
                log_message("Room " + room_token + " deleted (no members)");
            }
        }
    }
    close(client_socket);
    log_message("Client removed: " + to_string(client_socket));
}

string generate_room_token() {
    return to_string(rand() % 9000 + 1000);
}

bool create_chatroom(const string& room_name, const string& password_hash, string& room_token) {
    lock_guard<mutex> lock(chatrooms_mutex);
    
    room_token = generate_room_token();
    
    ofstream roomFile("room_tokens.txt", ios::app);
    if (!roomFile.is_open()) {
        log_message("Failed to open room_tokens.txt");
        return false;
    }
    roomFile << room_name << " " << room_token << endl;
    roomFile.close();
    
    ofstream passFile("room_passwords.txt", ios::app);
    if (!passFile.is_open()) {
        log_message("Failed to open room_passwords.txt");
        return false;
    }
    passFile << room_token << " " << password_hash << endl;
    passFile.close();
    
    ChatRoom room;
    room.name = room_name;
    room.token = room_token;
    chatrooms[room_token] = move(room);
    
    log_message("Chatroom created: " + room_name + " (Token: " + room_token + ")");
    return true;
}

bool authenticate_room(const string& room_token, const string& password_hash) {
    ifstream passFile("room_passwords.txt");
    if (!passFile.is_open()) {
        log_message("Failed to open room_passwords.txt for room auth");
        return false;
    }
    
    string token, stored_hash;
    string clean_room_token = room_token;
    string clean_password_hash = password_hash;
    
    // Clean the input strings
    if (clean_room_token.find_last_not_of(" \n\r\t") != string::npos) {
        clean_room_token = clean_room_token.substr(0, clean_room_token.find_last_not_of(" \n\r\t") + 1);
    }
    
    if (clean_password_hash.find_last_not_of(" \n\r\t") != string::npos) {
        clean_password_hash = clean_password_hash.substr(0, clean_password_hash.find_last_not_of(" \n\r\t") + 1);
    }
    
    while (passFile >> token >> stored_hash) {
        // Clean stored values
        if (token.find_last_not_of(" \n\r\t") != string::npos) {
            token = token.substr(0, token.find_last_not_of(" \n\r\t") + 1);
        }
        if (stored_hash.find_last_not_of(" \n\r\t") != string::npos) {
            stored_hash = stored_hash.substr(0, stored_hash.find_last_not_of(" \n\r\t") + 1);
        }
        
        if (token == clean_room_token) {
            passFile.close();
            bool result = (stored_hash == clean_password_hash);
            log_message("Room " + clean_room_token + " auth attempt:");
            log_message("Stored hash: [" + stored_hash + "]");
            log_message("Received hash: [" + clean_password_hash + "]");
            log_message("Result: " + string(result ? "SUCCESS" : "FAILED"));
            return result;
        }
    }
    passFile.close();
    log_message("Room token not found: " + clean_room_token);
    return false;
}

void broadcast_to_room(const string& message, int sender_fd, const string& room_token) {
    lock_guard<mutex> lock(chatrooms_mutex);
    if (chatrooms.find(room_token) == chatrooms.end()) {
        log_message("Room not found for broadcast: " + room_token);
        return;
    }
    
    ChatRoom& room = chatrooms[room_token];
    lock_guard<mutex> room_lock(*room.room_mutex);
    
    string sender_name = room.members[sender_fd];
    string formatted_message = sender_name + ": " + message;
    
    // Store message without extra newlines
    string history_file = "chatroom_" + room_token + ".txt";
    string enc_msg = encrypt(formatted_message);
    string encrypted_message = stringToHex(enc_msg);
    
    ofstream history(history_file, ios::app);
    if (history.is_open()) {
        history << encrypted_message << endl;  // Use endl for consistent line endings
        history.close();
    }
    
    // Add single newline for display
    formatted_message += "\n";
    for (const auto& member : room.members) {
        send(member.first, formatted_message.c_str(), formatted_message.length(), 0);
    }
    log_message("Broadcast to room " + room_token + ": " + message);
}

void send_room_history(int client_socket, const string& room_token) {
    string history_file = "chatroom_" + room_token + ".txt";
    ifstream history(history_file);
    if (!history.is_open()) {
        string msg = "No chat history available for this room.\n";
        send(client_socket, msg.c_str(), msg.length(), 0);
        log_message("No history file for room: " + room_token);
        return;
    }

    string history_block = "\n=== Room " + room_token + " History ===\n";
    send(client_socket, history_block.c_str(), history_block.length(), 0);

    string line;
    while (getline(history, line)) {
        if (!line.empty()) {
            try {
                string dec = hexToString(line);
                string decrypted = decrypt(dec);
                // Add single newline for display
                decrypted += "\n";
                send(client_socket, decrypted.c_str(), decrypted.length(), 0);
            } catch (const exception& e) {
                log_message("Error decrypting message in history: " + string(e.what()));
                continue;
            }
        }
    }
    
    history_block = "=== End of History ===\n";  // Removed extra newline
    send(client_socket, history_block.c_str(), history_block.length(), 0);
    history.close();
    log_message("Sent history for room: " + room_token);
}

void send_active_room_members(int client_socket, const string& room_token) {
    lock_guard<mutex> lock(chatrooms_mutex);
    if (chatrooms.find(room_token) == chatrooms.end()) {
        log_message("Room not found for member list: " + room_token);
        return;
    }
    
    ChatRoom& room = chatrooms[room_token];
    string members_msg = "\n=== Active Room Members ===\n";
    
    for (const auto& member : room.members) {
        members_msg += "- " + member.second + "\n";
    }
    
    members_msg += "=== Total Members: " + to_string(room.members.size()) + " ===\n\n";
    send(client_socket, members_msg.c_str(), members_msg.length(), 0);
    log_message("Sent member list for room: " + room_token);
}

void handle_client(int client_socket) {
    char buffer[1024];
    memset(buffer, 0, 1024);
    int recv_bytes = recv(client_socket, buffer, 1024, 0);
    
    if (recv_bytes <= 0) {
        close(client_socket);
        log_message("Failed to receive initial message from client: " + to_string(client_socket));
        return;
    }
    
    string request(buffer);
    log_message("Received request: " + request);
    stringstream ss(request);
    string command, username, password_hash;
    ss >> command >> username >> password_hash;

    if (command == REGISTER_CMD) {
        bool success = register_user(username, password_hash);
        string response = success ? "Registration successful" : "Username already exists";
        send(client_socket, response.c_str(), response.length(), 0);
        usleep(100000);
        close(client_socket);
        return;
    }
    else if (command == LOGIN_CMD) {
        if (!authenticate_user(username, password_hash)) {
            string error_msg = "Authentication failed!";
            send(client_socket, error_msg.c_str(), error_msg.length(), 0);
            usleep(100000);
            close(client_socket);
            return;
        }

        {
            lock_guard<mutex> lock(clients_mutex);
            clients.push_back(client_socket);
            client_usernames[client_socket] = username;
        }
        
        string welcome_msg = "Welcome " + username + "!\n";
        send(client_socket, welcome_msg.c_str(), welcome_msg.length(), 0);
        
        string options_msg = "\n1. Create Chatroom\n2. Join Chatroom\nChoice: ";
        send(client_socket, options_msg.c_str(), options_msg.length(), 0);
        
        memset(buffer, 0, 1024);
        recv(client_socket, buffer, 1024, 0);
        string choice = buffer;
        log_message(username + " chose option: " + choice);
        
        string current_room_token;

        if (choice[0] == '1') {
            send(client_socket, "Enter room name: ", 16, 0);
            memset(buffer, 0, 1024);
            recv(client_socket, buffer, 1024, 0);
            string room_name = buffer;
            
            send(client_socket, "Enter room password: ", 20, 0);
            memset(buffer, 0, 1024);
            recv(client_socket, buffer, 1024, 0);
            string room_password_hash = buffer;  // Already hashed by client
            
            string room_token;
            if (create_chatroom(room_name, room_password_hash, room_token)) {
                string success_msg = "Room created! Token: " + room_token + "\n";
                send(client_socket, success_msg.c_str(), success_msg.length(), 0);
                
                chatrooms[room_token].members[client_socket] = username;
                current_room_token = room_token;
            } else {
                string fail_msg = "Room creation failed!\n";
                send(client_socket, fail_msg.c_str(), fail_msg.length(), 0);
                remove_client(client_socket);
                return;
            }
        }
        else if (choice[0] == '2') {
            send(client_socket, "Enter Room ID (4 digits): ", 25, 0);
            memset(buffer, 0, 1024);
            recv(client_socket, buffer, 1024, 0);
            string room_token = string(buffer);
            room_token.erase(room_token.find_last_not_of(" \n\r\t") + 1);  // Clean input
            
            send(client_socket, "Enter room password: ", 20, 0);
            memset(buffer, 0, 1024);
            recv(client_socket, buffer, 1024, 0);
            string room_password_hash = string(buffer);
            room_password_hash.erase(room_password_hash.find_last_not_of(" \n\r\t") + 1);  // Clean input
            
            if (authenticate_room(room_token, room_password_hash)) {
                lock_guard<mutex> lock(chatrooms_mutex);
                // chatrooms.find(room_token) != chatrooms.end()
                if (1) {
                    chatrooms[room_token].members[client_socket] = username;
                    current_room_token = room_token;
                    
                    string join_msg = "Joined room " + room_token + "!\n";
                    send(client_socket, join_msg.c_str(), join_msg.length(), 0);
                    send_room_history(client_socket, room_token);
                } else {
                    string fail_msg = "Room doesn't exist!\n";
                    send(client_socket, fail_msg.c_str(), fail_msg.length(), 0);
                    remove_client(client_socket);
                    return;
                }
            } else {
                string fail_msg = "Room authentication failed!\n";
                send(client_socket, fail_msg.c_str(), fail_msg.length(), 0);
                remove_client(client_socket);
                return;
            }
        }
        
        while (!current_room_token.empty()) {
            memset(buffer, 0, 1024);
            int bytes_received = recv(client_socket, buffer, 1024, 0);
            if (bytes_received <= 0) {
                string exit_msg = username + " has left the room";  // Removed period and newline
                broadcast_to_room(exit_msg, client_socket, current_room_token);
                break;
            }
            //Debug
            log_message(buffer);
            string message = hexToString(string(buffer));
            message = decrypt(message);
            // Clean any trailing whitespace and newlines
            message = message.substr(0, message.find_last_not_of(" \n\r\t"));
            //int len = message.size();
            log_message("Received message in room " + current_room_token + ": " + message );
            //log_message(message.length());
            if (message == "quit") {
                string exit_msg = username + " has left the room";  // Removed period and newline
                broadcast_to_room(exit_msg, client_socket, current_room_token);
                break;
            }
            
            if (message == HISTORY_CMD) {
                send_room_history(client_socket, current_room_token);
            }
            else if (message == PEERS_CMD) {
                send_active_room_members(client_socket, current_room_token);
            }
            else if (!message.empty()) {
                broadcast_to_room(message, client_socket, current_room_token);
            }
            //usleep(10000);
        }
        
        remove_client(client_socket, current_room_token);
        return;
    }
}

int main() {
    srand(time(0));
    
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        log_message("Failed to create socket");
        return 1;
    }
    
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        log_message("Bind failed");
        return 1;
    }
    
    if (listen(server_socket, MAX_CLIENTS) == -1) {
        log_message("Listen failed");
        return 1;
    }
    
    log_message("Server started on port " + to_string(PORT));
    
    while (true) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
        
        if (client_socket == -1) {
            log_message("Accept failed");
            continue;
        }
        
        log_message("New client connected: " + to_string(client_socket));
        thread(handle_client, client_socket).detach();
    }
    
    close(server_socket);
    return 0;
}
