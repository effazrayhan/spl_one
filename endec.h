#include <iostream>
#include <string>
#include <vector>
using namespace std;

const int BLOCK_SIZE = 16;
string password = "oikireKIrekiOIKIREkiREKIMODHUmodhu";

// Generate key based on password
vector<unsigned char> generateKey(const string& pwd) {
    vector<unsigned char> key(16, 0);
    for (size_t i = 0; i < pwd.length(); i++) {
        key[i % 16] ^= pwd[i];
    }
    return key;
}

// XOR-based encryption (reversible)
string encrypt(const string& message) {
    string pwd = password;    
    vector<unsigned char> key = generateKey(pwd);
    string encrypted;

    // Pad message to be multiple of BLOCK_SIZE
    string padded = message;
    size_t padding = BLOCK_SIZE - (message.length() % BLOCK_SIZE);
    if (padding == 0) padding = BLOCK_SIZE; // Always pad at least one block
    padded.append(padding, char(padding));

    // Encrypt each character with key using simple XOR
    for (size_t i = 0; i < padded.size(); i++) {
        encrypted += padded[i] ^ key[i % 16];
    }
    return encrypted;
}

string decrypt(const string& encrypted) {
    string pwd = password;    
    vector<unsigned char> key = generateKey(pwd);
    string decrypted;

    // Decrypt by XOR again (same operation)
    for (size_t i = 0; i < encrypted.size(); i++) {
        decrypted += encrypted[i] ^ key[i % 16];
    }

    // Remove padding
    if (!decrypted.empty()) {
        unsigned char padding = decrypted.back();
        if (padding > 0 && padding <= BLOCK_SIZE && padding <= decrypted.size()) {
            decrypted.erase(decrypted.size() - padding);
        }
    }

    return decrypted;
}

// Convert string to hex representation
string stringToHex(const string& input) {
    static const char hex_digits[] = "0123456789ABCDEF";
    string output;
    for (unsigned char c : input) {
        output.push_back(hex_digits[c >> 4]);
        output.push_back(hex_digits[c & 15]);
    }
    return output;
}

// Convert hex to string
string hexToString(const string& hex) {
    string result;
    for (size_t i = 0; i < hex.length(); i += 2) {
        string byte = hex.substr(i, 2);
        char chr = (char)(int)strtol(byte.c_str(), nullptr, 16);
        result.push_back(chr);
    }
    return result;
}

// int main() {
//     string message;
//     int choice;

//     cout << "1. Encrypt\n2. Decrypt\nChoice: ";
//     cin >> choice;
//     cin.ignore();

//     if (choice == 1) {
//         cout << "Enter the message to encrypt: ";
//         getline(cin, message);
        
//         string encrypted = encrypt(message, password);
//         cout << "Encrypted message (hex): " << stringToHex(encrypted) << endl;
//     } else if (choice == 2) {
//         cout << "Enter the encrypted message (hex): ";
//         string hexInput;
//         getline(cin, hexInput);

//         string encrypted = hexToString(hexInput);
//         string decrypted = decrypt(encrypted, password);
//         cout << "Decrypted message: " << decrypted << endl;
//     }

//     return 0;
// }
