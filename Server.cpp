#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <fstream>
#include <thread>
#include <vector>
#include <mutex>
#include <algorithm>
#include <map>
#include <string>
#include <sstream>
#pragma comment(lib, "Ws2_32.lib")

#define SERVER_PORT 8080
#define BUFFER_SIZE 1024
#define UDP_PORT 8081

// Add these structures to store user and resource information
std::map<std::string, std::pair<std::string, bool>> userDirectory; // username -> <IP, active>
std::map<std::string, std::string> resourceDirectory; // resourceName -> ownerName
std::vector<SOCKET> udpClients;
std::mutex clientsMutex;
SOCKET udpSocket;

void registerClient(const std::string& username, const std::string& ip, const std::vector<std::string>& resources);
void sendResourceList(SOCKET clientSocket);
void handleClient(SOCKET clientSocket);
void sendHelloMessages();

// Functions below are not required in this project but are kept for potential use in the final project,
// which may involve advanced file transfer or message broadcasting functionalities.

int64_t SendFile(SOCKET s, const std::string& fileName, int chunkSize);
void receiveFile(SOCKET clientSocket, const std::string& filename);
void broadcastMessage(const std::string& message, SOCKET senderSocket);


int main() {
    WSADATA wsaData;
    SOCKET serverSocket, clientSocket, udpSocket;
    struct sockaddr_in serverAddr, clientAddr, udpAddr;
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
    std::thread helloThread(sendHelloMessages);
    helloThread.detach();

    while (true) {
        clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientAddrSize);
        if (clientSocket != INVALID_SOCKET) {
            std::lock_guard<std::mutex> lock(clientsMutex);
            udpClients.push_back(clientSocket);
            std::thread(handleClient, clientSocket).detach();
        }
    }

// Functions below are not required in this project but are kept for potential use in the final project,
// which may involve advanced file transfer or message broadcasting functionalities.

    // // Create UDP socket
    // udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    // if (udpSocket == INVALID_SOCKET)
    // {
    //     std::cerr << "UDP socket creation failed.\n";
    //     closesocket(serverSocket);
    //     WSACleanup();
    //     return 1;
    // }

    // // UDP address structure
    // udpAddr.sin_family = AF_INET;
    // udpAddr.sin_port = htons(UDP_PORT);
    // udpAddr.sin_addr.s_addr = INADDR_ANY;

    // // Bind UDP socket
    // if (bind(udpSocket, (sockaddr*)&udpAddr, sizeof(udpAddr)) == SOCKET_ERROR)
    // {
    //     std::cerr << "UDP Bind failed";
    //     closesocket(udpSocket);
    //     closesocket(serverSocket);
    //     WSACleanup();
    //     return 1;
    // }


    // std::cout << "Server is running and waiting for connections...\n";

    // std::vector<std::thread> clientThreads;

    // // Accept client connections and handle them in separate threads
    // while (true) {
    //     clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientAddrSize);
    //     if (clientSocket == INVALID_SOCKET) {
    //         std::cerr << "Failed to accept client connection.\n";
    //         continue;
    //     }

    //     std::cout << "Client connected.\n";

    //     // Adds clients to the UDP clients list
    //     {
    //         std::lock_guard<std::mutex> lock(clientsMutex);
    //         udpClients.push_back(clientSocket);
    //     }

    //     clientThreads.push_back(std::thread(handleClient, clientSocket));
    // }


   // Cleanup (unreachable in this example, but good practice to include)
    closesocket(serverSocket);
    WSACleanup();

    return 0;

}

void registerClient(const std::string& username, const std::string& ip, const std::vector<std::string>& resources) {
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        userDirectory[username] = { ip, true };
        for (const auto& resource : resources) {
            resourceDirectory[resource] = username;
        }
    }
    std::cout << "Registered user: " << username << " with resources.\n";
}
void sendHelloMessages() {
    while (true) {
        std::lock_guard<std::mutex> lock(clientsMutex);
        for (const auto& [username, userInfo] : userDirectory) {
            const std::string& ip = userInfo.first;
            sockaddr_in clientAddr;
            clientAddr.sin_family = AF_INET;
            clientAddr.sin_port = htons(UDP_PORT);
            inet_pton(AF_INET, ip.c_str(), &clientAddr.sin_addr);

            std::string helloMessage = "HELLO " + username;
            sendto(udpSocket, helloMessage.c_str(), helloMessage.size(), 0, (sockaddr*)&clientAddr, sizeof(clientAddr));
        }
        std::this_thread::sleep_for(std::chrono::seconds(10)); // Adjust the interval as needed
    }
}

