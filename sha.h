#include <iostream>     
#include <fstream>     
#include <string>
#include <vector>           
#include <bitset>      
#include <sstream>     
#include <iomanip>     
#include <cstdlib>     
#include <ctime>       
#define MAX_LEN 200
#define ROTRIGHT(word, bits) (((word) >> (bits)) | ((word) << (32 - (bits))))
#define SSIG0(x) (ROTRIGHT(x, 7) ^ ROTRIGHT(x, 18) ^ ((x) >> 3))
#define SSIG1(x) (ROTRIGHT(x, 17) ^ ROTRIGHT(x, 19) ^ ((x) >> 10))
#define CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define BSIG0(x) (ROTRIGHT(x, 7) ^ ROTRIGHT(x, 18) ^ ((x) >> 3))
#define BSIG1(x) (ROTRIGHT(x, 17) ^ ROTRIGHT(x, 19) ^ ((x) >> 10))
#define EP0(x) (ROTRIGHT(x, 2) ^ ROTRIGHT(x, 13) ^ ROTRIGHT(x, 22))
#define EP1(x) (ROTRIGHT(x, 6) ^ ROTRIGHT(x, 11) ^ ROTRIGHT(x, 25))

using namespace std;

const bool show_block_state_add_1 = 0;
const bool show_distance_from_512bit = 0;
const bool show_padding_results = false;
const bool show_working_vars_for_t = 0;
const bool show_T1_calculation = false;
const bool show_T2_calculation = false;
const bool show_hash_segments = false;
const bool show_Wt = false;

vector<unsigned long> convert_to_binary(const string);
vector<unsigned long> pad_to_512bits(const vector<unsigned long>);
vector<unsigned long> resize_block(vector<unsigned long>);
string compute_hash(const vector<unsigned long>);
string hashdata(string s);
string show_as_hex(unsigned long);
void cout_block_state(vector<unsigned long>);
string show_as_binary(unsigned long);

// Hash function
string hashdata(string s) {
    vector<unsigned long> block;
    block = convert_to_binary(s);
    block = pad_to_512bits(block);
    block = resize_block(block);
    string hash = compute_hash(block);
    return hash;
}

vector<unsigned long> resize_block(vector<unsigned long> input) {
    vector<unsigned long> output(16);

    for (int i = 0; i < 64; i = i + 4) {
        bitset<32> temp(0);

        temp = (unsigned long)input[i] << 24;
        temp |= (unsigned long)input[i + 1] << 16;
        temp |= (unsigned long)input[i + 2] << 8;
        temp |= (unsigned long)input[i + 3];

        output[i / 4] = temp.to_ulong();
    }

    return output;
}

string show_as_hex(unsigned long input) {
    bitset<32> bs(input);
    unsigned n = bs.to_ulong();

    stringstream sstream;
    sstream << std::hex << std::setw(8) << std::setfill('0') << n;
    string temp;
    sstream >> temp;

    return temp;
}

string show_as_binary(unsigned long input) {
    bitset<8> bs(input);
    return bs.to_string();
}

vector<unsigned long> convert_to_binary(const string input) {
    vector<unsigned long> block;

    for (int i = 0; i < input.size(); ++i) {
        bitset<8> b(input.c_str()[i]);
        block.push_back(b.to_ulong());
    }

    return block;
}

