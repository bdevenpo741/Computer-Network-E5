// Client.cpp

#define _WINSOCK_DEPRECATED_NO_WARNINGS // Suppress deprecation warnings (optional)

#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h> // For getaddrinfo, getnameinfo
#include <fstream>
#include <thread>
#include <string>
#include <sstream>
#include <filesystem> // For directory operations
#include <algorithm>  // For std::replace
#include <chrono>
#include <atomic>     // For std::atomic

#pragma comment(lib, "Ws2_32.lib")

#define SERVER_PORT 8080
#define BUFFER_SIZE 1024

namespace fs = std::filesystem; // Alias for filesystem namespace

int64_t SendFile(SOCKET s, const std::string& fileName, int chunkSize = 64 * 1024);
void receiveFile(SOCKET& clientSocket, const std::string& filename);
void receiveMessages(SOCKET clientSocket, const std::string& username);
void registerWithServer(SOCKET clientSocket, const std::string& username, const std::string& password, int filePort);
void announceResources(SOCKET clientSocket, const std::string& username, const std::string& directoryPath);
void requestResourceList(SOCKET clientSocket, const std::string& username);
void handleIncomingConnections(std::atomic<int>& filePort, const std::string& directoryPath);
void requestFileFromPeer(const std::string& ownerIP, int ownerPort, const std::string& filename);
int64_t GetFileSize(const std::string& fileName);
int SendBuffer(SOCKET s, const char* buffer, int bufferSize, int chunkSize = 4 * 1024);

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
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr); // Connect to local server

    // Connect to server
    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Connection to server failed.\n";
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Connected to server.\n";

    std::string username;
    std::cout << "Enter username: ";
    std::getline(std::cin, username);
    std::string password = "password"; // Optional password handling

    // Start listening for incoming file transfer requests
    std::atomic<int> filePort(0); // Initialize to 0, will be set dynamically
    std::string directoryPath;
    std::cout << "Enter the path of the directory to share: ";
    std::getline(std::cin, directoryPath);

    // Start the file server thread, pass filePort by reference
    std::thread fileServerThread(handleIncomingConnections, std::ref(filePort), directoryPath);

    // Wait for the file server to start and assign the port
    while (filePort.load() == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Register with server
    registerWithServer(clientSocket, username, password, filePort);

    // Announce resources by specifying a directory
    announceResources(clientSocket, username, directoryPath);

    // Start a thread to receive messages from the server
    std::thread receiveThread(receiveMessages, clientSocket, username);

    while (true) {
        std::cout << "Enter command (get <filename> <owner_username> / GET_RESOURCES / exit): ";
        std::cin.getline(buffer, BUFFER_SIZE);
        std::string command(buffer);

        if (command == "exit") {
            // Notify server of logout
            std::string logoutMessage = "LOGOUT " + username;
            send(clientSocket, logoutMessage.c_str(), logoutMessage.length(), 0);
            break;
        }
        else if (command == "GET_RESOURCES") {
            requestResourceList(clientSocket, username);
        }
        else {
            // Parse the command to decide on file transfer action
            std::istringstream iss(command);
            std::string action, filename, ownerUsername;
            iss >> action >> filename >> ownerUsername;

            if (action == "get") {
                if (filename.empty() || ownerUsername.empty()) {
                    std::cout << "Usage: get <filename> <owner_username>\n";
                    continue;
                }
                // Send request to server to get owner's address
                std::string getFileRequest = "GET_FILE " + filename + " " + ownerUsername;
                send(clientSocket, getFileRequest.c_str(), getFileRequest.length(), 0);
            }
            else {
                // Send as a chat message
                send(clientSocket, command.c_str(), command.length(), 0);
            }
        }
    }

    // Cleanup
    receiveThread.join();
    fileServerThread.detach(); // You may implement proper shutdown signaling
    closesocket(clientSocket);
    WSACleanup();
    return 0;
}

void registerWithServer(SOCKET clientSocket, const std::string& username, const std::string& password, int filePort) {
    // Use local IP 127.0.0.1 for testing on the same machine
    std::string localIP = "127.0.0.1"; // Localhost IP

    std::string registerMessage = "REGISTER " + username + " " + password + " " + localIP + " " + std::to_string(filePort);
    send(clientSocket, registerMessage.c_str(), registerMessage.length(), 0);

    // Wait for acknowledgment
    char buffer[BUFFER_SIZE];
    int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0);
    if (bytesReceived > 0) {
        buffer[bytesReceived] = '\0';
        std::string response(buffer);
        std::cout << "Server response: " << response << "\n";
    }
}

