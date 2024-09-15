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
int64_t SendFile(SOCKET s, const std::string& fileName, int chunkSize);
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
    int buff = 1024;
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
            SendFile(clientSocket, filename, buff);
        } else {
            std::cerr << "Unknown command: " << command << "\n";
        }
    }

    // Close the client socket
    closesocket(clientSocket);
    std::cout << "Client disconnected.\n";
}

/*void sendFile(SOCKET clientSocket, const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Could not open file: " << filename << "\n";
        return;
    }

    char buffer[BUFFER_SIZE];
    while (!file.eof()) {
        file.read(buffer, BUFFER_SIZE);
        int bytesRead = file.gcount();
        unsigned int bytesSent = 0;
        int bytesToSend = 0;

        while (bytesSent < BUFFER_SIZE)
        {
                bytesToSend = BUFFER_SIZE - bytesSent;
            send(clientSocket, buffer + bytesSent, bytesToSend, 0);
            bytesSent += bytesToSend;
        }
        
    }

    file.close();
    std::cout << "File " << filename << " sent to client.\n";
}*/

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






int64_t GetFileSize(const std::string& fileName) {
    // no idea how to get filesizes > 2.1 GB in a C++ kind-of way.
    // I will cheat and use Microsoft's C-style file API
    FILE* f;
    if (fopen_s(&f, fileName.c_str(), "rb") != 0) {
        return -1;
    }
    _fseeki64(f, 0, SEEK_END);
    const int64_t len = _ftelli64(f);
    fclose(f);
    return len;
}

///
/// Recieves data in to buffer until bufferSize value is met
///
int RecvBuffer(SOCKET s, char* buffer, int bufferSize, int chunkSize = 4 * 1024) {
    int i = 0;
    while (i < bufferSize) {
        const int l = recv(s, &buffer[i], __min(chunkSize, bufferSize - i), 0);
        if (l < 0) { return l; } // this is an error
        i += l;
    }
    return i;
}

///
/// Sends data in buffer until bufferSize value is met
///
int SendBuffer(SOCKET s, const char* buffer, int bufferSize, int chunkSize = 4 * 1024) {

    int i = 0;
    while (i < bufferSize) {
        const int l = send(s, &buffer[i], __min(chunkSize, bufferSize - i), 0);
        if (l < 0) { return l; } // this is an error
        i += l;
    }
    return i;
}

//
// Sends a file
// returns size of file if success
// returns -1 if file couldn't be opened for input
// returns -2 if couldn't send file length properly
// returns -3 if file couldn't be sent properly
//
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

//
// Receives a file
// returns size of file if success
// returns -1 if file couldn't be opened for output
// returns -2 if couldn't receive file length properly
// returns -3 if couldn't receive file properly
//
int64_t RecvFile(SOCKET s, const std::string& fileName, int chunkSize = 64 * 1024) {
    std::ofstream file(fileName, std::ofstream::binary);
    if (file.fail()) { return -1; }

    int64_t fileSize;
    if (RecvBuffer(s, reinterpret_cast<char*>(&fileSize),
        sizeof(fileSize)) != sizeof(fileSize)) {
        return -2;
    }

    char* buffer = new char[chunkSize];
    bool errored = false;
    int64_t i = fileSize;
    while (i != 0) {
        const int r = RecvBuffer(s, buffer, (int)__min(i, (int64_t)chunkSize));
        if ((r < 0) || !file.write(buffer, r)) { errored = true; break; }
        i -= r;
    }
    delete[] buffer;

    file.close();

    return errored ? -3 : fileSize;
}