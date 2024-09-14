#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <fstream>

#pragma comment(lib, "Ws2_32.lib")

#define SERVER_PORT 8080
#define BUFFER_SIZE 1024

void sendFile(SOCKET& clientSocket, const std::string& filename);
void receiveFile(SOCKET& clientSocket, const std::string& filename);

int main(int argc, char* argv[]) {
    WSADATA wsaData;
    SOCKET clientSocket;
    struct sockaddr_in serverAddr;
    char buffer[BUFFER_SIZE];

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed.\n";
        return 1;
    }

    // Create socket
    clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed.\n";
        WSACleanup();
        return 1;
    }

    // Server address
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);

    // Connect to server
    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Connection to server failed.\n";
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Connected to server.\n";
    
    while (true) {
        std::cout << "Enter command (put <filename> / get <filename> / exit): ";
        std::cin.getline(buffer, BUFFER_SIZE);
        std::string command(buffer);

        // Send command to the server
        send(clientSocket, buffer, command.size(), 0);

        if (command == "exit") {
            break;
        }

        // Parse the command to decide on file transfer action
        std::string action = command.substr(0, command.find(' '));
        std::string filename = command.substr(command.find(' ') + 1);

        if (action == "put") {
            sendFile(clientSocket, filename);

            // Wait for confirmation from the server
            int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE, 0);
            if (bytesReceived > 0) {
                buffer[bytesReceived] = '\0';
                std::cout << "Server response: " << buffer << "\n";
            }
        } else if (action == "get") {
            receiveFile(clientSocket, filename);
        } else {
            std::cout << "Invalid command.\n";
        }
    }

    // Cleanup
    closesocket(clientSocket);
    WSACleanup();
    return 0;
}

void sendFile(SOCKET& clientSocket, const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Could not open file: " << filename << "\n";
        return;
    }

    char buffer[BUFFER_SIZE];
    bool dataSent = false;

    while (!file.eof()) {
        file.read(buffer, BUFFER_SIZE);
        int bytesRead = file.gcount();
        send(clientSocket, buffer, bytesRead, 0);
        dataSent = true;
    }

    file.close();

    if (dataSent) {
        std::cout << "File " << filename << " sent successfully.\n";
    } else {
        std::cerr << "No data was sent for file: " << filename << "\n";
    }
}

void receiveFile(SOCKET& clientSocket, const std::string& filename) {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Could not create file: " << filename << "\n";
        return;
    }

    char buffer[BUFFER_SIZE];
    int bytesReceived;
    while ((bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE, 0)) > 0) {
        file.write(buffer, bytesReceived);
    }

    file.close();
    std::cout << "File " << filename << " received successfully.\n";
}
