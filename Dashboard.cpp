#include<iostream>
#include<istream>
using namespace std;
bool userNameExists(string user){
    return false;
}
bool generateToken(string user , string *pass){
    if(!userNameExists(user)){
        
    }
    else{
        cout << "Username Exists!" << endl;
        return false;
    }
    return true;
}
string generateHash(string *s){
    return "0";
}

bool checkPass(int token , string hash){
    
    return false;
}
int getToken(string username){
    return -1;
}
void clientSignup(){
    string user,pass;
    cout << "Enter Username : ";
    cin >> user;
    cout << endl << "Enter Password : ";
    cin >> pass;
    cout << endl;


}
void clientLogin(){
    string user,pass;
    cout << "Enter Username : ";
    cin >> user;
    cout << endl << "Enter Password : ";
    cin >> pass;
    cout << endl;

    if(getToken(user) != -1){
        checkPass(getToken(user) , generateHash(&pass));
    }    
}
void client(){
    int choice;
    cout << "Enter Your Choice" << endl;
    cout << "1. Login" << endl << "2. Signup" << endl;
    cin >> choice;
    switch (choice)
    {
    case 1:
        clientLogin();
        break;
    case 2:
        clientSignup();
        break;
    default:
        cout << "Wrong input. Try again!" << endl;
        client();
        break;
    }
}
int main(){
    client();
    return 0;
}