vector<unsigned long> pad_to_512bits(vector<unsigned long> block) {
    int l = block.size() * 8;
    int k = 447 - l;
    if (show_block_state_add_1)
        cout_block_state(block);

    unsigned long t1 = 0x80;
    block.push_back(t1);

    if (show_block_state_add_1)
        cout_block_state(block);
    k = k - 7;

    if (show_distance_from_512bit) {
        cout << "l: " << l << endl;
        cout << "k: " << k + 7 << endl;
    }
    if (show_distance_from_512bit)
        cout << "adding " << k / 8 << " empty eight bit blocks!\n";
    for (int i = 0; i < k / 8; i++)
        block.push_back(0x00000000);

    bitset<64> big_64bit_blob(l);
    if (show_distance_from_512bit)
        cout << "l in a 64 bit binary blob: \n\t" << big_64bit_blob << endl;
    string big_64bit_string = big_64bit_blob.to_string();
    bitset<8> temp_string_holder1(big_64bit_string.substr(0, 8));
    block.push_back(temp_string_holder1.to_ulong());
    for (int i = 8; i < 63; i = i + 8) {
        bitset<8> temp_string_holder2(big_64bit_string.substr(i, 8));
        block.push_back(temp_string_holder2.to_ulong());
    }
    if (show_padding_results) {
        for (int i = 0; i < block.size(); i = i + 4)
            cout << i << ": " << show_as_binary(block[i]) << "     "
                 << i + 1 << ": " << show_as_binary(block[i + 1]) << "     "
                 << i + 2 << ": " << show_as_binary(block[i + 2]) << "     "
                 << i + 3 << ": " << show_as_binary(block[i + 3]) << endl;

        for (int i = 0; i < block.size(); i = i + 4)
            cout << i << ": " << "0x" + show_as_hex(block[i]) << "     "
                 << i + 1 << ": " << "0x" + show_as_hex(block[i + 1]) << "     "
                 << i + 2 << ": " << "0x" + show_as_hex(block[i + 2]) << "     "
                 << i + 3 << ": " << "0x" + show_as_hex(block[i + 3]) << endl;
    }
    return block;
}

