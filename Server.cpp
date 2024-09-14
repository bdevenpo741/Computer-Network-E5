#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <fstream>
#include <thread>
#include <vector>

#pragma comment(lib, "Ws2_32.lib")

#define SERVER_PORT 8080
#define BUFFER_SIZE 1024

void handleClient(SOCKET clientSocket);

void sendFile(SOCKET clientSocket, const std::string& filename);
void receiveFile(SOCKET clientSocket, const std::string& filename);

int main() {
    WSADATA wsaData;
    SOCKET serverSocket, clientSocket;
    struct sockaddr_in serverAddr, clientAddr;
    int clientAddrSize = sizeof(clientAddr);

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed.\n";
        return 1;
    }

    // Create socket
    serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed.\n";
        WSACleanup();
        return 1;
    }

    // Set up the server address structure
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    // Bind the socket
    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed.\n";
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    // Listen for incoming connections
    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed.\n";
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Server is running and waiting for connections...\n";

    std::vector<std::thread> clientThreads;

    // Accept client connections and handle them in separate threads
    while (true) {
        clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientAddrSize);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "Failed to accept client connection.\n";
            continue;
        }

        std::cout << "Client connected.\n";
        clientThreads.push_back(std::thread(handleClient, clientSocket));
    }

    // Cleanup (unreachable in this example, but good practice to include)
    closesocket(serverSocket);
    WSACleanup();

    return 0;
}

void handleClient(SOCKET clientSocket) {
    char buffer[BUFFER_SIZE];
    int bytesReceived;

    while ((bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE, 0)) > 0) {
        buffer[bytesReceived] = '\0';
        std::string command(buffer);

        std::string action = command.substr(0, command.find(' '));
        std::string filename = command.substr(command.find(' ') + 1);

        if (action == "put") {
            std::cout << "Receiving file: " << filename << "\n";
            receiveFile(clientSocket, filename);
        } else if (action == "get") {
            std::cout << "Sending file: " << filename << "\n";
            sendFile(clientSocket, filename);
        } else {
            std::cerr << "Unknown command: " << command << "\n";
        }
    }

    // Close the client socket
    closesocket(clientSocket);
    std::cout << "Client disconnected.\n";
}

void sendFile(SOCKET clientSocket, const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Could not open file: " << filename << "\n";
        return;
    }

    char buffer[BUFFER_SIZE];
    while (!file.eof()) {
        file.read(buffer, BUFFER_SIZE);
        int bytesRead = file.gcount();
        send(clientSocket, buffer, bytesRead, 0);
    }

    file.close();
    std::cout << "File " << filename << " sent to client.\n";
}

void receiveFile(SOCKET clientSocket, const std::string& filename) {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Could not create file: " << filename << "\n";
        return;
    }

    char buffer[BUFFER_SIZE];
    int bytesReceived;
    bool dataReceived = false;

    while ((bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE, 0)) > 0) {
        dataReceived = true;
        file.write(buffer, bytesReceived);
        if (bytesReceived < BUFFER_SIZE) {
            break;  // Assume the transmission is complete if less than buffer size is received.
        }
    }

    file.close();

    if (!dataReceived) {
        std::cerr << "No data received for file: " << filename << "\n";
        send(clientSocket, "File upload failed: No data received", 38, 0);
        std::remove(filename.c_str());
    } else {
        std::cout << "File " << filename << " received from client.\n";
        send(clientSocket, "File upload successful", 22, 0);
    }
}
