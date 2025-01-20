#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <ctime>
#include <cstdlib>
using namespace std;
//Token
string generateToken() {
    srand(time(0));
    string token = to_string(rand());
    return token;
}
//Hash
string generateHash(string pass) {
    string hash = "";
    for (char c : pass) {
        hash += to_string((int)c + 7);
    }
    return hash;
}

//Username Check
bool userNameExists(string user) {
    ifstream userFile("user_tokens.txt");
    string username, token;
    while (userFile >> username >> token) {
        if (username == user) {
            return true;
        }
    }
    return false;
}

//Get Token -Username
int getToken(string username) {
    ifstream userFile("user_tokens.txt");
    string user, token;
    while (userFile >> user >> token) {
        if (user == username) {
            return stoi(token);
        }
    }
    return -1;
}

//Store the username and password 
bool storeUser(string user, string pass) {
    if (userNameExists(user)) {
        cout << "Username already exists!" << endl;
        return false;
    }

    //Generate and store token
    string token = generateToken();
    ofstream userFile("user_tokens.txt", ios::app);
    userFile << user << " " << token << endl;
    userFile.close();

    //Store hashed password and token
    ofstream passFile("token_passwords.txt", ios::app);
    passFile << token << " " << generateHash(pass) << endl;
    passFile.close();

    cout << "Signup successful!" << endl;
    return true;
}

//Check Pass
bool checkPass(int token, string hash) {
    ifstream passFile("token_passwords.txt");
    string storedToken, storedHash;
    while (passFile >> storedToken >> storedHash) {
        if (to_string(token) == storedToken && hash == storedHash) {
            return true;
        }
    }
    return false;
}

//Signup
void clientSignup() {
    string user, pass;
    cout << "Enter Username: ";
    cin >> user;
    cout << "Enter Password: ";
    cin >> pass;

    if (storeUser(user, pass)) {
        cout << "Signup successful. You can now login." << endl;
    } else {
        cout << "Signup failed. Try again." << endl;
    }
}

//Login
void clientLogin() {
    string user, pass;
    cout << "Enter Username: ";
    cin >> user;
    cout << "Enter Password: ";
    cin >> pass;

    int token = getToken(user);
    if (token != -1) {
        if (checkPass(token, generateHash(pass))) {
            cout << "Login successful!" << endl;
            int choice;
            cout << "1. Create Chatroom\n2. Join Chatroom\n";
            cin >> choice;

            if (choice == 1) {
                cout << "Feature under construction." << endl;
            } else if (choice == 2) {
                cout << "Feature under construction." << endl;
            } else {
                cout << "Invalid choice." << endl;
            }
        } else {
            cout << "Invalid password!" << endl;
        }
    } else {
        cout << "Username not found!" << endl;
    }
}
void client() {
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
            client();
            break;
    }
}

int main() {
    client();
    return 0;
}
