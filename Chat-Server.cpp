#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <string>
#include <algorithm>
#pragma comment(lib, "ws2_32.lib")
#include "database.h"

using namespace std;

class ClientInfo {
    string username;
    SOCKET socket;
    string username_inconvo;
    sockaddr_in address;
public:
    ClientInfo(SOCKET socket, sockaddr_in address) {
        this->socket = socket;
        this->address = address;
    }
    void setUsername(const string& username) {
        this->username = username;
    }
    SOCKET getSocket() const {
        return socket;
    }
    string getUsername() const {
        return username;
    }
    void setUsernameInConvo(const string& username) {
        this->username_inconvo = username;
    }
    void emptyUsernameInConvo() {
        this->username_inconvo = "";
    }
    string getUsernameInConvo() const {
        return username_inconvo;
    }
};

// Store ClientInfo as pointers
vector<ClientInfo*> active_clients;
mutex client_mutex;

void broadcast_message(const string& message, SOCKET sender_socket) {
    lock_guard<mutex> lock(client_mutex);
    for (const auto& client : active_clients) {
        if (client->getSocket() != sender_socket) {
            send(client->getSocket(), message.c_str(), message.length(), 0);
        }
    }
}

void handle_client(ClientInfo* client, Database& db) {
    char buffer[1024];
    string message_buffer;
    bool is_registered = false;

    while (!is_registered) {
        string login_prompt = "Type /register if you are new, else /login if you have already registered.\n";
        send(client->getSocket(), login_prompt.c_str(), login_prompt.length(), 0);
        char buffer[1024];
        int bytes_received = recv(client->getSocket(), buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0) {
            cerr << "Client " << client->getSocket() << " disconnected before authentication.\n";
            closesocket(client->getSocket());
            delete client; // Free memory
            return;
        }
        buffer[bytes_received] = '\0';
        string command(buffer);

        if (command.rfind("/register", 0) == 0) {
            string username_prompt = "Please enter your username\n";
            send(client->getSocket(), username_prompt.c_str(), username_prompt.length(), 0);
            char username_buffer[1024];
            int bytes_received = recv(client->getSocket(), username_buffer, sizeof(username_buffer) - 1, 0);
            if (bytes_received <= 0) {
                cerr << "Client " << client->getSocket() << " disconnected before authentication.\n";
                closesocket(client->getSocket());
                delete client;
                return;
            }
            username_buffer[bytes_received] = '\0';
            string username(username_buffer);

            string password_prompt = "Please enter your password\n";
            send(client->getSocket(), password_prompt.c_str(), password_prompt.length(), 0);
            char password_buffer[10240];
            int bytes_recieved = recv(client->getSocket(), password_buffer, sizeof(password_buffer) - 1, 0);
            if (bytes_recieved <= 0) {
                cerr << "Client " << client->getSocket() << " disconnected before authentication\n";
                closesocket(client->getSocket());
                delete client;
                return;
            }
            password_buffer[bytes_recieved] = '\0';
            string password(password_buffer);

            if (db.registerUser(username, password)) {
                username.erase(0, username.find_first_not_of(" \t\r\n")); // Remove leading spaces
                username.erase(username.find_last_not_of(" \t\r\n") + 1); // Remove trailing spaces
                client->setUsername(username);
                is_registered = true;
            }
            else {
                string error_message = "Username already exists. Please try again.\n";
                send(client->getSocket(), error_message.c_str(), error_message.length(), 0);
            }
        }
        else if (command.rfind("/login", 0) == 0) {
            string username_prompt = "Enter your username:\n";
            send(client->getSocket(), username_prompt.c_str(), username_prompt.length(), 0);
            char username_buffer[1024];
            int bytes_received = recv(client->getSocket(), username_buffer, sizeof(username_buffer) - 1, 0);
            if (bytes_received <= 0) {
                cerr << "Client " << client->getSocket() << " disconnected before authentication.\n";
                closesocket(client->getSocket());
                delete client;
                return;
            }
            username_buffer[bytes_received] = '\0';
            string username(username_buffer);

            string password_prompt = "Enter your password:\n";
            send(client->getSocket(), password_prompt.c_str(), password_prompt.length(), 0);
            char password_buffer[1024];
            bytes_received = recv(client->getSocket(), password_buffer, sizeof(password_buffer) - 1, 0);
            if (bytes_received <= 0) {
                cerr << "Client " << client->getSocket() << " disconnected before authentication.\n";
                closesocket(client->getSocket());
                delete client;
                return;
            }
            password_buffer[bytes_received] = '\0';
            string password(password_buffer);

            if (db.loginUser(username, password)) {
                username.erase(0, username.find_first_not_of(" \t\r\n")); // Remove leading spaces
                username.erase(username.find_last_not_of(" \t\r\n") + 1); // Remove trailing spaces
                client->setUsername(username);
                is_registered = true;
            }
            else {
                string error_message = "Invalid username or password. Please try again.\n";
                send(client->getSocket(), error_message.c_str(), error_message.length(), 0);
            }
        }
    }

    cout << "Database operations were successful\n" << endl;
    string welcome_message = "Client " + to_string(client->getSocket()) + " has joined the chat.\n";
    broadcast_message(welcome_message, client->getSocket());

    {
        lock_guard<mutex> lock(client_mutex);
        active_clients.push_back(client);
    }

    while (true) {
        int bytes_received = recv(client->getSocket(), buffer, sizeof(buffer) - 1, 0);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            message_buffer += buffer;

            size_t pos = 0;
            while ((pos = message_buffer.find('\n')) != string::npos) {
                string message = message_buffer.substr(0, pos);
                message_buffer.erase(0, pos + 1);

                if (message.rfind("/whisper ", 0) == 0) {
                    string target_username = message.substr(9); // Extract username after "/whisper "

                    lock_guard<mutex> lock(client_mutex);
                    auto target_it = find_if(active_clients.begin(), active_clients.end(),
                        [&target_username](ClientInfo* c) { return c->getUsername() == target_username; });
                    if (target_username == client->getUsername()) {
                        string error_message = "You cannot have a private conversation with yourself";
                        send(client->getSocket(), error_message.c_str(), error_message.length(), 0);
                    }
                    else if (target_it != active_clients.end()) {
                        client->setUsernameInConvo(target_username);
                        (*target_it)->setUsernameInConvo(client->getUsername());

                        string confirm_message = "You are now in a private conversation with " + target_username + ". Type /exit to leave.\n";
                        send(client->getSocket(), confirm_message.c_str(), confirm_message.length(), 0);

                        string target_notify = client->getUsername() + " has started a private chat with you. Type /exit to leave.\n";
                        send((*target_it)->getSocket(), target_notify.c_str(), target_notify.length(), 0);
                    }
                    else {
                        string error_msg = "User '" + target_username + "' not found.\n";
                        send(client->getSocket(), error_msg.c_str(), error_msg.length(), 0);
                    }
                }
                else if (message == "/exit") {
					string inConvo = client->getUsernameInConvo();
					if (!inConvo.empty()) {
						lock_guard<mutex> lock(client_mutex);
						auto target_it = find_if(active_clients.begin(), active_clients.end(),
							[&inConvo](ClientInfo* c) { return c->getUsername() == inConvo; });
						if (target_it != active_clients.end()) {
							string exit_msg = "You left the private chat.\n";
							send(client->getSocket(), exit_msg.c_str(), exit_msg.length(), 0);
							string target_exit_msg = client->getUsername() + " has left the private chat.\n";
							send((*target_it)->getSocket(), target_exit_msg.c_str(), target_exit_msg.length(), 0);
							client->emptyUsernameInConvo();
							(*target_it)->emptyUsernameInConvo();
						}
					}
                }
                else {
                    string target_user = client->getUsernameInConvo();
                    if (!target_user.empty()) {
                        lock_guard<mutex> lock(client_mutex);
                        auto target_it = find_if(active_clients.begin(), active_clients.end(),
                            [&target_user](ClientInfo* c) { return c->getUsername() == target_user; });

                        if (target_it != active_clients.end()) {
                            string private_msg = client->getUsername() + " (private): " + message + "\n";
                            send((*target_it)->getSocket(), private_msg.c_str(), private_msg.length(), 0);
                        }
                    }
                    else {
                        string public_msg = client->getUsername() + ": " + message + "\n";
                        broadcast_message(public_msg, client->getSocket());
                    }
                }

            }
        }

        else {
            string disconnect_message = "Client " + to_string(client->getSocket()) + " has left the chat.\n";
            cout << disconnect_message;
            broadcast_message(disconnect_message, client->getSocket());

            closesocket(client->getSocket());

            lock_guard<mutex> lock(client_mutex);
            active_clients.erase(remove_if(active_clients.begin(), active_clients.end(),
                [client](ClientInfo* c) { return c == client; }), active_clients.end());

            delete client; // Free memory
            break;
        }
    }
}

int main(int argc, char** argv) {
    if (argc < 5) {
        cout << "Usage: " << argv[0] << " <port number>" << endl;
        return 0;
    }

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "WSAStartup failed!" << endl;
        return 1;
    }

    int port = atoi(argv[1]);
    SOCKET server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == INVALID_SOCKET) {
        cerr << "Socket creation failed!" << endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in server_address = {};
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = INADDR_ANY;

    if (::bind(server_socket, (struct sockaddr*)&server_address, sizeof(server_address)) == SOCKET_ERROR) {
        cerr << "Could not bind!" << endl;
        return 1;
    }

    listen(server_socket, SOMAXCONN);
    Database db(argv[2], argv[3], argv[4], "chat");

    while (true) {
        sockaddr_in remote_address = {};
        int addr_len = sizeof(remote_address);
        SOCKET client_socket = accept(server_socket, (struct sockaddr*)&remote_address, &addr_len);
        ClientInfo* new_client = new ClientInfo(client_socket, remote_address);

        thread client_thread(handle_client, new_client, ref(db));
        client_thread.detach();
    }

    return 0;
}
