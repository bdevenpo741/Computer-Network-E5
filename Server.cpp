#include <iostream>
#include <winsock2.h>  // For Windows Sockets
#include <thread>
#include <fstream>
#include "checksum.h"  //include the checksum error dection

#pragma comment(lib, "ws2_32.lib")  // Link Winsock library

//Server is listens to port 8080 for client connections
const int PORT = 8080;

const int BUFFER_SIZE = 1024;

void handle_put(SOCKET client_socket, std::string& filename) {
    char buffer[BUFFER_SIZE];
    std::ofstream outfile(filename, std::ios::binary);
    if (!outfile) {
        std::cerr << "Error creating file on server.\n";
        return;
    }

    // Receive the checksum from the client
    unsigned int client_checksum;
    recv(client_socket, (char*)&client_checksum, sizeof(client_checksum), 0);
    std::cout << "Checksum received from client: " << client_checksum << std::endl;

    // Receive the file
    int bytes_received = 0;
    while ((bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0) {
        outfile.write(buffer, bytes_received);
    }

    outfile.close();
    std::cout << "File '" << filename << "' received from client.\n";

    // Compute checksum of the received file
    unsigned int server_checksum = compute_checksum(filename.c_str(), BUFFER_SIZE );
    std::cout << "Checksum calculated on server for file '" << filename << "': " << server_checksum << std::endl;

    // Compare the checksums
    if (client_checksum == server_checksum) {
        std::cout << "Checksum match: File transfer successful.\n";
    } else {
        std::cerr << "Checksum mismatch: File may be corrupted.\n";
    }
}

void handle_get(SOCKET client_socket, const std::string& filename) {
    char buffer[BUFFER_SIZE];
    std::ifstream infile(filename, std::ios::binary);
    if (!infile) {
        std::cerr << "Error: File not found on server.\n";
        send(client_socket, "ERROR: File not found", 21, 0);
        return;
    }

    while (!infile.eof()) {
        infile.read(buffer, BUFFER_SIZE);
        send(client_socket, buffer, infile.gcount(), 0);
    }
    infile.close();
    std::cout << "File '" << filename << "' sent to client.\n";
}

void client_handler(SOCKET client_socket) {


    char buffer[BUFFER_SIZE];
    while (true) {
        // Receive data from the client

        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            std::cerr << "Client disconnected or timeout. Bytes received: " << bytes_received << "\n";
            break;
        }
        
        buffer[bytes_received] = '\0';
        // Debugging received data new(10:48pm) worked
        std::cout << "Received data: " << buffer << std::endl;

        std::string command(buffer);

        //New code (10:57)
        if (command.substr(0, 3) == "put") {
            // Ensure the command has enough length to extract the filename
            if (command.length() > 4) {
                std::string filename = command.substr(4);  // Extract filename
                std::cout << "Command received: put " << filename << std::endl;
                handle_put(client_socket, filename);
            } else {
                std::cerr << "No filename provided with put command.\n";
            }
        } else if (command.substr(0, 3) == "get") {
            if (command.length() > 4) {
                std::string filename = command.substr(4);  // Extract filename
                std::cout << "Command received: get " << filename << std::endl;
                handle_get(client_socket, filename);
            } else {
                std::cerr << "No filename provided with get command.\n";
            }
        } else {
            std::cerr << "Unknown command received: " << command << std::endl;
        }
    }
    closesocket(client_socket);
}



int main() {
    WSADATA wsa;
    SOCKET server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    int client_addr_len = sizeof(client_addr);

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "Failed to initialize Winsock. Error Code: " << WSAGetLastError() << "\n";
        return 1;
    }

    // Create server socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == INVALID_SOCKET) {
        std::cerr << "Could not create socket. Error Code: " << WSAGetLastError() << "\n";
        return 1;
    }

    // Prepare the sockaddr_in structure
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Bind the socket
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed. Error Code: " << WSAGetLastError() << "\n";
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    // Listen for incoming connections
    if (listen(server_socket, 5) == SOCKET_ERROR) {
        std::cerr << "Listen failed. Error Code: " << WSAGetLastError() << "\n";
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    std::cout << "Server is listening on port " << PORT << "...\n";

    // Main server loop to accept and handle clients
    while (true) {
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_addr_len);
        if (client_socket == INVALID_SOCKET) {
            std::cerr << "Accept failed. Error Code: " << WSAGetLastError() << "\n";
            continue;
        }
        std::cout << "Client connected.\n";

        // Create a new thread for each client
        std::thread(client_handler, client_socket).detach();
    }

    closesocket(server_socket);
    WSACleanup();
    return 0;
}