void sendResourceList(SOCKET clientSocket) {
    std::string resourceList = "Available resources:\n";
    for (const auto& [resource, owner] : resourceDirectory) {
        resourceList += resource + " - owned by " + owner + "\n";
    }
    send(clientSocket, resourceList.c_str(), resourceList.size(), 0);
}

void handleClient(SOCKET clientSocket) {
    char buffer[BUFFER_SIZE];
    int bytesReceived;
    int buff = 1024;
    while ((bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE, 0)) > 0) {
        buffer[bytesReceived] = '\0';
        std::string command(buffer);

                 std::string action = command.substr(0, command.find(' '));
                 std::string filename = command.substr(command.find(' ') + 1);
            if (action == "REGISTER") {
                std::string username = filename.substr(0, filename.find(' '));
                std::string resourceList = filename.substr(filename.find(' ') + 1);
                std::vector<std::string> resources;
                std::istringstream iss(resourceList);
                std::string resource;
            while (getline(iss, resource, ',')) {
                resources.push_back(resource);
                }
                sockaddr_in clientAddr;
                registerClient(username, inet_ntoa(clientAddr.sin_addr), resources);
            } else if (action == "list_resources") {
                sendResourceList(clientSocket);
           
           } else if (action[0] == '%') {
                     action = action.substr(1); // Remove the '%' character
                     std::cout << action << " && " << filename << std::endl;

                if (action == "put") {
                    std::cout << "Receiving file: " << filename << "\n";
                    receiveFile(clientSocket, filename);
                } else if (action == "get") {
                    std::cout << "Sending file: " << filename << "\n";
                    SendFile(clientSocket, filename, buff);
                } else {
                    std::cerr << "Unknown command: " << command << "\n";
                }
            }
            else {
               std::cout << "Broadcasting message: " << command << "\n";
               broadcastMessage(command, clientSocket);
            }
    }
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        auto it = std::remove(udpClients.begin(), udpClients.end(), clientSocket);
        udpClients.erase(it, udpClients.end());
    }
    // Close the client socket
    closesocket(clientSocket);
    std::cout << "Client disconnected.\n";
}


void broadcastMessage(const std::string& message, SOCKET senderSocket)
{
    {
        std::lock_guard<std::mutex> lock(clientsMutex);

        for (SOCKET client : udpClients)
        {
            send(client, message.c_str(), message.length(), 0);
        }
    }
}



//function to recieve file from client
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

//function to recieve file size for buffer
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

//recieve buffer data of file for retrieval
int RecvBuffer(SOCKET s, char* buffer, int bufferSize, int chunkSize = 4 * 1024) {
    int i = 0;
    while (i < bufferSize) {
        const int l = recv(s, &buffer[i], __min(chunkSize, bufferSize - i), 0);
        if (l < 0) { return l; } // this is an error
        i += l;
    }
    return i;
}

//sends buffer to confirm file data
int SendBuffer(SOCKET s, const char* buffer, int bufferSize, int chunkSize = 4 * 1024) {

    int i = 0;
    while (i < bufferSize) {
        const int l = send(s, &buffer[i], __min(chunkSize, bufferSize - i), 0);
        if (l < 0) { return l; } // this is an error
        i += l;
    }
    return i;
}

//function to send file data to client
int64_t SendFile(SOCKET s, const std::string& fileName, int chunkSize = 64 * 1024) {

    const int64_t fileSize = GetFileSize(fileName);
    if (fileSize < 0) { return -1; }

    std::ifstream file(fileName, std::ifstream::binary);
    if (file.fail()) { return -1; }

    if (SendBuffer(s, reinterpret_cast<const char*>(&fileSize),
        sizeof(fileSize)) != sizeof(fileSize)) {
        return -2;
    }

    char* buffer = new char[chunkSize];
    bool errored = false;
    int64_t i = fileSize;
    while (i != 0) {
        const int64_t ssize = __min(i, (int64_t)chunkSize);
        if (!file.read(buffer, ssize)) { errored = true; break; }
        const int l = SendBuffer(s, buffer, (int)ssize);
        if (l < 0) { errored = true; break; }
        i -= l;
    }
    delete[] buffer;

    file.close();

    return errored ? -3 : fileSize;
}


