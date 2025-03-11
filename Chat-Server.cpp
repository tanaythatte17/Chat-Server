#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <mutex>
#include <vector>
#include <string>
#include <algorithm>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

struct ClientInfo {
    SOCKET socket;
    sockaddr_in address;
};

vector<ClientInfo> active_clients;
mutex client_mutex;

void broadcast_message(const string& message, SOCKET sender_socket) {
    lock_guard<mutex> lock(client_mutex);
    for (const auto& client : active_clients) {
        if (client.socket != sender_socket) {
            send(client.socket, message.c_str(), message.length(), 0);
        }
    }
}

void handle_client(ClientInfo client) {
    char buffer[1024];
    string message_buffer;

    string welcome_message = "Client " + to_string(client.socket) + " has joined the chat.\n";
    broadcast_message(welcome_message, client.socket);
    cout << welcome_message;

    while (true) {
        int bytes_received = recv(client.socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            message_buffer += buffer;

            size_t pos = 0;
            while ((pos = message_buffer.find('\n')) != string::npos) {
                string message = message_buffer.substr(0, pos);
                message_buffer.erase(0, pos + 1);

                // Check if it's a whisper command
                if (message.rfind("/whisper ", 0) == 0) {
                    size_t first_space = message.find(' ', 9);
                    if (first_space != string::npos) {
                        string target_socket_str = message.substr(9, first_space - 9);
                        string whisper_message = message.substr(first_space + 1);

                        SOCKET target_socket = static_cast<SOCKET>(stoi(target_socket_str));

                        lock_guard<mutex> lock(client_mutex);
                        auto it = find_if(active_clients.begin(), active_clients.end(),
                            [target_socket](const ClientInfo& c) { return c.socket == target_socket; });

                        if (it != active_clients.end()) {
                            string formatted_message = "[Whisper from " + to_string(client.socket) + "]: " + whisper_message + "\n";
                            send(target_socket, formatted_message.c_str(), formatted_message.length(), 0);
                        }
                        else {
                            string error_msg = "User with socket " + target_socket_str + " not found.\n";
                            send(client.socket, error_msg.c_str(), error_msg.length(), 0);
                        }
                    }
                }
                else {
                    // Normal message broadcast
                    string formatted_message = "Client " + to_string(client.socket) + ": " + message + "\n";

                    cout << "\033[s"; // Save cursor position
                    cout << "\033[F"; // Move cursor up
                    cout << "\033[2K"; // Clear the line
                    cout << formatted_message; // Display the incoming message
                    cout << "\033[u"; // Restore cursor position
                    cout << flush;

                    broadcast_message(formatted_message, client.socket);
                }
            }
        }
        else if (bytes_received == 0 || WSAGetLastError() == WSAECONNRESET) {
            string disconnect_message = "Client " + to_string(client.socket) + " has left the chat.\n";
            cout << disconnect_message;
            broadcast_message(disconnect_message, client.socket);

            closesocket(client.socket);

            lock_guard<mutex> lock(client_mutex);
            active_clients.erase(remove_if(active_clients.begin(), active_clients.end(),
                [client](const ClientInfo& c) { return c.socket == client.socket; }), active_clients.end());
            break;
        }
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
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
        cerr << "Socket creation failed! Error: " << WSAGetLastError() << endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, (struct sockaddr*)&server_address, sizeof(server_address)) == SOCKET_ERROR) {
        cerr << "Could not bind! Error: " << WSAGetLastError() << endl;
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    if (listen(server_socket, SOMAXCONN) == SOCKET_ERROR) {
        cerr << "Could not listen! Error: " << WSAGetLastError() << endl;
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    cout << "Server listening on port " << port << "..." << endl;

    while (true) {
        sockaddr_in remote_address;
        memset(&remote_address, 0, sizeof(remote_address));
        int remote_addrlen = sizeof(remote_address);

        SOCKET client_socket = accept(server_socket, (struct sockaddr*)&remote_address, &remote_addrlen);
        if (client_socket == INVALID_SOCKET) {
            cerr << "Could not accept! Error: " << WSAGetLastError() << endl;
            continue;
        }

        ClientInfo new_client = { client_socket, remote_address };

        lock_guard<mutex> lock(client_mutex);
        active_clients.push_back(new_client);

        cout << "New client connected: " << client_socket << endl;
        thread client_thread(handle_client, new_client);
        client_thread.detach();
    }

    closesocket(server_socket);
    WSACleanup();

    return 0;
}