void announceResources(SOCKET clientSocket, const std::string& username, const std::string& directoryPath) {
    // Collect filenames from the directory
    std::string resources;
    try {
        for (const auto& entry : fs::directory_iterator(directoryPath)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                // Replace spaces with underscores in filenames
                std::replace(filename.begin(), filename.end(), ' ', '_');
                resources += filename + " ";
            }
        }
    }
    catch (const fs::filesystem_error& e) {
        std::cerr << "Error accessing directory: " << e.what() << "\n";
        return;
    }

    if (resources.empty()) {
        std::cout << "No files found in the directory.\n";
        return;
    }

    std::string resourceMessage = "RESOURCES " + username + " " + resources;
    send(clientSocket, resourceMessage.c_str(), resourceMessage.length(), 0);
}

void requestResourceList(SOCKET clientSocket, const std::string& username) {
    std::string getRequest = "GET_RESOURCES " + username;
    send(clientSocket, getRequest.c_str(), getRequest.length(), 0);
}

void receiveMessages(SOCKET clientSocket, const std::string& username) {
    char buffer[BUFFER_SIZE];

    while (true) {
        int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0);
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            std::string message(buffer);

            if (message == "HELLO") {
                // Respond to hello message
                std::string helloAck = "HELLO_ACK " + username;
                send(clientSocket, helloAck.c_str(), helloAck.length(), 0);
            }
            else if (message.substr(0, 13) == "RESOURCE_LIST") {
                // Parse and display resource list
                std::cout << "Available Resources:\n";
                std::istringstream iss(message.substr(14));
                std::string resource;
                while (iss >> resource) {
                    std::cout << resource << "\n";
                }
            }
            else if (message.substr(0, 9) == "OWNER_INFO") {
                // Received owner's info for file transfer
                std::istringstream iss(message);
                std::string token, ownerIP, filename;
                int ownerPort;
                iss >> token >> ownerIP >> ownerPort >> filename;

                // Start file transfer from owner
                requestFileFromPeer(ownerIP, ownerPort, filename);
            }
            else if (message.substr(0, 5) == "ERROR") {
                std::cout << "Server error: " << message.substr(6) << "\n";
            }
            else {
                // Handle other messages
                std::cout << "Server: " << message << "\n";
            }
        }
        else if (bytesReceived == 0) {
            std::cout << "Server disconnected.\n";
            break;
        }
        else {
            std::cerr << "Error receiving message from server.\n";
            break;
        }
    }
}

void handleIncomingConnections(std::atomic<int>& filePort, const std::string& directoryPath) {
    SOCKET listenSocket;
    struct sockaddr_in serverAddr;

    // Create listening socket
    listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        std::cerr << "File server socket creation failed.\n";
        return;
    }

    // Bind the socket to port 0 (dynamic port)
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(0); // Use 0 to let the OS assign an available port
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "File server bind failed.\n";
        closesocket(listenSocket);
        return;
    }

    // Retrieve the assigned port number
    sockaddr_in localAddr;
    int addrLen = sizeof(localAddr);
    if (getsockname(listenSocket, (sockaddr*)&localAddr, &addrLen) == SOCKET_ERROR) {
        std::cerr << "getsockname failed.\n";
        closesocket(listenSocket);
        return;
    }
    filePort = ntohs(localAddr.sin_port);
    std::cout << "File server is listening on port " << filePort << "\n";

    // Listen for incoming connections
    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "File server listen failed.\n";
        closesocket(listenSocket);
        return;
    }

    while (true) {
        SOCKET clientSocket;
        sockaddr_in clientAddr;
        int clientAddrSize = sizeof(clientAddr);

        clientSocket = accept(listenSocket, (sockaddr*)&clientAddr, &clientAddrSize);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "File server accept failed.\n";
            continue;
        }

        // Handle file request in a separate thread
        std::thread([clientSocket, directoryPath]() {
            char buffer[BUFFER_SIZE];
            int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0);
            if (bytesReceived > 0) {
                buffer[bytesReceived] = '\0';
                std::string filename(buffer);
                std::cout << "Received file request for: " << filename << "\n";

                // Construct the file path
                fs::path filePath = fs::path(directoryPath) / filename;

                // Check if file exists
                if (!fs::exists(filePath)) {
                    std::cerr << "File does not exist: " << filePath << "\n";
                    // Send error message to requester
                    std::string errorMsg = "ERROR File not found.";
                    send(clientSocket, errorMsg.c_str(), errorMsg.length(), 0);
                }
                else {
                    // Send the file
                    int64_t result = SendFile(clientSocket, filePath.string());
                    if (result < 0) {
                        std::cerr << "Failed to send file: " << filename << "\n";
                    }
                    else {
                        std::cout << "Sent file: " << filename << "\n";
                    }
                }
            }
            else {
                std::cerr << "Failed to receive filename from requester.\n";
            }
            closesocket(clientSocket);
            }).detach();
    }
}