string compute_hash(const vector<unsigned long> block) {
    unsigned long k[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
        0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
        0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
        0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
        0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
        0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
        0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
        0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
        0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
    };

    // Initialize hash values to SHA-256 constants for each call
    unsigned long H0 = 0x6a09e667;
    unsigned long H1 = 0xbb67ae85;
    unsigned long H2 = 0x3c6ef372;
    unsigned long H3 = 0xa54ff53a;
    unsigned long H4 = 0x510e527f;
    unsigned long H5 = 0x9b05688c;
    unsigned long H6 = 0x1f83d9ab;
    unsigned long H7 = 0x5be0cd19;

    unsigned long W[64];

    for (int t = 0; t <= 15; t++) {
        W[t] = block[t] & 0xFFFFFFFF;

        if (show_Wt)
            cout << "W[" << t << "]: 0x" << show_as_hex(W[t]) << endl;
    }

    for (int t = 16; t <= 63; t++) {
        W[t] = SSIG1(W[t - 2]) + W[t - 7] + SSIG0(W[t - 15]) + W[t - 16];
        W[t] = W[t] & 0xFFFFFFFF;
        if (show_Wt)
            cout << "W[" << t << "]: " << W[t];
    }

    unsigned long temp1;
    unsigned long temp2;
    unsigned long a = H0;
    unsigned long b = H1;
    unsigned long c = H2;
    unsigned long d = H3;
    unsigned long e = H4;
    unsigned long f = H5;
    unsigned long g = H6;
    unsigned long h = H7;

    if (show_working_vars_for_t)
        cout << "         A        B        C        D        "
             << "E        F        G        H        T1       T2\n";

    for (int t = 0; t < 64; t++) {
        temp1 = h + EP1(e) + CH(e, f, g) + k[t] + W[t];
        if ((t == 20) & show_T1_calculation) {
            cout << "h: 0x" << hex << h << "  dec:" << dec << h
                 << "  sign:" << dec << (int)h << endl;
            cout << "EP1(e): 0x" << hex << EP1(e) << "  dec:"
                 << dec << EP1(e) << "  sign:" << dec << (int)EP1(e)
                 << endl;
            cout << "CH(e,f,g): 0x" << hex << CH(e, f, g) << "  dec:"
                 << dec << CH(e, f, g) << "  sign:" << dec
                 << (int)CH(e, f, g) << endl;
            cout << "k[t]: 0x" << hex << k[t] << "  dec:" << dec
                 << k[t] << "  sign:" << dec << (int)k[t] << endl;
            cout << "W[t]: 0x" << hex << W[t] << "  dec:" << dec
                 << W[t] << "  sign:" << dec << (int)W[t] << endl;
            cout << "temp1: 0x" << hex << temp1 << "  dec:" << dec
                 << temp1 << "  sign:" << dec << (int)temp1 << endl;
        }
        temp2 = EP0(a) + MAJ(a, b, c);
        if ((t == 20) & show_T2_calculation) {
            cout << "a: 0x" << hex << a << "  dec:" << dec << a
                 << "  sign:" << dec << (int)a << endl;
            cout << "b: 0x" << hex << b << "  dec:" << dec << b
                 << "  sign:" << dec << (int)b << endl;
            cout << "c: 0x" << hex << c << "  dec:" << dec << c
                 << "  sign:" << dec << (int)c << endl;
            cout << "EP0(a): 0x" << hex << EP0(a) << "  dec:"
                 << dec << EP0(a) << "  sign:" << dec << (int)EP0(a)
                 << endl;
            cout << "MAJ(a,b,c): 0x" << hex << MAJ(a, b, c) << "  dec:"
                 << dec << MAJ(a, b, c) << "  sign:" << dec
                 << (int)MAJ(a, b, c) << endl;
            cout << "temp2: 0x" << hex << temp2 << "  dec:" << dec
                 << temp2 << "  sign:" << dec << (int)temp2 << endl;
        }
        h = g;
        g = f;
        f = e;
        e = (d + temp1) & 0xFFFFFFFF;
        d = c;
        c = b;
        b = a;
        a = (temp1 + temp2) & 0xFFFFFFFF;
        if (show_working_vars_for_t) {
            cout << "t= " << t << " ";
            cout << show_as_hex(a) << " " << show_as_hex(b) << " "
                 << show_as_hex(c) << " " << show_as_hex(d) << " "
                 << show_as_hex(e) << " " << show_as_hex(f) << " "
                 << show_as_hex(g) << " " << show_as_hex(h) << " "
                 << endl;
        }
    }
    if (show_hash_segments) {
        cout << "H0: " << show_as_hex(H0) << " + " << show_as_hex(a)
             << " " << show_as_hex(H0 + a) << endl;
        cout << "H1: " << show_as_hex(H1) << " + " << show_as_hex(b)
             << " " << show_as_hex(H1 + b) << endl;
        cout << "H2: " << show_as_hex(H2) << " + " << show_as_hex(c)
             << " " << show_as_hex(H2 + c) << endl;
        cout << "H3: " << show_as_hex(H3) << " + " << show_as_hex(d)
             << " " << show_as_hex(H3 + d) << endl;
        cout << "H4: " << show_as_hex(H4) << " + " << show_as_hex(e)
             << " " << show_as_hex(H4 + e) << endl;
        cout << "H5: " << show_as_hex(H5) << " + " << show_as_hex(f)
             << " " << show_as_hex(H5 + f) << endl;
        cout << "H6: " << show_as_hex(H6) << " + " << show_as_hex(g)
             << " " << show_as_hex(H6 + g) << endl;
        cout << "H7: " << show_as_hex(H7) << " + " << show_as_hex(h)
             << " " << show_as_hex(H7 + h) << endl;
    }

    // Compute final hash values
    H0 = (H0 + a) & 0xFFFFFFFF;
    H1 = (H1 + b) & 0xFFFFFFFF;
    H2 = (H2 + c) & 0xFFFFFFFF;
    H3 = (H3 + d) & 0xFFFFFFFF;
    H4 = (H4 + e) & 0xFFFFFFFF;
    H5 = (H5 + f) & 0xFFFFFFFF;
    H6 = (H6 + g) & 0xFFFFFFFF;
    H7 = (H7 + h) & 0xFFFFFFFF;

    return show_as_hex(H0) + show_as_hex(H1) + show_as_hex(H2) +
           show_as_hex(H3) + show_as_hex(H4) + show_as_hex(H5) +
           show_as_hex(H6) + show_as_hex(H7);
}

// Dummy implementation of cout_block_state (since it wasn’t provided)
void cout_block_state(vector<unsigned long> block) {
    for (size_t i = 0; i < block.size(); i++) {
        cout << "block[" << i << "]: " << show_as_hex(block[i]) << endl;
    }
}


