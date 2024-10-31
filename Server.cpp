// Server.cpp

#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <fstream>
#include <thread>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <string>
#include <sstream>
#include <chrono>
#include <algorithm>
#pragma comment(lib, "Ws2_32.lib")

#define SERVER_PORT 8080
#define BUFFER_SIZE 1024

// Global server socket
SOCKET serverSocket;

// Struct to store user information
struct UserInfo {
    sockaddr_in addr;
    std::chrono::steady_clock::time_point lastActive;
};

// User directory: username -> UserInfo
std::unordered_map<std::string, UserInfo> userDirectory;

// Resource directory: resource name -> owner username
std::unordered_map<std::string, std::string> resourceDirectory;

std::mutex directoryMutex;

void handleClientMessage(const std::string& message, const sockaddr_in& clientAddr);
void sendHelloMessages();

int main() {
    WSADATA wsaData;
    struct sockaddr_in serverAddr, clientAddr;
    int clientAddrSize = sizeof(clientAddr);

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed.\n";
        return 1;
    }

    // Create UDP socket
    serverSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "UDP socket creation failed.\n";
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

    std::cout << "Server is running and waiting for messages...\n";

    // Start a thread to send periodic hello messages
    std::thread helloThread(sendHelloMessages);

    char buffer[BUFFER_SIZE];

    while (true) {
        int bytesReceived = recvfrom(serverSocket, buffer, BUFFER_SIZE - 1, 0, (sockaddr*)&clientAddr, &clientAddrSize);
        if (bytesReceived == SOCKET_ERROR) {
            int error = WSAGetLastError();
            std::cerr << "recvfrom failed with error: " << error << "\n";
            continue;
        }

        buffer[bytesReceived] = '\0';
        std::string message(buffer);

        // Handle the client message
        handleClientMessage(message, clientAddr);
    }

    // Cleanup (unreachable in this example)
    closesocket(serverSocket);
    WSACleanup();

    return 0;
}

void handleClientMessage(const std::string& message, const sockaddr_in& clientAddr) {
    std::lock_guard<std::mutex> lock(directoryMutex);

    std::istringstream iss(message);
    std::string command;
    iss >> command;

    if (command == "REGISTER") {
        // Handle registration
        std::string username, password;
        iss >> username >> password; // Password is optional

        // Add to user directory
        UserInfo userInfo;
        userInfo.addr = clientAddr;
        userInfo.lastActive = std::chrono::steady_clock::now();
        userDirectory[username] = userInfo;

        // Send acknowledgment
        std::string ack = "REGISTERED " + username;
        sendto(serverSocket, ack.c_str(), ack.length(), 0, (sockaddr*)&clientAddr, sizeof(clientAddr));

        std::cout << "Registered user: " << username << "\n";
    } else {
        // For other commands, extract username
        std::string username;
        iss >> username;

        if (userDirectory.find(username) != userDirectory.end()) {
            // Update user's addr and lastActive time
            userDirectory[username].addr = clientAddr;
            userDirectory[username].lastActive = std::chrono::steady_clock::now();
        } else {
            // User not registered
            std::cerr << "Received " << command << " from unregistered user: " << username << "\n";
            return;
        }

        if (command == "RESOURCES") {
            // Handle resource announcement
            std::string resource;
            while (iss >> resource) {
                resourceDirectory[resource] = username;
            }

            std::cout << "User " << username << " announced resources.\n";
        } else if (command == "GET_RESOURCES") {
            // Handle resource request
            std::string resourceList = "RESOURCE_LIST ";
            for (const auto& resource : resourceDirectory) {
                resourceList += resource.first + ":" + resource.second + " ";
            }

            // Send resource list to client
            sendto(serverSocket, resourceList.c_str(), resourceList.length(), 0, (sockaddr*)&clientAddr, sizeof(clientAddr));
        } else if (command == "HELLO_ACK") {
            // Handle hello acknowledgment
            std::cout << "Received HELLO_ACK from " << username << "\n";
        } else if (command == "LOGOUT") {
            // Handle logout
            // Remove user from directories
            userDirectory.erase(username);

            // Remove user's resources
            for (auto it = resourceDirectory.begin(); it != resourceDirectory.end();) {
                if (it->second == username) {
                    it = resourceDirectory.erase(it);
                } else {
                    ++it;
                }
            }

            std::cout << "User " << username << " logged out.\n";
        } else {
            // Unknown command
            std::cerr << "Unknown command received: " << message << "\n";
        }
    }
}

void sendHelloMessages() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(30)); // Adjust interval as needed

        std::lock_guard<std::mutex> lock(directoryMutex);
        auto now = std::chrono::steady_clock::now();

        for (auto it = userDirectory.begin(); it != userDirectory.end();) {
            std::string username = it->first;
            UserInfo& userInfo = it->second;

            // Send HELLO message
            std::string helloMessage = "HELLO";
            int sendResult = sendto(serverSocket, helloMessage.c_str(), helloMessage.length(), 0, (sockaddr*)&userInfo.addr, sizeof(userInfo.addr));
            if (sendResult == SOCKET_ERROR) {
                int error = WSAGetLastError();
                std::cerr << "sendto failed with error: " << error << "\n";
                ++it;
                continue;
            }

            // Check if the user has been inactive for more than a threshold
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - userInfo.lastActive).count();
            if (duration > 90) { // If inactive for more than 90 seconds
                std::cout << "Removing inactive user: " << username << "\n";

                // Remove user's resources
                for (auto resIt = resourceDirectory.begin(); resIt != resourceDirectory.end();) {
                    if (resIt->second == username) {
                        resIt = resourceDirectory.erase(resIt);
                    } else {
                        ++resIt;
                    }
                }

                // Remove user from directory
                it = userDirectory.erase(it);
            } else {
                ++it;
            }
        }
    }
}