void requestFileFromPeer(const std::string& ownerIP, int ownerPort, const std::string& filename) {
    std::cout << "Attempting to connect to owner at " << ownerIP << ":" << ownerPort << "\n";
    SOCKET peerSocket;
    struct sockaddr_in peerAddr;

    // Create socket
    peerSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (peerSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed.\n";
        return;
    }

    // Owner address
    peerAddr.sin_family = AF_INET;
    peerAddr.sin_port = htons(ownerPort);
    inet_pton(AF_INET, ownerIP.c_str(), &peerAddr.sin_addr);

    // Connect to owner client
    if (connect(peerSocket, (sockaddr*)&peerAddr, sizeof(peerAddr)) == SOCKET_ERROR) {
        std::cerr << "Connection to owner client failed.\n";
        closesocket(peerSocket);
        return;
    }

    std::cout << "Connected to owner client. Requesting file: " << filename << "\n";

    // Send filename to owner client
    send(peerSocket, filename.c_str(), filename.length(), 0);

    // Receive the file
    receiveFile(peerSocket, filename);

    // Close the connection
    closesocket(peerSocket);
}

// Function to receive file from server or peer
void receiveFile(SOCKET& clientSocket, const std::string& filename) {
    // Receive the file size first
    int64_t fileSize;
    int bytesReceived = recv(clientSocket, (char*)&fileSize, sizeof(fileSize), 0);
    if (bytesReceived != sizeof(fileSize)) {
        std::cerr << "Failed to receive file size. Bytes received: " << bytesReceived << "\n";
        return;
    }
    std::cout << "Receiving file: " << filename << " Size: " << fileSize << " bytes\n";

    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Could not create file: " << filename << "\n";
        return;
    }

    char buffer[BUFFER_SIZE];
    int64_t totalBytesReceived = 0;
    while (totalBytesReceived < fileSize) {
        bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE, 0);
        if (bytesReceived <= 0) {
            std::cerr << "Failed to receive file data. Bytes received: " << bytesReceived << "\n";
            break;
        }
        file.write(buffer, bytesReceived);
        totalBytesReceived += bytesReceived;
    }

    file.close();

    if (totalBytesReceived == fileSize) {
        std::cout << "File " << filename << " received successfully.\n";
    }
    else {
        std::cerr << "File transfer incomplete. Expected: " << fileSize << ", Received: " << totalBytesReceived << "\n";
    }
}

// Function to get file size
int64_t GetFileSize(const std::string& fileName) {
    FILE* f;
    if (fopen_s(&f, fileName.c_str(), "rb") != 0) {
        return -1;
    }
    _fseeki64(f, 0, SEEK_END);
    const int64_t len = _ftelli64(f);
    fclose(f);
    return len;
}

// Function to send buffer
int SendBuffer(SOCKET s, const char* buffer, int bufferSize, int chunkSize) {
    int i = 0;
    while (i < bufferSize) {
        const int l = send(s, &buffer[i], min(chunkSize, bufferSize - i), 0);
        if (l < 0) { return l; } // This is an error
        i += l;
    }
    return i;
}

// Function to send file to client
int64_t SendFile(SOCKET s, const std::string& fileName, int chunkSize) {
    const int64_t fileSize = GetFileSize(fileName);
    if (fileSize < 0) {
        std::cerr << "Failed to get file size.\n";
        return -1;
    }

    std::ifstream file(fileName, std::ifstream::binary);
    if (file.fail()) {
        std::cerr << "Failed to open file.\n";
        return -1;
    }

    // Send the file size first
    if (SendBuffer(s, reinterpret_cast<const char*>(&fileSize), sizeof(fileSize)) != sizeof(fileSize)) {
        std::cerr << "Failed to send file size.\n";
        return -2;
    }

    char* buffer = new char[chunkSize];
    bool errored = false;
    int64_t bytesSent = 0;
    while (bytesSent < fileSize) {
        file.read(buffer, chunkSize);
        std::streamsize bytesRead = file.gcount();
        if (bytesRead <= 0) {
            errored = true;
            break;
        }
        if (SendBuffer(s, buffer, static_cast<int>(bytesRead)) != bytesRead) {
            errored = true;
            break;
        }
        bytesSent += bytesRead;
    }
    delete[] buffer;

    file.close();

    return errored ? -3 : fileSize;
}
