// Client.cpp

#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <fstream>
#include <thread>
#include <string>
#include <sstream>
#pragma comment(lib, "Ws2_32.lib")

#define SERVER_PORT 8080
#define BUFFER_SIZE 1024

void receiveMessages(SOCKET clientSocket, sockaddr_in serverAddr, const std::string& username);
void registerWithServer(SOCKET clientSocket, sockaddr_in serverAddr, const std::string& username, const std::string& password);
void announceResources(SOCKET clientSocket, sockaddr_in serverAddr, const std::string& username, const std::string& resources);
void requestResourceList(SOCKET clientSocket, sockaddr_in serverAddr, const std::string& username);

int main(int argc, char* argv[]) {
    WSADATA wsaData;
    SOCKET clientSocket;
    struct sockaddr_in serverAddr;

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed.\n";
        return 1;
    }

    // Create UDP socket
    clientSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed.\n";
        WSACleanup();
        return 1;
    }

    // Bind the client socket to a local port
    sockaddr_in clientAddr;
    clientAddr.sin_family = AF_INET;
    clientAddr.sin_addr.s_addr = INADDR_ANY;
    clientAddr.sin_port = htons(0); // Use 0 to let the OS assign a port

    if (bind(clientSocket, (sockaddr*)&clientAddr, sizeof(clientAddr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed with error: " << WSAGetLastError() << "\n";
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    // Server address
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);

    std::string username;
    std::cout << "Enter username: ";
    std::getline(std::cin, username);
    std::string password = "password"; // Optional password handling

    // Register with server
    registerWithServer(clientSocket, serverAddr, username, password);

    // Announce resources
    std::string resources;
    std::cout << "Enter resources to share (space-separated filenames): ";
    std::getline(std::cin, resources);
    announceResources(clientSocket, serverAddr, username, resources);

    // Start a thread to receive messages from the server
    std::thread receiveThread(receiveMessages, clientSocket, serverAddr, username);

    while (true) {
        std::cout << "Enter command (GET_RESOURCES / exit): ";
        std::string command;
        std::getline(std::cin, command);

        if (command == "exit") {
            // Notify server of logout
            std::string logoutMessage = "LOGOUT " + username;
            sendto(clientSocket, logoutMessage.c_str(), logoutMessage.length(), 0, (sockaddr*)&serverAddr, sizeof(serverAddr));
            break;
        } else if (command == "GET_RESOURCES") {
            requestResourceList(clientSocket, serverAddr, username);
        } else {
            std::cout << "Unknown command.\n";
        }
    }

    // Cleanup
    receiveThread.detach(); // Or handle thread termination properly
    closesocket(clientSocket);
    WSACleanup();
    return 0;
}

void registerWithServer(SOCKET clientSocket, sockaddr_in serverAddr, const std::string& username, const std::string& password) {
    std::string registerMessage = "REGISTER " + username + " " + password;
    sendto(clientSocket, registerMessage.c_str(), registerMessage.length(), 0, (sockaddr*)&serverAddr, sizeof(serverAddr));

    // Optionally, wait for acknowledgment
    char buffer[BUFFER_SIZE];
    sockaddr_in fromAddr;
    int fromAddrSize = sizeof(fromAddr);
    int bytesReceived = recvfrom(clientSocket, buffer, BUFFER_SIZE - 1, 0, (sockaddr*)&fromAddr, &fromAddrSize);
    if (bytesReceived > 0) {
        buffer[bytesReceived] = '\0';
        std::string response(buffer);
        std::cout << "Server response: " << response << "\n";
    }
}

void announceResources(SOCKET clientSocket, sockaddr_in serverAddr, const std::string& username, const std::string& resources) {
    std::string resourceMessage = "RESOURCES " + username + " " + resources;
    sendto(clientSocket, resourceMessage.c_str(), resourceMessage.length(), 0, (sockaddr*)&serverAddr, sizeof(serverAddr));
}

void requestResourceList(SOCKET clientSocket, sockaddr_in serverAddr, const std::string& username) {
    std::string getRequest = "GET_RESOURCES " + username;
    sendto(clientSocket, getRequest.c_str(), getRequest.length(), 0, (sockaddr*)&serverAddr, sizeof(serverAddr));
}

void receiveMessages(SOCKET clientSocket, sockaddr_in serverAddr, const std::string& username) {
    char buffer[BUFFER_SIZE];
    sockaddr_in fromAddr;
    int fromAddrSize = sizeof(fromAddr);

    while (true) {
        int bytesReceived = recvfrom(clientSocket, buffer, BUFFER_SIZE - 1, 0, (sockaddr*)&fromAddr, &fromAddrSize);
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            std::string message(buffer);

            if (message == "HELLO") {
                // Respond to hello message
                std::string helloAck = "HELLO_ACK " + username;
                sendto(clientSocket, helloAck.c_str(), helloAck.length(), 0, (sockaddr*)&serverAddr, sizeof(serverAddr));
            } else if (message.substr(0, 13) == "RESOURCE_LIST") {
                // Parse and display resource list
                std::cout << "Available Resources:\n";
                std::istringstream iss(message.substr(14));
                std::string resource;
                while (iss >> resource) {
                    std::cout << resource << "\n";
                }
            } else {
                // Handle other messages
                std::cout << "Received message: " << message << "\n";
            }
        }
    }
}
