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

// Struct to store user information
struct UserInfo {
    SOCKET clientSocket;
    std::chrono::steady_clock::time_point lastActive;
    std::string ipAddress;
    int filePort;
};

std::vector<SOCKET> clientSockets;
std::mutex clientsMutex;

// User directory: username -> UserInfo
std::unordered_map<std::string, UserInfo> userDirectory;

// Resource directory: resource name -> owner username
std::unordered_map<std::string, std::string> resourceDirectory;

std::mutex directoryMutex;

void broadcastMessage(const std::string& message, SOCKET senderSocket);
void handleClient(SOCKET clientSocket);
int64_t SendFile(SOCKET s, const std::string& fileName, int chunkSize);
void receiveFile(SOCKET clientSocket, const std::string& filename);
void sendHelloMessages();

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

    // Create TCP socket
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

    // Start a thread to send periodic hello messages
    std::thread helloThread(sendHelloMessages);
    helloThread.detach(); // Detach the thread to run independently

    std::vector<std::thread> clientThreads;

    // Accept client connections and handle them in separate threads
    while (true) {
        clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientAddrSize);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "Failed to accept client connection.\n";
            continue;
        }

        std::cout << "Client connected.\n";

        // Adds client to the clients list
        {
            std::lock_guard<std::mutex> lock(clientsMutex);
            clientSockets.push_back(clientSocket);
        }

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
    int buff = 1024;
    std::string username;

    while ((bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[bytesReceived] = '\0';
        std::string command(buffer);

        // Parse the command
        std::istringstream iss(command);
        std::string action;
        iss >> action;

        if (action == "REGISTER") {
            // Handle registration
            iss >> username;
            std::string password, ipAddress;
            int filePort;
            iss >> password >> ipAddress >> filePort;

            // Add to user directory
            {
                std::lock_guard<std::mutex> lock(directoryMutex);
                UserInfo userInfo;
                userInfo.clientSocket = clientSocket;
                userInfo.lastActive = std::chrono::steady_clock::now();
                userInfo.ipAddress = ipAddress;
                userInfo.filePort = filePort;
                userDirectory[username] = userInfo;
            }
            // Send acknowledgment
            std::string ack = "REGISTERED " + username;
            send(clientSocket, ack.c_str(), ack.length(), 0);
            std::cout << "Registered user: " << username << "\n";
        }
        else if (action == "GET_FILE") {
            // Handle file request
            std::string filename, ownerUsername;
            iss >> filename >> ownerUsername;
            {
                std::lock_guard<std::mutex> lock(directoryMutex);
                // Check if resource exists and is owned by ownerUsername
                if (resourceDirectory.find(filename) != resourceDirectory.end() && resourceDirectory[filename] == ownerUsername) {
                    // Get owner info
                    if (userDirectory.find(ownerUsername) != userDirectory.end()) {
                        UserInfo& ownerInfo = userDirectory[ownerUsername];
                        std::string ownerInfoMessage = "OWNER_INFO " + ownerInfo.ipAddress + " " + std::to_string(ownerInfo.filePort) + " " + filename;
                        send(clientSocket, ownerInfoMessage.c_str(), ownerInfoMessage.length(), 0);
                        std::cout << "Sent owner info to " << username << " for file " << filename << "\n";
                    }
                    else {
                        std::string errorMsg = "ERROR Owner not online.";
                        send(clientSocket, errorMsg.c_str(), errorMsg.length(), 0);
                    }
                }
                else {
                    std::string errorMsg = "ERROR File not found or incorrect owner.";
                    send(clientSocket, errorMsg.c_str(), errorMsg.length(), 0);
                }
            }
        }
        else if (action == "RESOURCES") {
            // Handle resource announcement
            iss >> username;
            std::string resource;
            {
                std::lock_guard<std::mutex> lock(directoryMutex);
                while (iss >> resource) {
                    resourceDirectory[resource] = username;
                }
            }
            std::cout << "User " << username << " announced resources.\n";
        }
        else if (action == "GET_RESOURCES") {
            // Handle resource request
            std::string resourceList = "RESOURCE_LIST ";
            {
                std::lock_guard<std::mutex> lock(directoryMutex);
                for (const auto& resource : resourceDirectory) {
                    resourceList += resource.first + ":" + resource.second + " ";
                }
            }
            // Send resource list to client
            send(clientSocket, resourceList.c_str(), resourceList.length(), 0);
        }
        else if (action == "HELLO_ACK") {
            // Handle hello acknowledgment
            iss >> username;
            std::cout << "Received HELLO_ACK from " << username << "\n";
            // Update last active time
            {
                std::lock_guard<std::mutex> lock(directoryMutex);
                if (userDirectory.find(username) != userDirectory.end()) {
                    userDirectory[username].lastActive = std::chrono::steady_clock::now();
                }
            }
        }
        else if (action == "LOGOUT") {
            // Handle logout
            iss >> username;
            {
                std::lock_guard<std::mutex> lock(directoryMutex);
                // Remove user from directories
                userDirectory.erase(username);
                // Remove user's resources
                for (auto it = resourceDirectory.begin(); it != resourceDirectory.end();) {
                    if (it->second == username) {
                        it = resourceDirectory.erase(it);
                    }
                    else {
                        ++it;
                    }
                }
            }
            std::cout << "User " << username << " logged out.\n";
            break; // Exit the loop to end the thread
        }
        else {
            // Handle other commands or messages
            std::cout << "Received unknown command: " << command << "\n";
        }
    }

    // Client disconnected
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        auto it = std::remove(clientSockets.begin(), clientSockets.end(), clientSocket);
        clientSockets.erase(it, clientSockets.end());
    }
    {
        std::lock_guard<std::mutex> lock(directoryMutex);
        // Remove user from directories if not already removed
        for (auto it = userDirectory.begin(); it != userDirectory.end();) {
            if (it->second.clientSocket == clientSocket) {
                std::string disconnectedUser = it->first;
                userDirectory.erase(it);
                // Remove user's resources
                for (auto resIt = resourceDirectory.begin(); resIt != resourceDirectory.end();) {
                    if (resIt->second == disconnectedUser) {
                        resIt = resourceDirectory.erase(resIt);
                    }
                    else {
                        ++resIt;
                    }
                }
                break;
            }
            else {
                ++it;
            }
        }
    }
    // Close the client socket
    closesocket(clientSocket);
    std::cout << "Client disconnected.\n";
}

// Function to send periodic hello messages
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
            int sendResult = send(userInfo.clientSocket, helloMessage.c_str(), helloMessage.length(), 0);
            if (sendResult == SOCKET_ERROR) {
                int error = WSAGetLastError();
                std::cerr << "send failed with error: " << error << "\n";
                // Remove client from directories
                std::cout << "Removing user due to send error: " << username << "\n";
                userDirectory.erase(it++);
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
                    }
                    else {
                        ++resIt;
                    }
                }

                // Close the client socket
                closesocket(userInfo.clientSocket);

                // Remove user from directory
                userDirectory.erase(it++);
            }
            else {
                ++it;
            }
        }
    }
}
